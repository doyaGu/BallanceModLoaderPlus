#include "ScriptNativeBuffer.h"

#include <cassert>
#include <cstring>
#include <cstdio>

NativeBuffer::NativeBuffer(size_t size) {
    m_Buffer = static_cast<char *>(asAllocMem(size));
    m_Size = size;
    m_CursorPos = 0;
    m_Owned = true;
}

NativeBuffer::NativeBuffer(void *buffer, size_t size): m_Buffer(static_cast<char *>(buffer)), m_Size(size), m_CursorPos(0), m_Owned(false) {}

NativeBuffer::~NativeBuffer() {
    if (m_Owned && m_Buffer) {
        asFreeMem(m_Buffer);
        m_Buffer = nullptr;
    }

    m_Size = 0;
    m_CursorPos = 0;

    if(m_WeakRefFlag) {
        // Tell the ones that hold weak references that the object is destroyed
        m_WeakRefFlag->Set(true);
        m_WeakRefFlag->Release();
    }
}

int NativeBuffer::AddRef() const {
    return m_RefCount.AddRef();
}

int NativeBuffer::Release() const {
    const int r = m_RefCount.Release();
    if (r == 0) {
        std::atomic_thread_fence(std::memory_order_acquire);
        this->~NativeBuffer();
        asFreeMem(const_cast<NativeBuffer *>(this));
    }
    return r;
}

asILockableSharedBool *NativeBuffer::GetWeakRefFlag() {
    if(!m_WeakRefFlag)
        m_WeakRefFlag = asCreateLockableSharedBool();
    return m_WeakRefFlag;
}

char &NativeBuffer::operator[](size_t index) {
    static char dummy = 0;
    if (index >= m_Size) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Index out of bounds");
        return dummy;
    }
    return m_Buffer[index];
}

const char &NativeBuffer::operator[](size_t index) const {
    static constexpr char dummy = 0;
    if (index >= m_Size) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Index out of bounds");
        return dummy;
    }
    return m_Buffer[index];
}

size_t NativeBuffer::Write(void *x, size_t size) {
    if (!m_Buffer || m_CursorPos + size > m_Size || !x)
        return 0;
    memcpy(&m_Buffer[m_CursorPos], x, size);
    m_CursorPos += size;
    return size;
}

size_t NativeBuffer::Read(void *x, size_t size) {
    if (!m_Buffer || m_CursorPos + size > m_Size || !x)
        return 0;
    memcpy(x, &m_Buffer[m_CursorPos], size);
    m_CursorPos += size;
    return size;
}

size_t NativeBuffer::WriteString(const char *str) {
    if (!str)
        return 0;
    const size_t len = strlen(str) + 1;
    return Write((void *)str, static_cast<int>(len));
}

size_t NativeBuffer::WriteString(const std::string &str) {
    return WriteString(str.c_str());
}

size_t NativeBuffer::ReadString(char *outStr, size_t maxSize) {
    if (!m_Buffer || m_CursorPos >= m_Size)
        return 0;

    size_t i = 0;
    for (; i < maxSize - 1 && m_CursorPos < m_Size; ++i) {
        outStr[i] = m_Buffer[m_CursorPos++];
        if (outStr[i] == '\0')
            break;
    }

    outStr[i] = '\0';
    m_CursorPos += i + 1;
    return i + 1;
}

size_t NativeBuffer::ReadString(std::string &str) {
    if (!m_Buffer || m_CursorPos >= m_Size)
        return 0;

    size_t count = 0;
    str.clear();
    while (m_CursorPos < m_Size) {
        const char c = m_Buffer[m_CursorPos++];
        if (c == '\0')
            break;
        ++count;
        str.push_back(c);
    }

    m_CursorPos += count + 1;
    return count;
}

bool NativeBuffer::Fill(int value, size_t size) {
    if (!m_Buffer || m_CursorPos + size >= m_Size)
        return false;

    memset(&m_Buffer[m_CursorPos], value, size);
    m_CursorPos += size;
    return true;
}

bool NativeBuffer::Seek(size_t pos) {
    if (pos >= m_Size)
        return false;
    m_CursorPos = pos;
    return true;
}

bool NativeBuffer::Skip(size_t offset) {
    return Seek(m_CursorPos + offset);
}

int NativeBuffer::Compare(const NativeBuffer &other, size_t size) const {
    if (!m_Buffer || m_CursorPos + size > m_Size)
        return -1;
    if (other.CursorPos() + size > other.Size())
        return 1;
    return memcmp(&m_Buffer[m_CursorPos], other.Cursor(), size);
}

size_t NativeBuffer::Merge(const NativeBuffer &other, bool truncate) {
    int size = other.Size() - other.CursorPos();
    if (size == 0 || !m_Buffer)
        return 0;

    if (truncate) {
        if (m_CursorPos + size > m_Size)
            size = m_Size - m_CursorPos;
    } else if (m_CursorPos + size > m_Size) {
        return 0;
    }

    memcpy(&m_Buffer[m_CursorPos], other.Cursor(), size);
    m_CursorPos += size;
    return size;
}

NativeBuffer *NativeBuffer::Extract(size_t size) {
    if (!m_Buffer || m_CursorPos + size > m_Size)
        return nullptr;

    auto *buffer = Create(&m_Buffer[m_CursorPos], size);
    m_CursorPos += size;
    return buffer;
}

NativePointer NativeBuffer::ToPointer() const {
    return NativePointer(&m_Buffer[m_CursorPos]);
}

size_t NativeBuffer::Load(const char *filename, size_t size, int offset) {
    if (!m_Buffer || m_CursorPos + size > m_Size)
        return 0;

    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return 0;

    if (offset > 0)
        fseek(fp, offset, SEEK_SET);
    size = fread(&m_Buffer[m_CursorPos], sizeof(char), size, fp);
    fclose(fp);
    m_CursorPos += size;
    return size;
}

size_t NativeBuffer::Save(const char *filename, size_t size) {
    if (!m_Buffer || m_CursorPos + size > m_Size)
        return 0;

    FILE *fp = fopen(filename, "wb");
    if (!fp)
        return 0;

    size = fwrite(&m_Buffer[m_CursorPos], sizeof(char), size, fp);
    fclose(fp);
    m_CursorPos += size;
    return size;
}

static void NativeBufferWriteGeneric(asIScriptGeneric *gen) {
    asIScriptEngine *engine = gen->GetEngine();
    const int typeId = gen->GetArgTypeId(0);
    void *addr = *static_cast<void **>(gen->GetAddressOfArg(0));;
    auto *self = static_cast<NativeBuffer *>(gen->GetObject());
    size_t size = 0;

    if (typeId & asTYPEID_SCRIPTOBJECT) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Cannot write script objects to buffer");
        gen->SetReturnDWord(0);
        return;
    }

    if (typeId & asTYPEID_APPOBJECT) {
        if (typeId & asTYPEID_OBJHANDLE) {
            asIScriptContext *ctx = asGetActiveContext();
            ctx->SetException("Cannot write object handle to buffer");
            gen->SetReturnDWord(0);
            return;
        }

        asITypeInfo *type = engine->GetTypeInfoById(typeId);
        if (!type) {
            gen->SetReturnDWord(0);
            return;
        }

        if (strcmp(type->GetName(), "string") == 0) {
            std::string &str = *static_cast<std::string *>(addr);
            size = self->WriteString(str);
        } else {
            size = engine->GetSizeOfPrimitiveType(typeId);
            if (size == 0) {
                if (type->GetFlags() & asOBJ_POD) {
                    size = type->GetSize();
                } else {
                    asIScriptContext *ctx = asGetActiveContext();
                    ctx->SetException("Cannot write non-POD object to buffer");
                    gen->SetReturnDWord(0);
                    return;
                }
            }

            if (size != 0 && self->CursorPos() + size <= self->Size()) {
                size = self->Write(addr, size);
            } else {
                gen->SetReturnDWord(0);
                return;
            }
        }
    } else {
        size = engine->GetSizeOfPrimitiveType(typeId);
        if (size != 0 && self->CursorPos() + size <= self->Size()) {
            size = self->Write(addr, size);
        }
    }

    gen->SetReturnDWord(size);
}

static void NativeBufferReadGeneric(asIScriptGeneric *gen) {
    asIScriptEngine *engine = gen->GetEngine();
    const int typeId = gen->GetArgTypeId(0);
    void *addr = *static_cast<void **>(gen->GetAddressOfArg(0));;
    auto *self = static_cast<NativeBuffer *>(gen->GetObject());
    size_t size = 0;

    if (typeId & asTYPEID_SCRIPTOBJECT) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException("Cannot read script objects from buffer");
        gen->SetReturnDWord(0);
        return;
    }

    if (typeId & asTYPEID_APPOBJECT) {
        if (typeId & asTYPEID_OBJHANDLE) {
            asIScriptContext *ctx = asGetActiveContext();
            ctx->SetException("Cannot read object handle from buffer");
            gen->SetReturnDWord(0);
            return;
        }

        asITypeInfo *type = engine->GetTypeInfoById(typeId);
        if (!type) {
            gen->SetReturnDWord(0);
            return;
        }

        if (strcmp(type->GetName(), "string") == 0) {
            std::string &str = *static_cast<std::string *>(addr);
            size = self->ReadString(str);
        } else {
            size = engine->GetSizeOfPrimitiveType(typeId);
            if (size == 0) {
                if (type->GetFlags() & asOBJ_POD) {
                    size = type->GetSize();
                } else {
                    asIScriptContext *ctx = asGetActiveContext();
                    ctx->SetException("Cannot read non-POD object from buffer");
                    gen->SetReturnDWord(0);
                    return;
                }
            }

            if (size != 0 && self->CursorPos() + size <= self->Size()) {
                size = self->Read(addr, size);
            } else {
                gen->SetReturnDWord(0);
                return;
            }
        }
    } else {
        size = engine->GetSizeOfPrimitiveType(typeId);
        if (size != 0 && self->CursorPos() + size <= self->Size()) {
            size = self->Read(addr, size);
        }
    }

    gen->SetReturnDWord(size);
}

void RegisterNativeBuffer(asIScriptEngine *engine) {
    int r = 0;

    r = engine->RegisterObjectType("NativeBuffer", 0, asOBJ_REF); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("NativeBuffer", asBEHAVE_FACTORY, "NativeBuffer@ f(size_t size)", asFUNCTIONPR(NativeBuffer::Create, (size_t), NativeBuffer *), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("NativeBuffer", asBEHAVE_FACTORY, "NativeBuffer@ f(uintptr_t buf, size_t size)", asFUNCTIONPR(NativeBuffer::Create, (void *, size_t), NativeBuffer *), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("NativeBuffer", asBEHAVE_FACTORY, "NativeBuffer@ f(NativePointer ptr, size_t size)", asFUNCTIONPR([](NativePointer ptr, size_t size) { return NativeBuffer::Create(ptr.Get(), size); }, (NativePointer, size_t), NativeBuffer *), asCALL_CDECL); assert(r >= 0);

    r = engine->RegisterObjectBehaviour("NativeBuffer", asBEHAVE_ADDREF, "void f()", asMETHOD(NativeBuffer, AddRef), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("NativeBuffer", asBEHAVE_RELEASE, "void f()", asMETHOD(NativeBuffer, Release), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("NativeBuffer", asBEHAVE_GET_WEAKREF_FLAG, "int &f()", asMETHOD(NativeBuffer, GetWeakRefFlag), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("NativeBuffer", "uint8 &opIndex(uint index)", asMETHODPR(NativeBuffer, operator[], (size_t), char &), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "const uint8 &opIndex(uint index) const", asMETHODPR(NativeBuffer, operator[], (size_t) const, const char &), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("NativeBuffer", "size_t Write(?&in)", asFUNCTION(NativeBufferWriteGeneric), asCALL_GENERIC); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t Read(?&out)", asFUNCTION(NativeBufferReadGeneric), asCALL_GENERIC); assert(r >= 0);

    r = engine->RegisterObjectMethod("NativeBuffer", "size_t WriteInt(int value)", asMETHODPR(NativeBuffer, WriteInt, (int), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t WriteUInt(uint value)", asMETHODPR(NativeBuffer, WriteUInt, (unsigned int), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t WriteFloat(float value)", asMETHODPR(NativeBuffer, WriteFloat, (float), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t WriteDouble(double value)", asMETHODPR(NativeBuffer, WriteDouble, (double), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t WriteShort(int16 value)", asMETHODPR(NativeBuffer, WriteShort, (short), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t WriteChar(int8 value)", asMETHODPR(NativeBuffer, WriteChar, (char), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t WriteUChar(uint8 value)", asMETHODPR(NativeBuffer, WriteUChar, (unsigned char), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t WriteLong(int64 value)", asMETHODPR(NativeBuffer, WriteLong, (long), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t WriteULong(uint64 value)", asMETHODPR(NativeBuffer, WriteULong, (unsigned long), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t WriteString(const string &in value)", asMETHODPR(NativeBuffer, WriteString, (const std::string &), size_t), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("NativeBuffer", "size_t ReadInt(int &out value)", asMETHODPR(NativeBuffer, ReadInt, (int &), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t ReadUInt(uint &out value)", asMETHODPR(NativeBuffer, ReadUInt, (unsigned int &), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t ReadFloat(float &out value)", asMETHODPR(NativeBuffer, ReadFloat, (float &), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t ReadDouble(double &out value)", asMETHODPR(NativeBuffer, ReadDouble, (double &), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t ReadShort(int16 &out value)", asMETHODPR(NativeBuffer, ReadShort, (short &), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t ReadChar(int8 &out value)", asMETHODPR(NativeBuffer, ReadChar, (char &), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t ReadUChar(uint8 &out value)", asMETHODPR(NativeBuffer, ReadUChar, (unsigned char &), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t ReadLong(int64 &out value)", asMETHODPR(NativeBuffer, ReadLong, (long &), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t ReadULong(uint64 &out value)", asMETHODPR(NativeBuffer, ReadULong, (unsigned long &), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t ReadString(string &out value)", asMETHODPR(NativeBuffer, ReadString, (std::string &), size_t), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("NativeBuffer", "bool Fill(int value, size_t size)", asMETHODPR(NativeBuffer, Fill, (int, size_t), bool), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("NativeBuffer", "bool Seek(size_t pos)", asMETHODPR(NativeBuffer, Seek, (size_t), bool), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "bool Skip(size_t offset)", asMETHODPR(NativeBuffer, Skip, (size_t), bool), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "void Reset()", asMETHODPR(NativeBuffer, Reset, (), void), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("NativeBuffer", "size_t Size() const", asMETHODPR(NativeBuffer, Size, () const, size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t CursorPos() const", asMETHODPR(NativeBuffer, CursorPos, () const, size_t), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("NativeBuffer", "bool IsValid() const", asMETHODPR(NativeBuffer, IsValid, () const, bool), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "bool IsEmpty() const", asMETHODPR(NativeBuffer, IsEmpty, () const, bool), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("NativeBuffer", "int Compare(const NativeBuffer &in other, size_t size) const", asMETHODPR(NativeBuffer, Compare, (const NativeBuffer &, size_t) const, int), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t Merge(const NativeBuffer &in other, bool truncate = false)", asMETHODPR(NativeBuffer, Merge, (const NativeBuffer &, bool), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "NativeBuffer@ Extract(size_t size)", asMETHODPR(NativeBuffer, Extract, (size_t), NativeBuffer *), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "NativePointer ToPointer() const", asMETHODPR(NativeBuffer, ToPointer, () const, NativePointer), asCALL_THISCALL); assert(r >= 0);

    r = engine->RegisterObjectMethod("NativeBuffer", "size_t Load(const string &in filename, size_t size, int offset = 0)", asMETHODPR(NativeBuffer, Load, (const char *, size_t, int), size_t), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("NativeBuffer", "size_t Save(const string &in filename, size_t size)", asMETHODPR(NativeBuffer, Save, (const char *, size_t), size_t), asCALL_THISCALL); assert(r >= 0);
}
