#include "IVP/Runtime.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "CKAll.h"
#include "CryptoUtils.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

namespace BML::IVP {
namespace {

#include "IVP/generated/IvpRetailImage.inc"

struct IvpSymbolRecord {
    const char *Name;
    uint32_t Rva;
    uint32_t Flags;
};

constexpr IvpSymbolRecord kIvpSymbols[] = {
#include "IVP/generated/IvpSymbols.inc"
};

constexpr uint32_t kIvpSymbolCount =
    static_cast<uint32_t>(sizeof(kIvpSymbols) / sizeof(kIvpSymbols[0]));

consteval bool IvpSymbolsAreSorted() {
    for (size_t i = 1; i < kIvpSymbolCount; ++i) {
        const char *left = kIvpSymbols[i - 1].Name;
        const char *right = kIvpSymbols[i].Name;
        while (*left && *left == *right) {
            ++left;
            ++right;
        }
        if (static_cast<unsigned char>(*left) >=
            static_cast<unsigned char>(*right))
            return false;
    }
    return true;
}

static_assert(kIvpSymbolCount >= 800);
static_assert(IvpSymbolsAreSorted());

template <typename T>
T &At(void *base, size_t offset) noexcept {
    return *reinterpret_cast<T *>(static_cast<unsigned char *>(base) + offset);
}

bool Matches(const unsigned char *image, DWORD imageSize,
             const RetailInstructionAnchor &anchor) noexcept {
    return anchor.Rva <= imageSize && anchor.Size <= imageSize - anchor.Rva &&
           std::memcmp(image + anchor.Rva, anchor.Bytes.data(), anchor.Size) == 0;
}

unsigned char HexDigit(char value) noexcept {
    if (value >= '0' && value <= '9')
        return static_cast<unsigned char>(value - '0');
    if (value >= 'A' && value <= 'F')
        return static_cast<unsigned char>(value - 'A' + 10);
    if (value >= 'a' && value <= 'f')
        return static_cast<unsigned char>(value - 'a' + 10);
    return 0xff;
}

bool MatchesRetailFile(HMODULE module) noexcept {
    try {
        // GetModuleFileNameW can report any path accepted by the Windows
        // loader, including an extended-length path. Hash the file that
        // supplied this module before any retail RVA becomes callable.
        std::array<wchar_t, 32768> path{};
        const DWORD length = GetModuleFileNameW(
            module, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size())
            return false;

        std::array<std::uint8_t, 32> digest{};
        if (!utils::Sha256File(std::wstring(path.data(), length), digest.data()))
            return false;

        static_assert(sizeof(kImageSha256) == digest.size() * 2 + 1);
        for (size_t i = 0; i < digest.size(); ++i) {
            const unsigned char high = HexDigit(kImageSha256[i * 2]);
            const unsigned char low = HexDigit(kImageSha256[i * 2 + 1]);
            if (high > 0x0f || low > 0x0f ||
                digest[i] != static_cast<unsigned char>((high << 4) | low))
                return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

using GetPhysicsObjectFn = void *(__thiscall *)(void *, CK3dEntity *, int);

} // namespace

struct Runtime::Storage {
    struct NativeObject {
        void *Manager = nullptr;
        void *Environment = nullptr;
        void *PhysicsObject = nullptr;
        void *RealObject = nullptr;
        void *Core = nullptr;
        void *Material = nullptr;
    };

    explicit Storage(CKContext *context) noexcept : Context(context) {}

    int EnsureBinding() noexcept {
        HMODULE module = GetModuleHandleA("physics_RT.dll");
        if (!module)
            return BML_ERROR_UNAVAILABLE;
        if (module == Module)
            return BindStatus;

        Module = module;
        BindStatus = BML_ERROR_VERSION_MISMATCH;
        Timestamp = 0;
        ImageSize = 0;

        const auto *image = reinterpret_cast<const unsigned char *>(module);
        const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(image);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
            return BindStatus;
        const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(image + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
            return BindStatus;

        Timestamp = nt->FileHeader.TimeDateStamp;
        ImageSize = nt->OptionalHeader.SizeOfImage;
        if (Timestamp != kImageTimestamp || ImageSize != kImageSize)
            return BindStatus;

        if (!MatchesRetailFile(module))
            return BindStatus;

        // The complete file identity rejects a different retail binary; the
        // independent in-memory anchors additionally reject a patched module.
        for (const RetailInstructionAnchor &anchor : kInstructionAnchors) {
            if (!Matches(image, ImageSize, anchor))
                return BindStatus;
        }

        GetPhysicsObject = reinterpret_cast<GetPhysicsObjectFn>(
            reinterpret_cast<uintptr_t>(module) + kRvaGetPhysicsObject);
        BindStatus = BML_OK;
        return BindStatus;
    }

    void *Manager() const noexcept {
        return Context ? Context->GetManagerByGuid(CKGUID(0x6BED328B, 0x141F5148)) : nullptr;
    }

    CK3dEntity *ValidateEntity(CK3dEntity *candidate) const noexcept {
        if (!Context || !candidate)
            return nullptr;

        // The C boundary accepts an opaque pointer. Establish its identity
        // against CK's live registry before dereferencing caller-controlled
        // memory or entering physics_RT.dll.
        const XObjectPointerArray &entities =
            Context->GetObjectListByType(CKCID_3DENTITY, TRUE);
        for (XObjectPointerArray::ConstIterator it = entities.Begin();
             it != entities.End(); ++it) {
            if (*it != candidate)
                continue;
            CK3dEntity *entity = CK3dEntity::Cast(*it);
            return entity && !entity->IsToBeDeleted() &&
                           CKGetObject(Context, entity->GetID()) == entity
                       ? entity
                       : nullptr;
        }
        return nullptr;
    }

    bool IsExecutableRva(uint32_t rva) const noexcept {
        if (!Module || rva >= ImageSize)
            return false;
        const auto *image = reinterpret_cast<const unsigned char *>(Module);
        const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(image);
        const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(image + dos->e_lfanew);
        const IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
                continue;
            const uint32_t start = section->VirtualAddress;
            const uint32_t span = (std::max)(section->Misc.VirtualSize,
                                             section->SizeOfRawData);
            if (rva >= start && rva - start < span)
                return true;
        }
        return false;
    }

    int Resolve(CK3dEntity *entity, NativeObject &out) noexcept {
        if (!entity)
            return BML_ERROR_INVALID_PARAMETER;
        entity = ValidateEntity(entity);
        if (!entity)
            return BML_ERROR_OBJECT_INVALID;
        const int status = EnsureBinding();
        if (status != BML_OK)
            return status;
        out.Manager = Manager();
        if (!out.Manager)
            return BML_ERROR_UNAVAILABLE;
        out.Environment = At<void *>(out.Manager, kManagerEnvironment);
        if (!out.Environment)
            return BML_ERROR_UNAVAILABLE;
        out.PhysicsObject = GetPhysicsObject(out.Manager, entity, 0);
        if (!out.PhysicsObject)
            return BML_ERROR_NOT_FOUND;
        out.RealObject = At<void *>(out.PhysicsObject, kPhysicsRealObject);
        if (!out.RealObject || At<void *>(out.RealObject, kRealClientData) != entity)
            return BML_ERROR_OBJECT_INVALID;
        out.Core = At<void *>(out.RealObject, kRealCore);
        if (!out.Core)
            return BML_ERROR_UNAVAILABLE;
        out.Material = At<void *>(out.RealObject, kRealMaterial);
        return BML_OK;
    }

    CKContext *Context = nullptr;
    HMODULE Module = nullptr;
    int BindStatus = BML_ERROR_UNAVAILABLE;
    DWORD Timestamp = 0;
    DWORD ImageSize = 0;
    GetPhysicsObjectFn GetPhysicsObject = nullptr;
};

Runtime::Runtime(CKContext *context) noexcept
    : m_Storage(new (std::nothrow) Storage(context)) {}

Runtime::~Runtime() = default;

int Runtime::ReadApiInfo(BML_IvpApiInfo &out) noexcept {
    if (!m_Storage)
        return BML_ERROR_OUT_OF_MEMORY;
    const int status = m_Storage->EnsureBinding();
    if (status != BML_OK)
        return status;
    BML_IvpApiInfo value{};
    value.Capabilities = BML_IVP_CAP_NATIVE_OBJECTS |
                         BML_IVP_CAP_IDA_SYMBOLS |
                         BML_IVP_CAP_EXECUTABLE_RVAS |
                         BML_IVP_CAP_RECONSTRUCTED_TYPES |
                         BML_IVP_CAP_RECONSTRUCTED_CALLS |
                         BML_IVP_CAP_ORIGINAL_DLL_LOCK;
    value.Architecture = BML_IVP_ARCH_X86;
    value.AbiRevision = BML_IVP_ABI_REVISION;
    value.ImageTimestamp = m_Storage->Timestamp;
    value.ImageSize = m_Storage->ImageSize;
    value.FunctionSymbolCount = kIvpSymbolCount;
    std::memcpy(value.ImageSha256, kImageSha256, sizeof(kImageSha256));
    out = value;
    return BML_OK;
}

int Runtime::GetManager(uintptr_t &out) noexcept {
    out = 0;
    if (!m_Storage)
        return BML_ERROR_OUT_OF_MEMORY;
    const int status = m_Storage->EnsureBinding();
    if (status != BML_OK)
        return status;
    void *manager = m_Storage->Manager();
    if (!manager)
        return BML_ERROR_UNAVAILABLE;
    out = reinterpret_cast<uintptr_t>(manager);
    return BML_OK;
}

int Runtime::GetEnvironment(uintptr_t &out) noexcept {
    out = 0;
    uintptr_t manager = 0;
    const int status = GetManager(manager);
    if (status != BML_OK)
        return status;
    void *environment = At<void *>(reinterpret_cast<void *>(manager), kManagerEnvironment);
    if (!environment)
        return BML_ERROR_UNAVAILABLE;
    out = reinterpret_cast<uintptr_t>(environment);
    return BML_OK;
}

#define BML_IVP_OBJECT_GETTER(Method, Field)                                      \
    int Runtime::Method(CK3dEntity *entity, uintptr_t &out) noexcept {            \
        out = 0;                                                                  \
        if (!m_Storage)                                                           \
            return BML_ERROR_OUT_OF_MEMORY;                                       \
        Storage::NativeObject object;                                             \
        const int status = m_Storage->Resolve(entity, object);                    \
        if (status == BML_OK)                                                     \
            out = reinterpret_cast<uintptr_t>(object.Field);                      \
        return status;                                                            \
    }

BML_IVP_OBJECT_GETTER(GetPhysicsObject, PhysicsObject)
BML_IVP_OBJECT_GETTER(GetRealObject, RealObject)
BML_IVP_OBJECT_GETTER(GetCore, Core)

#undef BML_IVP_OBJECT_GETTER

int Runtime::GetMaterial(CK3dEntity *entity, uintptr_t &out) noexcept {
    out = 0;
    if (!m_Storage)
        return BML_ERROR_OUT_OF_MEMORY;
    Storage::NativeObject object;
    const int status = m_Storage->Resolve(entity, object);
    if (status != BML_OK)
        return status;
    if (!object.Material)
        return BML_ERROR_UNAVAILABLE;
    out = reinterpret_cast<uintptr_t>(object.Material);
    return BML_OK;
}

int Runtime::ResolveSymbol(const char *name, uintptr_t &out) noexcept {
    out = 0;
    if (!name || name[0] == '\0')
        return BML_ERROR_INVALID_PARAMETER;
    const auto *end = kIvpSymbols + kIvpSymbolCount;
    const auto *found = std::lower_bound(
        kIvpSymbols, end, name,
        [](const IvpSymbolRecord &symbol, const char *candidate) {
            return std::strcmp(symbol.Name, candidate) < 0;
        });
    if (found == end || std::strcmp(found->Name, name) != 0)
        return BML_ERROR_NOT_FOUND;
    return ResolveRva(found->Rva, out);
}

int Runtime::ResolveRva(uint32_t rva, uintptr_t &out) noexcept {
    out = 0;
    if (!m_Storage)
        return BML_ERROR_OUT_OF_MEMORY;
    const int status = m_Storage->EnsureBinding();
    if (status != BML_OK)
        return status;
    if (!m_Storage->IsExecutableRva(rva))
        return BML_ERROR_INVALID_PARAMETER;
    out = reinterpret_cast<uintptr_t>(m_Storage->Module) + rva;
    return BML_OK;
}

uint32_t Runtime::GetSymbolCount() const noexcept {
    return kIvpSymbolCount;
}

int Runtime::GetSymbol(uint32_t index, BML_IvpSymbol &out) const noexcept {
    if (index >= kIvpSymbolCount)
        return BML_ERROR_NOT_FOUND;
    out.Name = kIvpSymbols[index].Name;
    out.Rva = kIvpSymbols[index].Rva;
    out.Flags = kIvpSymbols[index].Flags;
    return BML_OK;
}

} // namespace BML::IVP
