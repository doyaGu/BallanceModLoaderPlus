#ifndef BML_IVP_TYPES_H
#define BML_IVP_TYPES_H

#include "BML/IVP/Calls.h"

#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <type_traits>

// These names intentionally match the native engine. The declarations are a
// Ballance-specific compatibility reconstruction, not copied SDK headers.
using IVP_FLOAT = float;
using IVP_DOUBLE = double;
using IVP_INT32 = std::int32_t;
using IVP_UINT32 = std::uint32_t;
using IVP_Time_CODE = std::int32_t;
using IVP_HTIME = IVP_FLOAT;
using IVP_ERROR_STRING = const char *;
using ushort = unsigned short;
using uint = unsigned int;
// The supported retail ABI is fixed to 32-bit x86. Original IVP uses this
// spelling for pointer-sized offsets in its public declarations.
using intp = int;

static_assert(sizeof(IVP_FLOAT) == 4, "Retail IVP_FLOAT is binary32");
static_assert(sizeof(IVP_DOUBLE) == 8, "Retail IVP_DOUBLE is binary64");

enum IVP_BOOL : std::int32_t {
    IVP_FALSE = 0,
    IVP_TRUE = 1,
};

enum IVP_RETURN_TYPE : std::int32_t {
    IVP_FAULT = 0,
    IVP_OK = 1,
};

enum IVP_COORDINATE_INDEX : std::int32_t {
    IVP_INDEX_X = 0,
    IVP_INDEX_Y = 1,
    IVP_INDEX_Z = 2,
};

namespace BML::IVP::Detail {

inline std::uint16_t ByteSwap16(std::uint16_t value) {
    return static_cast<std::uint16_t>((value << 8u) | (value >> 8u));
}

inline std::uint32_t ByteSwap32(std::uint32_t value) {
    return ((value & 0x000000FFu) << 24u) |
           ((value & 0x0000FF00u) << 8u) |
           ((value & 0x00FF0000u) >> 8u) |
           ((value & 0xFF000000u) >> 24u);
}

inline void ByteSwapFloat(IVP_FLOAT &value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    bits = ByteSwap32(bits);
    std::memcpy(&value, &bits, sizeof(bits));
}

inline void ByteSwapDouble(IVP_DOUBLE &value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    bits = ((bits & 0x00000000000000FFull) << 56u) |
           ((bits & 0x000000000000FF00ull) << 40u) |
           ((bits & 0x0000000000FF0000ull) << 24u) |
           ((bits & 0x00000000FF000000ull) << 8u) |
           ((bits & 0x000000FF00000000ull) >> 8u) |
           ((bits & 0x0000FF0000000000ull) >> 24u) |
           ((bits & 0x00FF000000000000ull) >> 40u) |
           ((bits & 0xFF00000000000000ull) >> 56u);
    std::memcpy(&value, &bits, sizeof(bits));
}

} // namespace BML::IVP::Detail

// Ballance retains ivp_rand() but links away the two seed accessors. All three
// must operate on the DLL's process-global seed so calls made by the engine and
// by a Mod observe one sequence.
namespace BML::IVP::Detail {

inline int *RetailRandomSeed() noexcept {
    using Random = IVP_FLOAT (__cdecl *)();
    Random random = BML::IVP::ABI::Resolve<Random>(
        BML::IVP::ABI::Address::RandomFloat);
    if (!random)
        return nullptr;
    const std::uintptr_t imageBase =
        reinterpret_cast<std::uintptr_t>(random) -
        static_cast<std::uint32_t>(BML::IVP::ABI::Address::RandomFloat);
    return reinterpret_cast<int *>(
        imageBase + BML::IVP::ABI::RandomSeedRva);
}

} // namespace BML::IVP::Detail

inline IVP_FLOAT ivp_rand() {
    return BML::IVP::ABI::Invoke<IVP_FLOAT>(
        BML::IVP::ABI::Address::RandomFloat);
}

inline void ivp_srand(int seed) {
    int *state = BML::IVP::Detail::RetailRandomSeed();
    if (state)
        *state = seed == 0 ? 1 : seed;
}

inline int ivp_srand_read() {
    const int *state = BML::IVP::Detail::RetailRandomSeed();
    return state ? *state : 1;
}

// These free functions are part of the nearby public IVP surface and retain
// exact bodies in Ballance. Keep allocation and release operations on the
// retail MSVCRT side of the module boundary.
inline void * __cdecl p_malloc(unsigned int size) {
    return BML::IVP::ABI::Invoke<void *>(
        BML::IVP::ABI::Address::Allocate, size);
}

inline char * __cdecl p_calloc(int count, int size) {
    return BML::IVP::ABI::Invoke<char *>(
        BML::IVP::ABI::Address::AllocateZeroed, count, size);
}

inline void * __cdecl ivp_malloc_aligned(int size, int alignment) {
    return BML::IVP::ABI::Invoke<void *>(
        BML::IVP::ABI::Address::AllocateAligned, size, alignment);
}

inline void __cdecl ivp_free_aligned(void *memory) {
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, memory);
}

inline char * __cdecl p_strdup(const char *source) {
    return BML::IVP::ABI::Invoke<char *>(
        BML::IVP::ABI::Address::DuplicateString, source);
}

inline int __cdecl p_strlen(const char *source) {
    return BML::IVP::ABI::Invoke<int>(
        BML::IVP::ABI::Address::StringLength, source);
}

inline int __cdecl p_strcmp(const char *left, const char *right) {
    return BML::IVP::ABI::Invoke<int>(
        BML::IVP::ABI::Address::StringCompare, left, right);
}

// Ballance links these small public utilities away. Their reconstruction is
// independent of IVP object layout. Heap-producing functions still finish in
// p_malloc/p_strdup so ownership remains on the retail MSVCRT side.
inline void __cdecl ivp_byte_swap4(uint &value) {
    value = BML::IVP::Detail::ByteSwap32(value);
}

inline void __cdecl ivp_byte_swap2(ushort &value) {
    value = BML::IVP::Detail::ByteSwap16(value);
}

inline void __cdecl ivp_memory_check(void *) {}

inline void __cdecl p_free(void *memory) {
    // The stripped nearby wrapper was exactly free(memory). Ballance imports
    // MSVCRT's scalar delete entry from that same heap; never call the Mod's
    // UCRT free on memory returned by p_malloc.
    BML::IVP::ABI::RetailOperatorDelete(memory);
}

inline void * __cdecl ivp_calloc_aligned(int size, int alignment) {
    if (size <= 0)
        return nullptr;
    void *memory = ivp_malloc_aligned(size, alignment);
    if (memory)
        std::memset(memory, 0, static_cast<std::size_t>(size));
    return memory;
}

#ifndef IVP_WHITESPACE
#define IVP_WHITESPACE " \t,;\n"
#endif

inline char * __cdecl p_str_tok(char *source, const char *delimiters) {
    if (!delimiters)
        return nullptr;
    char *token = std::strtok(source, delimiters);
    if (!token)
        return nullptr;
    for (char *cursor = token; *cursor; ++cursor) {
        if (*cursor == '\r') {
            *cursor = '\0';
            break;
        }
    }
    return token;
}

inline int __cdecl p_atoi(const char *source) {
    return source ? std::atoi(source) : 0;
}

inline IVP_DOUBLE __cdecl p_atof(const char *source) {
    return source ? std::atof(source) : 0.0;
}

inline char * __cdecl p_read_first_token(FILE *stream) {
    if (!stream)
        return nullptr;
    static char buffer[1024];
    while (std::fgets(buffer, 1000, stream)) {
        if (buffer[0] == '#')
            continue;
        char *token = p_str_tok(buffer, IVP_WHITESPACE);
        if (token)
            return token;
    }
    return nullptr;
}

inline char * __cdecl p_get_next_token() {
    return p_str_tok(nullptr, IVP_WHITESPACE);
}

inline char * __cdecl p_get_string() {
    return p_strdup(p_str_tok(nullptr, "\n"));
}

inline int __cdecl p_get_num() {
    return p_atoi(p_get_next_token());
}

inline IVP_DOUBLE __cdecl p_get_float() {
    return p_atof(p_get_next_token());
}

namespace BML::IVP::Detail {

inline constexpr std::size_t PublicStringBufferSize = 10000;
inline char *PublicErrorBuffer = nullptr;

inline void FormatPublicString(
    char (&buffer)[PublicStringBufferSize], const char *format,
    std::va_list arguments) {
    buffer[0] = '\0';
    if (!format)
        return;
    std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    buffer[sizeof(buffer) - 1] = '\0';
}

inline char *DuplicateFormattedString(
    const char *format, std::va_list arguments) {
    if (!format)
        return nullptr;
    char buffer[PublicStringBufferSize];
    FormatPublicString(buffer, format, arguments);
    return p_strdup(buffer);
}

inline IVP_ERROR_STRING StorePublicError(
    const char *format, std::va_list arguments) {
    if (!format)
        return PublicErrorBuffer;
    char message[PublicStringBufferSize];
    FormatPublicString(message, format, arguments);
    char buffer[PublicStringBufferSize];
    std::snprintf(buffer, sizeof(buffer), "ERROR: %s", message);
    buffer[sizeof(buffer) - 1] = '\0';
    char *replacement = p_strdup(buffer);
    if (!replacement)
        return PublicErrorBuffer;
    p_free(PublicErrorBuffer);
    PublicErrorBuffer = replacement;
    return PublicErrorBuffer;
}

} // namespace BML::IVP::Detail

inline char * __cdecl p_make_string(const char *format, ...) {
    std::va_list arguments;
    va_start(arguments, format);
    char *result = BML::IVP::Detail::DuplicateFormattedString(
        format, arguments);
    va_end(arguments);
    return result;
}

inline char * __cdecl p_make_string_fast(const char *format, ...) {
    std::va_list arguments;
    va_start(arguments, format);
    char *result = BML::IVP::Detail::DuplicateFormattedString(
        format, arguments);
    va_end(arguments);
    return result;
}

inline IVP_ERROR_STRING __cdecl p_export_error(const char *format, ...) {
    std::va_list arguments;
    va_start(arguments, format);
    IVP_ERROR_STRING result = BML::IVP::Detail::StorePublicError(
        format, arguments);
    va_end(arguments);
    return result;
}

inline IVP_ERROR_STRING __cdecl p_get_error() {
    return BML::IVP::Detail::PublicErrorBuffer;
}

inline void __cdecl p_print_error() {
    if (const char *message = p_get_error()) {
        std::fputs(message, stderr);
        std::fputc('\n', stderr);
    }
}

inline void __cdecl ivp_message(const char *format, ...) {
    if (!format)
        return;
    char buffer[BML::IVP::Detail::PublicStringBufferSize];
    std::va_list arguments;
    va_start(arguments, format);
    BML::IVP::Detail::FormatPublicString(buffer, format, arguments);
    va_end(arguments);
    std::fputs(buffer, stderr);
}

inline long __cdecl p_get_time() {
    return static_cast<long>(std::time(nullptr));
}

inline int __cdecl strcasecmp(const char *left, const char *right) {
    if (!left)
        return right ? -1 : 0;
    if (!right)
        return 1;
    for (;; ++left, ++right) {
        const unsigned char a = static_cast<unsigned char>(*left);
        const unsigned char b = static_cast<unsigned char>(*right);
        const unsigned char foldedA =
            a >= 'a' && a <= 'z' ? static_cast<unsigned char>(a - 'a' + 'A') : a;
        const unsigned char foldedB =
            b >= 'a' && b <= 'z' ? static_cast<unsigned char>(b - 'a' + 'A') : b;
        if (foldedA != foldedB)
            return foldedA < foldedB ? -1 : 1;
        if (a == 0)
            return 0;
    }
}

template <class T>
class P_List {
public:
    T *first;
    int len;

    P_List() : first(nullptr), len(0) {}

    void insert(T *element) {
        element->next = first;
        if (first)
            first->prev = element;
        element->prev = nullptr;
        first = element;
        ++len;
    }

    void remove(T *element) {
        T *neighbor = element->prev;
        if (neighbor)
            neighbor->next = element->next;
        else
            first = element->next;
        neighbor = element->next;
        if (neighbor)
            neighbor->prev = element->prev;
        element->next = reinterpret_cast<T *>(
            static_cast<std::uintptr_t>(~std::uintptr_t{0}));
        --len;
    }
};

class P_String {
public:
    static const char *find_string(
        const char *text, const char *key, int mode) {
        if (!text || !key)
            return nullptr;
        const bool foldCase = mode == 1 || mode > 2;
        const bool wildcard = mode >= 2;
        for (const char *start = text;; ++start) {
            const char *candidate = start;
            const char *pattern = key;
            while (*pattern) {
                if (!*candidate)
                    break;
                unsigned char left = static_cast<unsigned char>(*candidate);
                unsigned char right = static_cast<unsigned char>(*pattern);
                if (foldCase) {
                    if (left >= 'a' && left <= 'z')
                        left = static_cast<unsigned char>(left - 'a' + 'A');
                    if (right >= 'a' && right <= 'z')
                        right = static_cast<unsigned char>(right - 'a' + 'A');
                }
                if (!(wildcard && *pattern == '?') && left != right)
                    break;
                ++candidate;
                ++pattern;
            }
            if (!*pattern)
                return start;
            if (!*start)
                return nullptr;
        }
    }

    static void uppercase(char *text) {
        if (!text)
            return;
        for (; *text; ++text) {
            if (*text >= 'a' && *text <= 'z')
                *text = static_cast<char>(*text - 'a' + 'A');
        }
    }

    static int string_cmp(
        const char *text, const char *pattern, IVP_BOOL upperCase) {
        if (!text)
            return -1;
        if (!pattern)
            return 1;
        const char *left = text;
        const char *right = pattern;
        for (;;) {
            char a = *left;
            char b = *right;
            if (b == '*') {
                if (!right[1])
                    return 0;
                const char *segment = ++right;
                const char *end = segment;
                while (*end && *end != '*')
                    ++end;
                const std::size_t length =
                    static_cast<std::size_t>(end - segment);
                if (!*end) {
                    const std::size_t remaining = std::strlen(left);
                    if (remaining < length)
                        return -1;
                    left += remaining - length;
                    continue;
                }
                char key[256];
                const std::size_t copied = (std::min)(length, sizeof(key) - 1);
                std::memcpy(key, segment, copied);
                key[copied] = '\0';
                left = find_string(
                    left, key, upperCase == IVP_TRUE ? 3 : 2);
                if (!left)
                    return -1;
                left += copied;
                right = end;
                continue;
            }
            if (!a)
                return static_cast<unsigned char>(b);
            if (!b)
                return static_cast<unsigned char>(a);
            if (b != '?') {
                if (upperCase == IVP_TRUE) {
                    const char leftFolded =
                        a >= 'a' && a <= 'z' ? static_cast<char>(a - 'a' + 'A') : a;
                    const char rightFolded =
                        b >= 'a' && b <= 'z' ? static_cast<char>(b - 'a' + 'A') : b;
                    if (leftFolded != rightFolded)
                        return 1;
                } else if (a != b) {
                    return 1;
                }
            }
            ++left;
            ++right;
        }
    }
};

static_assert(sizeof(P_List<int>) == 0x08);
static_assert(offsetof(P_List<int>, first) == 0x00);
static_assert(offsetof(P_List<int>, len) == 0x04);
static_assert(sizeof(P_String) == 0x01);

enum IVP_Movement_Type : std::int32_t {
    IVP_MT_UNDEFINED = 0x00,
    IVP_MT_MOVING = 0x01,
    IVP_MT_SLOW = 0x02,
    IVP_MT_CALM = 0x03,
    IVP_MT_NOT_SIM = 0x08,
    IVP_MT_STATIC_PHANTOM = 0x09,
    IVP_MT_STATIC = 0x10,
    IVP_MT_GET_MINDIST = 0x21,
};

enum IVP_ENV_STATE : std::int32_t {
    IVP_ES_PSI = 0,
    IVP_ES_PSI_INTEGRATOR = 1,
    IVP_ES_PSI_HULL = 2,
    IVP_ES_PSI_SHORT = 3,
    IVP_ES_PSI_CRITIC = 4,
    IVP_ES_AT = 5,
};

class IVP_Time {
    friend struct BML_IvpTimeLayoutCheck;

public:
    IVP_Time() = default;
    IVP_Time(double time) : seconds(time) {}

    double get_seconds() const { return seconds; }
    double get_time() const { return seconds; }
    void operator+=(double value) { seconds += value; }
    void operator-=(const IVP_Time value) { seconds -= value.seconds; }
    double operator-(const IVP_Time &other) const {
        // Do not copy the nearby source revision's intermediate IVP_FLOAT
        // cast: Ballance callers retain binary64 elapsed-time precision.
        return seconds - other.seconds;
    }
    IVP_Time operator+(double value) const { return IVP_Time(seconds + value); }

private:
    // The retail inline default constructor deliberately leaves the clock
    // value untouched. Callers that need an epoch must pass it explicitly.
    IVP_DOUBLE seconds;
};

struct BML_IvpTimeLayoutCheck {
    static constexpr std::size_t seconds = offsetof(IVP_Time, seconds);
};

class IVP_Inline_Math {
public:
    static IVP_RETURN_TYPE invert_2x2_matrix(
        const IVP_DOUBLE a1, const IVP_DOUBLE b1,
        const IVP_DOUBLE a2, const IVP_DOUBLE b2,
        IVP_DOUBLE *inverseA1, IVP_DOUBLE *inverseB1,
        IVP_DOUBLE *inverseA2, IVP_DOUBLE *inverseB2) {
        const IVP_DOUBLE determinant = a1 * b2 - b1 * a2;
        if (determinant * determinant < 1.0e-20)
            return IVP_FAULT;
        const IVP_DOUBLE inverse = 1.0 / determinant;
        *inverseA1 = b2 * inverse;
        *inverseB1 = -a2 * inverse;
        *inverseA2 = -b1 * inverse;
        *inverseB2 = a1 * inverse;
        return IVP_OK;
    }
    static IVP_FLOAT clamp(IVP_FLOAT value, IVP_FLOAT minimum,
                           IVP_FLOAT maximum) {
        return value < minimum ? minimum : (value > maximum ? maximum : value);
    }
    static IVP_FLOAT approx5_sin(IVP_FLOAT angle) {
        const IVP_FLOAT square = angle * angle;
        return angle - angle * square / 6.0f +
               angle * square * square / 120.0f;
    }
    static IVP_FLOAT approx5_cos(IVP_FLOAT angle) {
        const IVP_FLOAT square = angle * angle;
        return 1.0f - square * 0.5f + square * square / 24.0f;
    }
    static IVP_FLOAT save_acosf(IVP_FLOAT value) {
        return value >= 1.0f ? 0.0f : static_cast<IVP_FLOAT>(std::acos(value));
    }
    static IVP_FLOAT fast_approx_asin(IVP_FLOAT angle) {
        const IVP_FLOAT square = angle * angle;
        return angle + angle * square / 6.0f +
               3.0f * angle * square * square / 40.0f;
    }
    static IVP_FLOAT upper_limit_asin(IVP_FLOAT angle) {
        const IVP_FLOAT square = angle * angle;
        return angle + angle * square / 6.0f +
               0.40414f * angle * square * square;
    }
    static IVP_FLOAT fast_asin(IVP_FLOAT angle) {
        const IVP_FLOAT square = angle * angle;
        return angle + 0.12f * angle * square +
               0.29f * angle * square * square;
    }
    static IVP_FLOAT fast_anywhere_asin(IVP_FLOAT angle) {
        const IVP_FLOAT square = angle * angle;
        return 1.1f * angle - 0.419f * angle * square +
               0.76f * angle * square * square;
    }
    static IVP_FLOAT isqrt_float(IVP_FLOAT square) {
        return BML::IVP::ABI::Invoke<IVP_FLOAT>(
            BML::IVP::ABI::Address::InlineMathInverseSqrtFloat, square);
    }
    static IVP_DOUBLE isqrt_double(IVP_DOUBLE square) {
        return BML::IVP::ABI::Invoke<IVP_DOUBLE>(
            BML::IVP::ABI::Address::InlineMathInverseSqrtDouble, square);
    }
    static int int_div_2(int value) { return value / 2; }
    static IVP_DOUBLE fabsd(IVP_DOUBLE value) { return std::fabs(value); }
    static IVP_DOUBLE ivp_sqrtf(IVP_DOUBLE value) { return std::sqrt(value); }
    static IVP_DOUBLE sqrtd(IVP_DOUBLE value) { return std::sqrt(value); }
    static IVP_DOUBLE ivp_sinf(IVP_DOUBLE value) { return std::sin(value); }
    static IVP_DOUBLE ivp_cosf(IVP_DOUBLE value) { return std::cos(value); }
    static IVP_DOUBLE sind(IVP_DOUBLE value) { return std::sin(value); }
    static IVP_DOUBLE cosd(IVP_DOUBLE value) { return std::cos(value); }
    static IVP_DOUBLE acosd(IVP_DOUBLE value) { return std::acos(value); }
    static IVP_DOUBLE asind(IVP_DOUBLE value) { return std::asin(value); }
    static IVP_DOUBLE atand(IVP_DOUBLE value) { return std::atan(value); }
    static IVP_DOUBLE ivp_expf(IVP_DOUBLE value) { return std::exp(value); }
    static IVP_DOUBLE atan2d(IVP_DOUBLE y, IVP_DOUBLE x) {
        return std::atan2(y, x);
    }
};

struct IVP_U_Float_Point3 {
    IVP_FLOAT k[3];

    void set(const IVP_FLOAT source[3]) {
        k[0] = source[0]; k[1] = source[1]; k[2] = source[2];
    }
    void set(const IVP_DOUBLE source[3]) {
        k[0] = static_cast<IVP_FLOAT>(source[0]);
        k[1] = static_cast<IVP_FLOAT>(source[1]);
        k[2] = static_cast<IVP_FLOAT>(source[2]);
    }
    void byte_swap() {
        BML::IVP::Detail::ByteSwapFloat(k[0]);
        BML::IVP::Detail::ByteSwapFloat(k[1]);
        BML::IVP::Detail::ByteSwapFloat(k[2]);
    }
};

struct IVP_U_Point;
struct IVP_U_Quat;
struct IVP_U_Float_Quat;
class IVP_U_Hesse;

struct IVP_U_Float_Point {
    // Keep the retail IVP default-construction semantics: this POD-like
    // vector is intentionally left uninitialised until a caller sets it.
    // Several retained constructors (notably IVP_Friction_Core_Pair) rely on
    // that property for debug-only scratch members.
    IVP_FLOAT k[3];
    IVP_FLOAT hesse_val;

    IVP_U_Float_Point() = default;
    IVP_U_Float_Point(IVP_DOUBLE x, IVP_DOUBLE y, IVP_DOUBLE z) {
        set(static_cast<IVP_FLOAT>(x), static_cast<IVP_FLOAT>(y),
            static_cast<IVP_FLOAT>(z));
    }
    explicit IVP_U_Float_Point(const IVP_U_Float_Point *source) {
        set(source);
    }
    explicit IVP_U_Float_Point(const IVP_U_Point *source);

    void set(const IVP_U_Point *source);
    void set(const IVP_FLOAT source[3]) {
        k[0] = source[0]; k[1] = source[1]; k[2] = source[2];
    }
    void set(const IVP_U_Float_Point *source) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::FloatPointSet, this,
            [this, source] {
                k[0] = source->k[0]; k[1] = source->k[1];
                k[2] = source->k[2];
            }, source);
    }
    void set(IVP_FLOAT x, IVP_FLOAT y, IVP_FLOAT z) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::FloatPointSetComponents,
            this, [this, x, y, z] { k[0] = x; k[1] = y; k[2] = z; },
            x, y, z);
    }
    void set_to_zero() { set(0.0f, 0.0f, 0.0f); }
    void byte_swap() {
        BML::IVP::Detail::ByteSwapFloat(k[0]);
        BML::IVP::Detail::ByteSwapFloat(k[1]);
        BML::IVP::Detail::ByteSwapFloat(k[2]);
        BML::IVP::Detail::ByteSwapFloat(hesse_val);
    }
    void set_negative(const IVP_U_Float_Point *source) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::FloatPointSetNegative, this,
            [this, source] {
                k[0] = -source->k[0]; k[1] = -source->k[1];
                k[2] = -source->k[2];
            }, source);
    }
    IVP_DOUBLE quad_length() const {
        return BML::IVP::ABI::InvokeThisOr<IVP_DOUBLE>(
            BML::IVP::ABI::Address::FloatPointQuadraticLength,
            const_cast<IVP_U_Float_Point *>(this),
            [this] {
                return static_cast<IVP_DOUBLE>(k[0]) * k[0] +
                       static_cast<IVP_DOUBLE>(k[1]) * k[1] +
                       static_cast<IVP_DOUBLE>(k[2]) * k[2];
            });
    }
    IVP_DOUBLE real_length() const {
        return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
            BML::IVP::ABI::Address::FloatPointRealLength,
            const_cast<IVP_U_Float_Point *>(this));
    }
    IVP_RETURN_TYPE fast_normize() {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::FloatPointFastNormalize, this);
    }
    void mult(IVP_DOUBLE factor) {
        k[0] = static_cast<IVP_FLOAT>(k[0] * factor);
        k[1] = static_cast<IVP_FLOAT>(k[1] * factor);
        k[2] = static_cast<IVP_FLOAT>(k[2] * factor);
    }
    void add(const IVP_U_Float_Point *other) {
        k[0] += other->k[0]; k[1] += other->k[1]; k[2] += other->k[2];
    }
    void add(const IVP_U_Float_Point *left, const IVP_U_Float_Point *right) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::FloatPointAddPair,
            this, [this, left, right] {
                k[0] = left->k[0] + right->k[0];
                k[1] = left->k[1] + right->k[1];
                k[2] = left->k[2] + right->k[2];
            }, left, right);
    }
    void add_multiple(const IVP_U_Float_Point *value, IVP_DOUBLE factor) {
        k[0] += static_cast<IVP_FLOAT>(value->k[0] * factor);
        k[1] += static_cast<IVP_FLOAT>(value->k[1] * factor);
        k[2] += static_cast<IVP_FLOAT>(value->k[2] * factor);
    }
    void add_multiple(const IVP_U_Float_Point *base,
                      const IVP_U_Float_Point *value, IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::FloatPointAddMultiplePair,
            this, [this, base, value, factor] {
                k[0] = static_cast<IVP_FLOAT>(base->k[0] + value->k[0] * factor);
                k[1] = static_cast<IVP_FLOAT>(base->k[1] + value->k[1] * factor);
                k[2] = static_cast<IVP_FLOAT>(base->k[2] + value->k[2] * factor);
            }, base, value, factor);
    }
    void set_multiple(const IVP_U_Float_Point *value, IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::FloatPointSetMultiple,
            this, [this, value, factor] {
                k[0] = static_cast<IVP_FLOAT>(value->k[0] * factor);
                k[1] = static_cast<IVP_FLOAT>(value->k[1] * factor);
                k[2] = static_cast<IVP_FLOAT>(value->k[2] * factor);
            }, value, factor);
    }
    void subtract(const IVP_U_Float_Point *other) {
        k[0] -= other->k[0]; k[1] -= other->k[1]; k[2] -= other->k[2];
    }
    void subtract(const IVP_U_Float_Point *left,
                  const IVP_U_Float_Point *right) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::FloatPointSubtractPair,
            this, [this, left, right] {
                k[0] = left->k[0] - right->k[0];
                k[1] = left->k[1] - right->k[1];
                k[2] = left->k[2] - right->k[2];
            }, left, right);
    }
    void inline_subtract_and_mult(const IVP_U_Float_Point *left,
                                  const IVP_U_Float_Point *right,
                                  IVP_DOUBLE factor) {
        subtract(left, right); mult(factor);
    }
    void set_pairwise_mult(const IVP_U_Float_Point *left,
                           const IVP_U_Float_Point *right) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::FloatPointSetPairwiseMultiply,
            this, [this, left, right] {
                k[0] = left->k[0] * right->k[0];
                k[1] = left->k[1] * right->k[1];
                k[2] = left->k[2] * right->k[2];
            }, left, right);
    }
    IVP_DOUBLE dot_product(const IVP_U_Float_Point *other) const {
        return BML::IVP::ABI::InvokeThisOr<IVP_DOUBLE>(
            BML::IVP::ABI::Address::FloatPointDotProduct,
            const_cast<IVP_U_Float_Point *>(this),
            [this, other] {
                return static_cast<IVP_DOUBLE>(k[0]) * other->k[0] +
                       static_cast<IVP_DOUBLE>(k[1]) * other->k[1] +
                       static_cast<IVP_DOUBLE>(k[2]) * other->k[2];
            }, other);
    }
    void inline_calc_cross_product(const IVP_U_Float_Point *left,
                                   const IVP_U_Float_Point *right) {
        const IVP_FLOAT x = left->k[1] * right->k[2] - left->k[2] * right->k[1];
        const IVP_FLOAT y = left->k[2] * right->k[0] - left->k[0] * right->k[2];
        const IVP_FLOAT z = left->k[0] * right->k[1] - left->k[1] * right->k[0];
        set(x, y, z);
    }
    void calc_cross_product(const IVP_U_Float_Point *left,
                            const IVP_U_Float_Point *right) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::FloatPointCrossProduct,
            this, left, right);
    }
    void inline_calc_cross_product_and_normize(
        const IVP_U_Float_Point *left,
        const IVP_U_Float_Point *right) {
        inline_calc_cross_product(left, right);
        normize();
    }
    void inline_set_vert_to_area_defined_by_three_points(
        const IVP_U_Float_Point *point0,
        const IVP_U_Float_Point *point1,
        const IVP_U_Float_Point *point2) {
        const IVP_DOUBLE ax = point1->k[0] - point0->k[0];
        const IVP_DOUBLE ay = point1->k[1] - point0->k[1];
        const IVP_DOUBLE az = point1->k[2] - point0->k[2];
        const IVP_DOUBLE bx = point2->k[0] - point0->k[0];
        const IVP_DOUBLE by = point2->k[1] - point0->k[1];
        const IVP_DOUBLE bz = point2->k[2] - point0->k[2];
        set(static_cast<IVP_FLOAT>(by * az - ay * bz),
            static_cast<IVP_FLOAT>(bz * ax - az * bx),
            static_cast<IVP_FLOAT>(bx * ay - ax * by));
    }
    void set_orthogonal_part(const IVP_U_Float_Point *vector,
                             const IVP_U_Float_Point *normal) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::FloatPointOrthogonalPart,
            this, vector, normal);
    }
    IVP_RETURN_TYPE normize() {
        const IVP_DOUBLE length = real_length();
        if (length <= 1.0e-10)
            return IVP_FAULT;
        mult(1.0 / length);
        return IVP_OK;
    }
    IVP_DOUBLE fast_real_length() const {
        return std::sqrt(quad_length());
    }
    IVP_DOUBLE quad_distance_to(const IVP_U_Float_Point *other) const {
        const IVP_DOUBLE x = static_cast<IVP_DOUBLE>(k[0]) - other->k[0];
        const IVP_DOUBLE y = static_cast<IVP_DOUBLE>(k[1]) - other->k[1];
        const IVP_DOUBLE z = static_cast<IVP_DOUBLE>(k[2]) - other->k[2];
        return x * x + y * y + z * z;
    }
    IVP_DOUBLE real_length_plus_normize() {
        return BML::IVP::ABI::InvokeThisOr<IVP_DOUBLE>(
            BML::IVP::ABI::Address::FloatPointLengthAndNormalize, this,
            [this] {
                const IVP_DOUBLE length = real_length();
                if (length > 1.0e-10) mult(1.0 / length);
                return length;
            });
    }
    void set_interpolate(const IVP_U_Float_Point *from,
                         const IVP_U_Float_Point *to, IVP_DOUBLE factor) {
        set(from);
        add_multiple(to, factor);
        add_multiple(from, -factor);
    }
    void line_sqrt() {
        k[0] = std::sqrt(k[0]);
        k[1] = std::sqrt(k[1]);
        k[2] = std::sqrt(k[2]);
    }
    void rotate(IVP_COORDINATE_INDEX axis, IVP_FLOAT angle) {
        static constexpr int indices[5] = {0, 1, 2, 0, 1};
        const int *component = &indices[axis];
        const IVP_DOUBLE cosine = std::cos(angle);
        const IVP_DOUBLE sine = std::sin(angle);
        const IVP_DOUBLE first = cosine * k[component[1]] -
                                 sine * k[component[2]];
        k[component[2]] = static_cast<IVP_FLOAT>(
            sine * k[component[1]] + cosine * k[component[2]]);
        k[component[1]] = static_cast<IVP_FLOAT>(first);
    }
    void print(const char *comment = nullptr) const {
        if (comment)
            std::printf("%s Point %f %f %f\n", comment, k[0], k[1], k[2]);
        else
            std::printf("Point %f %f %f\n", k[0], k[1], k[2]);
    }

    void subtract(const IVP_U_Point *left, const IVP_U_Point *right);
    void inline_subtract_and_mult(const IVP_U_Point *left,
                                  const IVP_U_Point *right,
                                  IVP_DOUBLE factor);
    void set_multiple(const IVP_U_Point *value, IVP_DOUBLE factor);
    void set_multiple(const IVP_U_Quat *value, IVP_DOUBLE factor);
};

// A distinct source-level type in the retail engine even though the
// IVP_VECTOR_UNIT_FLOAT build stores the plane distance in the base class's
// fourth float. The type remains exactly 0x10 bytes.
struct IVP_U_Float_Hesse : IVP_U_Float_Point {
    IVP_U_Float_Hesse() = default;
    IVP_U_Float_Hesse(IVP_DOUBLE x, IVP_DOUBLE y, IVP_DOUBLE z,
                      IVP_DOUBLE distance) {
        set(static_cast<IVP_FLOAT>(x), static_cast<IVP_FLOAT>(y),
            static_cast<IVP_FLOAT>(z));
        hesse_val = static_cast<IVP_FLOAT>(distance);
    }

    void set4(const IVP_U_Float_Hesse *source) {
        set(source);
        hesse_val = source->hesse_val;
    }
    void calc_hesse(const IVP_U_Float_Point *point0,
                    const IVP_U_Float_Point *point1,
                    const IVP_U_Float_Point *point2) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::FloatHesseCalculate,
            this, point0, point1, point2);
    }
    void calc_hesse_val(const IVP_U_Float_Point *point) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::FloatHesseCalculateValue,
            this, [this, point] {
                hesse_val = -static_cast<IVP_FLOAT>(dot_product(point));
            }, point);
    }
    void proj_on_plane(const IVP_U_Float_Point *point,
                       IVP_U_Float_Point *result) const {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::FloatHesseProjectPlane,
            const_cast<IVP_U_Float_Hesse *>(this),
            [this, point, result] {
                result->set(point);
                result->add_multiple(this, -get_dist(point));
            }, point, result);
    }
    void mult_hesse(IVP_DOUBLE factor) {
        mult(factor);
        hesse_val = static_cast<IVP_FLOAT>(hesse_val * factor);
    }
    void normize() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::FloatHesseNormalize, this);
    }
    IVP_DOUBLE get_dist(const IVP_U_Float_Point *point) const {
        return dot_product(point) + hesse_val;
    }
    void byte_swap() { IVP_U_Float_Point::byte_swap(); }
};

struct alignas(8) IVP_U_Point {
    // Like the retail IVP value type, default construction is a no-op.  This
    // matters for matrix/scratch storage whose owner initializes only the
    // components it consumes.
    IVP_DOUBLE k[3];
    IVP_DOUBLE hesse_val;

    IVP_U_Point() = default;
    IVP_U_Point(IVP_DOUBLE x, IVP_DOUBLE y, IVP_DOUBLE z) { set(x, y, z); }
    IVP_U_Point(const IVP_U_Float_Point &source) { set(&source); }

    void set(const IVP_U_Point *source) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::PointSet, this,
            [this, source] {
                k[0] = source->k[0]; k[1] = source->k[1];
                k[2] = source->k[2];
            }, source);
    }
    void set(const IVP_U_Float_Point *source) {
        k[0] = source->k[0]; k[1] = source->k[1]; k[2] = source->k[2];
    }
    void set(IVP_DOUBLE x, IVP_DOUBLE y, IVP_DOUBLE z = 0.0) {
        k[0] = x; k[1] = y; k[2] = z;
    }
    void set_to_zero() { set(0.0, 0.0, 0.0); }
    void byte_swap() {
        BML::IVP::Detail::ByteSwapDouble(k[0]);
        BML::IVP::Detail::ByteSwapDouble(k[1]);
        BML::IVP::Detail::ByteSwapDouble(k[2]);
        BML::IVP::Detail::ByteSwapDouble(hesse_val);
    }
    void set_negative(const IVP_U_Point *source) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::PointSetNegative, this,
            [this, source] {
                k[0] = -source->k[0]; k[1] = -source->k[1];
                k[2] = -source->k[2];
            }, source);
    }
    IVP_DOUBLE dot_product(const IVP_U_Point *other) const {
        return k[0] * other->k[0] + k[1] * other->k[1] + k[2] * other->k[2];
    }
    IVP_DOUBLE dot_product(const IVP_U_Float_Point *other) const {
        return BML::IVP::ABI::InvokeThisOr<IVP_DOUBLE>(
            BML::IVP::ABI::Address::PointDotFloat,
            const_cast<IVP_U_Point *>(this),
            [this, other] {
                return k[0] * other->k[0] + k[1] * other->k[1] +
                       k[2] * other->k[2];
            }, other);
    }
    IVP_DOUBLE quad_length() const { return dot_product(this); }
    IVP_DOUBLE real_length() const { return std::sqrt(quad_length()); }
    void mult(IVP_DOUBLE factor) {
        k[0] *= factor; k[1] *= factor; k[2] *= factor;
    }
    void add(const IVP_U_Point *other) {
        k[0] += other->k[0]; k[1] += other->k[1]; k[2] += other->k[2];
    }
    void add(const IVP_U_Float_Point *other) {
        k[0] += other->k[0]; k[1] += other->k[1]; k[2] += other->k[2];
    }
    void add(const IVP_U_Point *left, const IVP_U_Point *right) {
        set(left); add(right);
    }
    void add(const IVP_U_Float_Point *left,
             const IVP_U_Float_Point *right) {
        set(left); add(right);
    }
    void add_multiple(const IVP_U_Point *value, IVP_DOUBLE factor) {
        k[0] += value->k[0] * factor;
        k[1] += value->k[1] * factor;
        k[2] += value->k[2] * factor;
    }
    void add_multiple(const IVP_U_Float_Point *value, IVP_DOUBLE factor) {
        k[0] += value->k[0] * factor;
        k[1] += value->k[1] * factor;
        k[2] += value->k[2] * factor;
    }
    void add_multiple(const IVP_U_Point *base, const IVP_U_Point *value,
                      IVP_DOUBLE factor) {
        set(base); add_multiple(value, factor);
    }
    void add_multiple(const IVP_U_Point *base,
                      const IVP_U_Float_Point *value, IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::PointAddMultipleMixed,
            this, [this, base, value, factor] {
                k[0] = base->k[0] + value->k[0] * factor;
                k[1] = base->k[1] + value->k[1] * factor;
                k[2] = base->k[2] + value->k[2] * factor;
            }, base, value, factor);
    }
    void subtract(const IVP_U_Point *other) {
        k[0] -= other->k[0]; k[1] -= other->k[1]; k[2] -= other->k[2];
    }
    void subtract(const IVP_U_Float_Point *other) {
        k[0] -= other->k[0]; k[1] -= other->k[1]; k[2] -= other->k[2];
    }
    void subtract(const IVP_U_Point *left, const IVP_U_Point *right) {
        set(left); subtract(right);
    }
    void subtract(const IVP_U_Float_Point *left,
                  const IVP_U_Float_Point *right) {
        set(left); subtract(right);
    }
    void subtract(const IVP_U_Float_Point *left,
                  const IVP_U_Point *right) {
        set(left); subtract(right);
    }
    void subtract(const IVP_U_Point *left,
                  const IVP_U_Float_Point *right) {
        set(left); subtract(right);
    }
    void inline_subtract_and_mult(const IVP_U_Point *left,
                                  const IVP_U_Point *right,
                                  IVP_DOUBLE factor) {
        subtract(left, right); mult(factor);
    }
    void inline_subtract_and_mult(const IVP_U_Float_Point *left,
                                  const IVP_U_Float_Point *right,
                                  IVP_DOUBLE factor) {
        subtract(left, right); mult(factor);
    }
    void set_multiple(const IVP_U_Point *value, IVP_DOUBLE factor) {
        set(value); mult(factor);
    }
    void set_multiple(const IVP_U_Float_Point *value, IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::PointSetMultipleFloat,
            this, [this, value, factor] {
                k[0] = value->k[0] * factor;
                k[1] = value->k[1] * factor;
                k[2] = value->k[2] * factor;
            }, value, factor);
    }
    void set_pairwise_mult(const IVP_U_Point *left,
                           const IVP_U_Point *right) {
        set(left->k[0] * right->k[0], left->k[1] * right->k[1],
            left->k[2] * right->k[2]);
    }
    void set_pairwise_mult(const IVP_U_Point *left,
                           const IVP_U_Float_Point *right) {
        set(left->k[0] * right->k[0], left->k[1] * right->k[1],
            left->k[2] * right->k[2]);
    }
    void inline_calc_cross_product(const IVP_U_Point *left,
                                   const IVP_U_Point *right) {
        const IVP_DOUBLE x = left->k[1] * right->k[2] - left->k[2] * right->k[1];
        const IVP_DOUBLE y = left->k[2] * right->k[0] - left->k[0] * right->k[2];
        const IVP_DOUBLE z = left->k[0] * right->k[1] - left->k[1] * right->k[0];
        set(x, y, z);
    }
    void calc_cross_product(const IVP_U_Point *left,
                            const IVP_U_Point *right) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PointCrossProduct,
            this, left, right);
    }
    void inline_calc_cross_product_and_normize(const IVP_U_Point *left,
                                               const IVP_U_Point *right) {
        inline_calc_cross_product(left, right);
        normize();
    }
    void inline_set_vert_to_area_defined_by_three_points(
        const IVP_U_Point *point0, const IVP_U_Point *point1,
        const IVP_U_Point *point2) {
        const IVP_DOUBLE ax = point1->k[0] - point0->k[0];
        const IVP_DOUBLE ay = point1->k[1] - point0->k[1];
        const IVP_DOUBLE az = point1->k[2] - point0->k[2];
        const IVP_DOUBLE bx = point2->k[0] - point0->k[0];
        const IVP_DOUBLE by = point2->k[1] - point0->k[1];
        const IVP_DOUBLE bz = point2->k[2] - point0->k[2];
        set(by * az - ay * bz, bz * ax - az * bx, bx * ay - ax * by);
    }
    void inline_set_vert_to_area_defined_by_three_points(
        const IVP_U_Float_Point *point0,
        const IVP_U_Float_Point *point1,
        const IVP_U_Point *point2) {
        const IVP_U_Point p0(*point0);
        const IVP_U_Point p1(*point1);
        inline_set_vert_to_area_defined_by_three_points(&p0, &p1, point2);
    }
    void inline_set_vert_to_area_defined_by_three_points(
        const IVP_U_Float_Point *point0,
        const IVP_U_Float_Point *point1,
        const IVP_U_Float_Point *point2) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::PointSetAreaNormalFloat,
            this, [this, point0, point1, point2] {
                const IVP_DOUBLE ax = point1->k[0] - point0->k[0];
                const IVP_DOUBLE ay = point1->k[1] - point0->k[1];
                const IVP_DOUBLE az = point1->k[2] - point0->k[2];
                const IVP_DOUBLE bx = point2->k[0] - point0->k[0];
                const IVP_DOUBLE by = point2->k[1] - point0->k[1];
                const IVP_DOUBLE bz = point2->k[2] - point0->k[2];
                set(by * az - ay * bz, bz * ax - az * bx,
                    bx * ay - ax * by);
            }, point0, point1, point2);
    }
    void solve_quadratic_equation_accurate(const IVP_U_Point *equation) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PointSolveQuadratic, this, equation);
    }
    IVP_BOOL is_parallel(const IVP_U_Point *other,
                         IVP_DOUBLE epsilon) const {
        return BML::IVP::ABI::InvokeThis<IVP_BOOL>(
            BML::IVP::ABI::Address::PointIsParallel,
            const_cast<IVP_U_Point *>(this), other, epsilon);
    }
    void line_min(const IVP_U_Point *other) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PointLineMin, this, other);
    }
    void line_max(const IVP_U_Point *other) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PointLineMax, this, other);
    }
    void calc_an_orthogonal(const IVP_U_Point *source) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PointCalculateOrthogonal, this, source);
    }
    IVP_RETURN_TYPE normize() {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::PointNormalize, this);
    }
    IVP_RETURN_TYPE fast_normize() {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::PointFastNormalize, this);
    }
    IVP_DOUBLE fast_real_length() const {
        return std::sqrt(quad_length());
    }
    IVP_DOUBLE quad_distance_to(const IVP_U_Point *other) const {
        const IVP_DOUBLE x = k[0] - other->k[0];
        const IVP_DOUBLE y = k[1] - other->k[1];
        const IVP_DOUBLE z = k[2] - other->k[2];
        return x * x + y * y + z * z;
    }
    IVP_DOUBLE quad_distance_to(const IVP_U_Float_Point *other) const {
        const IVP_DOUBLE x = k[0] - other->k[0];
        const IVP_DOUBLE y = k[1] - other->k[1];
        const IVP_DOUBLE z = k[2] - other->k[2];
        return x * x + y * y + z * z;
    }
    IVP_DOUBLE real_length_plus_normize() {
        return BML::IVP::ABI::InvokeThisOr<IVP_DOUBLE>(
            BML::IVP::ABI::Address::PointLengthAndNormalize, this,
            [this] {
                const IVP_DOUBLE length = real_length();
                if (length > 1.0e-10) mult(1.0 / length);
                return length;
            });
    }
    void solve_quadratic_equation_fast(const IVP_U_Point *equation) {
        constexpr IVP_DOUBLE epsilon = 1.0e-19;
        if (std::fabs(equation->k[0]) < epsilon) {
            k[0] = 0.0;
            if (std::fabs(equation->k[1]) < epsilon) {
                k[0] = -1.0;
                return;
            }
            k[1] = k[2] = -equation->k[2] / equation->k[1];
            return;
        }
        k[0] = equation->k[1] * equation->k[1] -
               4.0 * equation->k[0] * equation->k[2];
        if (k[0] >= 0.0) {
            const IVP_DOUBLE root = std::sqrt(k[0]);
            const IVP_DOUBLE inverse2a = 0.5 / equation->k[0];
            k[1] = (-equation->k[1] - root) * inverse2a;
            k[2] = (-equation->k[1] + root) * inverse2a;
        }
    }
    void set_orthogonal_part(const IVP_U_Point *vector,
                             const IVP_U_Point *normal) {
        const IVP_DOUBLE component = normal->dot_product(vector);
        add_multiple(vector, normal, -component);
    }
    void set_orthogonal_part(const IVP_U_Point *vector,
                             const IVP_U_Float_Point *normal) {
        const IVP_DOUBLE component = vector->dot_product(normal);
        add_multiple(vector, normal, -component);
    }
    void rotate(IVP_COORDINATE_INDEX axis, IVP_FLOAT angle) {
        static constexpr int indices[5] = {0, 1, 2, 0, 1};
        const int *component = &indices[axis];
        const IVP_DOUBLE cosine = std::cos(angle);
        const IVP_DOUBLE sine = std::sin(angle);
        const IVP_DOUBLE first = cosine * k[component[1]] -
                                 sine * k[component[2]];
        k[component[2]] = sine * k[component[1]] + cosine * k[component[2]];
        k[component[1]] = first;
    }
    void print(const char *comment = nullptr) {
        if (comment)
            std::printf("%s Point %f %f %f\n", comment, k[0], k[1], k[2]);
        else
            std::printf("Point %f %f %f\n", k[0], k[1], k[2]);
    }
    void set(const IVP_FLOAT source[3]) {
        set(static_cast<IVP_DOUBLE>(source[0]),
            static_cast<IVP_DOUBLE>(source[1]),
            static_cast<IVP_DOUBLE>(source[2]));
    }
    void set(const IVP_U_Quat *source);
    void set_multiple(const IVP_U_Quat *source, IVP_DOUBLE factor);
    IVP_RETURN_TYPE set_crossing(IVP_U_Hesse *plane0,
                                 IVP_U_Hesse *plane1,
                                 IVP_U_Hesse *plane2);
    void set_interpolate(const IVP_U_Point *from, const IVP_U_Point *to,
                         IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::PointInterpolate,
            this, [this, from, to, factor] {
                set(from);
                add_multiple(to, factor);
                add_multiple(from, -factor);
            }, from, to, factor);
    }
    void set_interpolate(const IVP_U_Float_Point *from,
                         const IVP_U_Float_Point *to,
                         IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PointInterpolateFloat,
            this, from, to, factor);
    }
};

inline void IVP_U_Float_Point::set(const IVP_U_Point *source) {
    k[0] = static_cast<IVP_FLOAT>(source->k[0]);
    k[1] = static_cast<IVP_FLOAT>(source->k[1]);
    k[2] = static_cast<IVP_FLOAT>(source->k[2]);
}

inline IVP_U_Float_Point::IVP_U_Float_Point(const IVP_U_Point *source) {
    set(source);
}

inline void IVP_U_Float_Point::subtract(const IVP_U_Point *left,
                                        const IVP_U_Point *right) {
    set(static_cast<IVP_FLOAT>(left->k[0] - right->k[0]),
        static_cast<IVP_FLOAT>(left->k[1] - right->k[1]),
        static_cast<IVP_FLOAT>(left->k[2] - right->k[2]));
}

inline void IVP_U_Float_Point::inline_subtract_and_mult(
    const IVP_U_Point *left, const IVP_U_Point *right, IVP_DOUBLE factor) {
    subtract(left, right);
    mult(factor);
}

inline void IVP_U_Float_Point::set_multiple(const IVP_U_Point *value,
                                            IVP_DOUBLE factor) {
    set(static_cast<IVP_FLOAT>(value->k[0] * factor),
        static_cast<IVP_FLOAT>(value->k[1] * factor),
        static_cast<IVP_FLOAT>(value->k[2] * factor));
}

struct alignas(8) IVP_U_Matrix3 {
    IVP_U_Point rows[3];

    IVP_U_Point *get_row(IVP_COORDINATE_INDEX index) { return &rows[index]; }
    const IVP_U_Point *get_row(IVP_COORDINATE_INDEX index) const {
        return &rows[index];
    }
    void set_identity() {
        rows[0].set(1.0, 0.0, 0.0);
        rows[1].set(0.0, 1.0, 0.0);
        rows[2].set(0.0, 0.0, 1.0);
    }
    void init3() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::Matrix3Initialize, this);
    }
    void init_normized3_col(const IVP_U_Point *column,
                            IVP_COORDINATE_INDEX index) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::Matrix3InitializeNormalizedColumn,
            this, column, index);
    }
    IVP_DOUBLE get_elem(int row, int column) const {
        return rows[row].k[column];
    }
    void set_elem(int row, int column, IVP_DOUBLE value) {
        rows[row].k[column] = value;
    }
    void inline_vmult3(const IVP_U_Point *input, IVP_U_Point *output) const {
        IVP_U_Point value;
        value.set(rows[0].dot_product(input), rows[1].dot_product(input),
                  rows[2].dot_product(input));
        output->set(&value);
    }
    void inline_vmult3(const IVP_U_Float_Point *input,
                       IVP_U_Float_Point *output) const {
        IVP_U_Float_Point value;
        value.set(static_cast<IVP_FLOAT>(rows[0].dot_product(input)),
                  static_cast<IVP_FLOAT>(rows[1].dot_product(input)),
                  static_cast<IVP_FLOAT>(rows[2].dot_product(input)));
        output->set(&value);
    }
    void inline_vimult3(const IVP_U_Point *input, IVP_U_Point *output) const {
        IVP_U_Point value;
        value.set(rows[0].k[0] * input->k[0] + rows[1].k[0] * input->k[1] +
                      rows[2].k[0] * input->k[2],
                  rows[0].k[1] * input->k[0] + rows[1].k[1] * input->k[1] +
                      rows[2].k[1] * input->k[2],
                  rows[0].k[2] * input->k[0] + rows[1].k[2] * input->k[1] +
                      rows[2].k[2] * input->k[2]);
        output->set(&value);
    }
    void inline_vimult3(const IVP_U_Float_Point *input,
                        IVP_U_Float_Point *output) const {
        IVP_U_Float_Point value;
        value.set(static_cast<IVP_FLOAT>(rows[0].k[0] * input->k[0] +
                                         rows[1].k[0] * input->k[1] +
                                         rows[2].k[0] * input->k[2]),
                  static_cast<IVP_FLOAT>(rows[0].k[1] * input->k[0] +
                                         rows[1].k[1] * input->k[1] +
                                         rows[2].k[1] * input->k[2]),
                  static_cast<IVP_FLOAT>(rows[0].k[2] * input->k[0] +
                                         rows[1].k[2] * input->k[1] +
                                         rows[2].k[2] * input->k[2]));
        output->set(&value);
    }
    void mmult3(const IVP_U_Matrix3 *right, IVP_U_Matrix3 *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::Matrix3Multiply,
            const_cast<IVP_U_Matrix3 *>(this), right, output);
    }
    void mimult3(const IVP_U_Matrix3 *right, IVP_U_Matrix3 *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::Matrix3InverseMultiply,
            const_cast<IVP_U_Matrix3 *>(this), right, output);
    }
    void mi2mult3(const IVP_U_Matrix3 *right, IVP_U_Matrix3 *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::Matrix3MultiplyInverse,
            const_cast<IVP_U_Matrix3 *>(this), right, output);
    }
    void transpose3() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::Matrix3Transpose, this);
    }
    void set_transpose3(const IVP_U_Matrix3 *source) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::Matrix3SetTranspose, this, source);
    }
    void vmult3(const IVP_U_Float_Point *input,
                IVP_U_Float_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::Matrix3TransformFloatPoint,
            const_cast<IVP_U_Matrix3 *>(this), input, output);
    }
    void vmult3(const IVP_U_Point *input, IVP_U_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::Matrix3TransformPoint,
            const_cast<IVP_U_Matrix3 *>(this), input, output);
    }
    void vimult3(const IVP_U_Point *input, IVP_U_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::Matrix3InverseTransformPoint,
            const_cast<IVP_U_Matrix3 *>(this), input, output);
    }
    void vimult3(const IVP_U_Float_Point *input,
                 IVP_U_Float_Point *output) const {
        inline_vimult3(input, output);
    }
    void get_row(IVP_COORDINATE_INDEX row, IVP_U_Point *output) const {
        output->set(&rows[row]);
    }
    void get_row(IVP_COORDINATE_INDEX row,
                 IVP_U_Float_Point *output) const {
        output->set(&rows[row]);
    }
    void get_col(IVP_COORDINATE_INDEX column, IVP_U_Point *output) const {
        output->set(rows[0].k[column], rows[1].k[column], rows[2].k[column]);
    }
    void get_col(IVP_COORDINATE_INDEX column,
                 IVP_U_Float_Point *output) const {
        output->set(static_cast<IVP_FLOAT>(rows[0].k[column]),
                    static_cast<IVP_FLOAT>(rows[1].k[column]),
                    static_cast<IVP_FLOAT>(rows[2].k[column]));
    }
    void set_row(IVP_COORDINATE_INDEX row, const IVP_U_Point *input) {
        rows[row].set(input);
    }
    void set_row(IVP_COORDINATE_INDEX row,
                 const IVP_U_Float_Point *input) {
        rows[row].set(input);
    }
    void set_col(IVP_COORDINATE_INDEX column, const IVP_U_Point *input) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::Matrix3SetColumnPoint,
            this, [this, column, input] {
                rows[0].k[column] = input->k[0];
                rows[1].k[column] = input->k[1];
                rows[2].k[column] = input->k[2];
            }, column, input);
    }
    void set_col(IVP_COORDINATE_INDEX column,
                 const IVP_U_Float_Point *input) {
        rows[0].k[column] = input->k[0];
        rows[1].k[column] = input->k[1];
        rows[2].k[column] = input->k[2];
    }
    void inline_mmult3(const IVP_U_Matrix3 *right,
                       IVP_U_Matrix3 *output) const {
        IVP_U_Matrix3 result;
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                result.rows[row].k[column] =
                    rows[row].k[0] * right->rows[0].k[column] +
                    rows[row].k[1] * right->rows[1].k[column] +
                    rows[row].k[2] * right->rows[2].k[column];
            }
        }
        *output = result;
    }
    void inline_mimult3(const IVP_U_Matrix3 *right,
                        IVP_U_Matrix3 *output) const {
        IVP_U_Matrix3 result;
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                result.rows[row].k[column] =
                    rows[0].k[row] * right->rows[0].k[column] +
                    rows[1].k[row] * right->rows[1].k[column] +
                    rows[2].k[row] * right->rows[2].k[column];
            }
        }
        *output = result;
    }
    void init_rows3(const IVP_U_Point *row0, const IVP_U_Point *row1,
                    const IVP_U_Point *row2) {
        rows[0].set(row0);
        rows[1].set(row1);
        rows[2].set(row2);
    }
    void init_columns3(const IVP_U_Point *column0,
                       const IVP_U_Point *column1,
                       const IVP_U_Point *column2) {
        set_col(IVP_INDEX_X, column0);
        set_col(IVP_INDEX_Y, column1);
        set_col(IVP_INDEX_Z, column2);
    }
    void init_normized3_col(const IVP_U_Point *column,
                            IVP_COORDINATE_INDEX index,
                            const IVP_U_Point *comparison) {
        IVP_U_Point axis0 = *column;
        axis0.normize();
        IVP_U_Point axis2;
        axis2.inline_calc_cross_product_and_normize(&axis0, comparison);
        IVP_U_Point axis1;
        axis1.inline_calc_cross_product(&axis2, &axis0);
        static constexpr IVP_COORDINATE_INDEX order[5] = {
            IVP_INDEX_X, IVP_INDEX_Y, IVP_INDEX_Z, IVP_INDEX_X, IVP_INDEX_Y};
        set_col(order[index], &axis0);
        set_col(order[index + 1], &axis1);
        set_col(order[index + 2], &axis2);
    }
    void init_normized3_row(const IVP_U_Point *row,
                            IVP_COORDINATE_INDEX index) {
        IVP_U_Point axis0 = *row;
        axis0.normize();
        IVP_U_Point axis2(axis0.k[1], axis0.k[2] - axis0.k[0],
                          -axis0.k[1]);
        if (axis2.normize() == IVP_FAULT) {
            axis2.set(axis0.k[2], -axis0.k[2],
                      axis0.k[1] - axis0.k[0]);
            axis2.normize();
        }
        IVP_U_Point axis1;
        axis1.inline_calc_cross_product(&axis2, &axis0);
        static constexpr IVP_COORDINATE_INDEX order[5] = {
            IVP_INDEX_X, IVP_INDEX_Y, IVP_INDEX_Z, IVP_INDEX_X, IVP_INDEX_Y};
        set_row(order[index], &axis0);
        set_row(order[index + 1], &axis1);
        set_row(order[index + 2], &axis2);
    }
    void init_normized3_row(const IVP_U_Point *row,
                            IVP_COORDINATE_INDEX index,
                            const IVP_U_Point *comparison) {
        IVP_U_Point axis0 = *row;
        axis0.normize();
        IVP_U_Point axis2;
        axis2.inline_calc_cross_product_and_normize(&axis0, comparison);
        IVP_U_Point axis1;
        axis1.inline_calc_cross_product(&axis2, &axis0);
        static constexpr IVP_COORDINATE_INDEX order[5] = {
            IVP_INDEX_X, IVP_INDEX_Y, IVP_INDEX_Z, IVP_INDEX_X, IVP_INDEX_Y};
        set_row(order[index], &axis0);
        set_row(order[index + 1], &axis1);
        set_row(order[index + 2], &axis2);
    }
    void init_rotated3(IVP_COORDINATE_INDEX axis, IVP_FLOAT angle) {
        static constexpr int indices[5] = {1, 2, 0, 1, 2};
        const int *component = &indices[axis];
        const IVP_DOUBLE sine = std::sin(angle);
        const IVP_DOUBLE cosine = std::cos(angle);
        for (auto &row : rows)
            row.set_to_zero();
        set_elem(component[2], component[2], 1.0);
        set_elem(component[0], component[0], cosine);
        set_elem(component[1], component[1], cosine);
        set_elem(component[0], component[1], -sine);
        set_elem(component[1], component[0], sine);
    }
    void orthogonize() {
        IVP_U_Point zAxis;
        IVP_U_Point yAxis;
        get_col(IVP_INDEX_Z, &zAxis);
        get_col(IVP_INDEX_Y, &yAxis);
        IVP_U_Point xAxis;
        IVP_U_Point correctedYAxis;
        xAxis.inline_calc_cross_product(&yAxis, &zAxis);
        correctedYAxis.inline_calc_cross_product(&zAxis, &xAxis);
        init_columns3(&xAxis, &correctedYAxis, &zAxis);
    }
    IVP_RETURN_TYPE normize() {
        if (rows[0].normize() == IVP_FAULT ||
            rows[1].normize() == IVP_FAULT ||
            rows[2].normize() == IVP_FAULT)
            return IVP_FAULT;
        return IVP_OK;
    }
    IVP_RETURN_TYPE orthonormize() {
        orthogonize();
        return normize();
    }
    IVP_DOUBLE quad_rot_distance_to(const IVP_U_Matrix3 *other) {
        return rows[0].quad_distance_to(&other->rows[0]) +
               rows[1].quad_distance_to(&other->rows[1]) +
               rows[2].quad_distance_to(&other->rows[2]);
    }
    IVP_DOUBLE get_determinante() const {
        IVP_U_Point cross;
        cross.inline_calc_cross_product(&rows[1], &rows[2]);
        return rows[0].dot_product(&cross);
    }
    IVP_RETURN_TYPE real_invert(IVP_DOUBLE epsilon = 1.0e-10) {
        IVP_U_Point column0;
        IVP_U_Point column1;
        IVP_U_Point column2;
        column0.inline_calc_cross_product(&rows[1], &rows[2]);
        column1.inline_calc_cross_product(&rows[2], &rows[0]);
        column2.inline_calc_cross_product(&rows[0], &rows[1]);
        const IVP_DOUBLE determinant = rows[0].dot_product(&column0);
        if (std::fabs(determinant) < epsilon)
            return IVP_FAULT;
        const IVP_DOUBLE inverse = 1.0 / determinant;
        column0.mult(inverse);
        column1.mult(inverse);
        column2.mult(inverse);
        set_col(IVP_INDEX_X, &column0);
        set_col(IVP_INDEX_Y, &column1);
        set_col(IVP_INDEX_Z, &column2);
        return IVP_OK;
    }
    int calc_eigen_vector(IVP_DOUBLE eigenValue,
                          IVP_U_Point *eigenVectorOut) {
        IVP_U_Point equationRows[3] = {rows[0], rows[1], rows[2]};
        equationRows[0].k[0] -= eigenValue;
        equationRows[1].k[1] -= eigenValue;
        equationRows[2].k[2] -= eigenValue;

        IVP_U_Point candidates[3];
        candidates[0].inline_calc_cross_product(&equationRows[0],
                                                &equationRows[1]);
        candidates[1].inline_calc_cross_product(&equationRows[0],
                                                &equationRows[2]);
        candidates[2].inline_calc_cross_product(&equationRows[1],
                                                &equationRows[2]);
        int best = 0;
        if (candidates[1].quad_length() > candidates[best].quad_length())
            best = 1;
        if (candidates[2].quad_length() > candidates[best].quad_length())
            best = 2;
        if (candidates[best].quad_length() > 1.0e-24) {
            eigenVectorOut->set(&candidates[best]);
            eigenVectorOut->mult(1.0 / eigenVectorOut->real_length());
            return 1;
        }

        int longest = 0;
        if (equationRows[1].quad_length() >
            equationRows[longest].quad_length())
            longest = 1;
        if (equationRows[2].quad_length() >
            equationRows[longest].quad_length())
            longest = 2;
        const IVP_U_Point &axis = equationRows[longest];
        if (axis.quad_length() <= 1.0e-24) {
            eigenVectorOut->set(0.0, 0.0, 1.0);
            return 3;
        }
        if (std::fabs(axis.k[0]) <= std::fabs(axis.k[1]) &&
            std::fabs(axis.k[0]) <= std::fabs(axis.k[2]))
            eigenVectorOut->set(0.0, -axis.k[2], axis.k[1]);
        else if (std::fabs(axis.k[1]) <= std::fabs(axis.k[2]))
            eigenVectorOut->set(-axis.k[2], 0.0, axis.k[0]);
        else
            eigenVectorOut->set(-axis.k[1], axis.k[0], 0.0);
        eigenVectorOut->mult(1.0 / eigenVectorOut->real_length());
        return 2;
    }
    IVP_RETURN_TYPE get_angles(IVP_FLOAT *alpha, IVP_FLOAT *beta,
                               IVP_FLOAT *gamma) {
        IVP_U_Matrix3 normalized = *this;
        if (normalized.orthonormize() == IVP_FAULT)
            return IVP_FAULT;
        const IVP_DOUBLE sinBeta = std::clamp(
            -normalized.get_elem(1, 2), -1.0, 1.0);
        const IVP_DOUBLE betaValue = std::asin(sinBeta);
        const IVP_DOUBLE cosBeta = std::cos(betaValue);
        IVP_DOUBLE alphaValue = 0.0;
        IVP_DOUBLE gammaValue = 0.0;
        if (std::fabs(cosBeta) > 1.0e-10) {
            alphaValue = std::atan2(normalized.get_elem(1, 0),
                                    normalized.get_elem(1, 1));
            gammaValue = std::atan2(normalized.get_elem(0, 2),
                                    normalized.get_elem(2, 2));
        } else {
            gammaValue = std::atan2(-normalized.get_elem(2, 0),
                                    normalized.get_elem(0, 0));
        }
        *alpha = static_cast<IVP_FLOAT>(alphaValue);
        *beta = static_cast<IVP_FLOAT>(betaValue);
        *gamma = static_cast<IVP_FLOAT>(gammaValue);
        return IVP_OK;
    }
    void byte_swap() {
        rows[0].byte_swap();
        rows[1].byte_swap();
        rows[2].byte_swap();
    }
};

struct alignas(8) IVP_U_Matrix : IVP_U_Matrix3 {
    IVP_U_Point vv;

    void set_identity() {
        IVP_U_Matrix3::set_identity();
        vv.set_to_zero();
    }
    void init() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixInitialize, this);
    }
    IVP_U_Point *get_position() { return &vv; }
    const IVP_U_Point *get_position() const { return &vv; }
    void inline_vmult4(const IVP_U_Point *input, IVP_U_Point *output) const {
        inline_vmult3(input, output);
        output->add(&vv);
    }
    void inline_vmult4(const IVP_U_Float_Point *input,
                       IVP_U_Point *output) const {
        IVP_U_Point source(*input);
        inline_vmult4(&source, output);
    }
    void inline_vmult4(const IVP_U_Float_Point *input,
                       IVP_U_Float_Point *output) const {
        IVP_U_Float_Point value;
        inline_vmult3(input, &value);
        value.k[0] += static_cast<IVP_FLOAT>(vv.k[0]);
        value.k[1] += static_cast<IVP_FLOAT>(vv.k[1]);
        value.k[2] += static_cast<IVP_FLOAT>(vv.k[2]);
        output->set(&value);
    }
    void inline_vimult4(const IVP_U_Point *input, IVP_U_Point *output) const {
        IVP_U_Point shifted;
        shifted.subtract(input, &vv);
        inline_vimult3(&shifted, output);
    }
    void inline_vimult4(const IVP_U_Point *input,
                        IVP_U_Float_Point *output) const {
        IVP_U_Point value;
        inline_vimult4(input, &value);
        output->set(&value);
    }
    void inline_vimult4(const IVP_U_Float_Point *input,
                        IVP_U_Float_Point *output) const {
        IVP_U_Point shifted(input->k[0] - vv.k[0],
                            input->k[1] - vv.k[1],
                            input->k[2] - vv.k[2]);
        IVP_U_Point value;
        inline_vimult3(&shifted, &value);
        output->set(&value);
    }
    IVP_RETURN_TYPE real_invert(IVP_DOUBLE epsilon = 1.0e-10) {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::MatrixInvert, this, epsilon);
    }
    void mmult4(const IVP_U_Matrix *right, IVP_U_Matrix *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixMultiply,
            const_cast<IVP_U_Matrix *>(this), right, output);
    }
    void mimult4(const IVP_U_Matrix *right, IVP_U_Matrix *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixInverseMultiply,
            const_cast<IVP_U_Matrix *>(this), right, output);
    }
    void mi2mult4(const IVP_U_Matrix *right, IVP_U_Matrix *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixMultiplyInverse,
            const_cast<IVP_U_Matrix *>(this), right, output);
    }
    void set_transpose(const IVP_U_Matrix *source) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixSetTranspose, this, source);
    }
    void vimult4(const IVP_U_Point *input,
                 IVP_U_Float_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixInverseTransformPointToFloat,
            const_cast<IVP_U_Matrix *>(this), input, output);
    }
    void vimult4(const IVP_U_Point *input, IVP_U_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixInverseTransformPoint,
            const_cast<IVP_U_Matrix *>(this), input, output);
    }
    void vimult4(const IVP_U_Float_Point *input,
                 IVP_U_Float_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixInverseTransformFloatPoint,
            const_cast<IVP_U_Matrix *>(this), input, output);
    }
    void vmult4(const IVP_U_Float_Point *input,
                IVP_U_Float_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixTransformFloatPoint,
            const_cast<IVP_U_Matrix *>(this), input, output);
    }
    void vmult4(const IVP_U_Point *input, IVP_U_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixTransformPoint,
            const_cast<IVP_U_Matrix *>(this), input, output);
    }
    void vmult4(const IVP_U_Float_Point *input, IVP_U_Point *output) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixTransformFloatPointToPoint,
            const_cast<IVP_U_Matrix *>(this), input, output);
    }
    void shift_os(const IVP_U_Point *shift) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MatrixShiftObject, this, shift);
    }
    void inline_mmult4(const IVP_U_Matrix *right,
                       IVP_U_Matrix *output) const {
        IVP_U_Matrix result;
        inline_mmult3(right, &result);
        inline_vmult3(&right->vv, &result.vv);
        result.vv.add(&vv);
        *output = result;
    }
    void inline_mimult4(const IVP_U_Matrix *right,
                        IVP_U_Matrix *output) const {
        IVP_U_Matrix result;
        inline_mimult3(right, &result);
        IVP_U_Point difference;
        difference.subtract(&right->vv, &vv);
        inline_vimult3(&difference, &result.vv);
        *output = result;
    }
    void init_rows4(const IVP_U_Point *row0, const IVP_U_Point *row1,
                    const IVP_U_Point *row2, const IVP_U_Point *shift) {
        init_rows3(row0, row1, row2);
        if (shift)
            vv.set(shift);
        else
            vv.set_to_zero();
    }
    void init_columns4(const IVP_U_Point *column0,
                       const IVP_U_Point *column1,
                       const IVP_U_Point *column2,
                       const IVP_U_Point *shift) {
        init_columns3(column0, column1, column2);
        if (shift)
            vv.set(shift);
        else
            vv.set_to_zero();
    }
    void init_rot_multiple(const IVP_U_Point *angles,
                           IVP_DOUBLE factor = 1.0) {
        const IVP_DOUBLE x = std::cos(angles->k[0] * factor);
        const IVP_DOUBLE y = std::sin(angles->k[0] * factor);
        const IVP_DOUBLE u = std::cos(angles->k[1] * factor);
        const IVP_DOUBLE w = std::sin(angles->k[1] * factor);
        const IVP_DOUBLE c = std::cos(angles->k[2] * factor);
        const IVP_DOUBLE s = std::sin(angles->k[2] * factor);
        vv.set_to_zero();
        set_elem(0, 0, u * c + w * y * s);
        set_elem(0, 1, w * y * c - u * s);
        set_elem(0, 2, w * x);
        set_elem(1, 0, x * s);
        set_elem(1, 1, x * c);
        set_elem(1, 2, -y);
        set_elem(2, 0, y * u * s - w * c);
        set_elem(2, 1, u * y * c + w * s);
        set_elem(2, 2, u * x);
    }
    void interpolate(const IVP_U_Matrix *from, const IVP_U_Matrix *to,
                     IVP_DOUBLE factor) {
        rows[0].set_interpolate(&from->rows[0], &to->rows[0], factor);
        rows[1].set_interpolate(&from->rows[1], &to->rows[1], factor);
        rows[2].set_interpolate(&from->rows[2], &to->rows[2], factor);
        vv.set_interpolate(&from->vv, &to->vv, factor);
    }
    void rotate(IVP_COORDINATE_INDEX axis, IVP_FLOAT angle,
                IVP_U_Matrix *output) {
        IVP_U_Matrix rotation;
        rotation.init_rotated3(axis, angle);
        rotation.vv.set_to_zero();
        rotation.inline_mmult4(this, output);
    }
    void rotate_invers(IVP_COORDINATE_INDEX axis, IVP_FLOAT angle,
                       IVP_U_Matrix *output) {
        static constexpr int indices[5] = {1, 2, 0, 1, 2};
        const int *component = &indices[axis];
        IVP_U_Matrix rotation;
        rotation.set_identity();
        const IVP_DOUBLE sine = std::sin(angle);
        const IVP_DOUBLE cosine = std::cos(angle);
        rotation.set_elem(component[0], component[0], cosine);
        rotation.set_elem(component[0], component[1], sine);
        rotation.set_elem(component[1], component[0], -sine);
        rotation.set_elem(component[1], component[1], cosine);
        mi2mult4(&rotation, output);
    }
    void transpose() {
        transpose3();
        IVP_U_Point transformed;
        inline_vmult3(&vv, &transformed);
        vv.set_negative(&transformed);
    }
    void get_4x4_column_major(IVP_FLOAT *output) const {
        output[0] = static_cast<IVP_FLOAT>(get_elem(0, 0));
        output[1] = static_cast<IVP_FLOAT>(get_elem(1, 0));
        output[2] = static_cast<IVP_FLOAT>(get_elem(2, 0));
        output[3] = 0.0f;
        output[4] = static_cast<IVP_FLOAT>(get_elem(0, 1));
        output[5] = static_cast<IVP_FLOAT>(get_elem(1, 1));
        output[6] = static_cast<IVP_FLOAT>(get_elem(2, 1));
        output[7] = 0.0f;
        output[8] = static_cast<IVP_FLOAT>(get_elem(0, 2));
        output[9] = static_cast<IVP_FLOAT>(get_elem(1, 2));
        output[10] = static_cast<IVP_FLOAT>(get_elem(2, 2));
        output[11] = 0.0f;
        output[12] = static_cast<IVP_FLOAT>(vv.k[0]);
        output[13] = static_cast<IVP_FLOAT>(vv.k[1]);
        output[14] = static_cast<IVP_FLOAT>(vv.k[2]);
        output[15] = 1.0f;
    }
    IVP_RETURN_TYPE real_invert(const IVP_U_Matrix *source,
                                IVP_DOUBLE epsilon = 1.0e-10) {
        set_matrix(source);
        return real_invert(epsilon);
    }
    void shift_ws(const IVP_U_Point *shift) { vv.add(shift); }
    IVP_DOUBLE quad_distance_to(const IVP_U_Matrix *other) {
        return quad_rot_distance_to(other) + vv.quad_distance_to(&other->vv);
    }
    void set_matrix(const IVP_U_Matrix *source) { *this = *source; }
    void print(const char *headline = nullptr) {
        if (headline)
            std::printf("%s\n", headline);
        std::printf("mm\t%g %g %g\n\t%g %g %g\n\t%g %g %g\n",
                    get_elem(0, 0), get_elem(0, 1), get_elem(0, 2),
                    get_elem(1, 0), get_elem(1, 1), get_elem(1, 2),
                    get_elem(2, 0), get_elem(2, 1), get_elem(2, 2));
        std::printf("vv\t%g %g %g\n", vv.k[0], vv.k[1], vv.k[2]);
    }
    IVP_ERROR_STRING write_to_file(FILE *stream,
                                   const char *key = nullptr) {
        if (!stream)
            return "ERROR in write Matrix";
        std::fprintf(stream, "\t\t%s\n", key ? key : "MATRIX_START");
        std::fprintf(stream,
                     "\t\t\tMATRIX_ROT %g %g %g %g %g %g %g %g %g\n",
                     get_elem(0, 0), get_elem(0, 1), get_elem(0, 2),
                     get_elem(1, 0), get_elem(1, 1), get_elem(1, 2),
                     get_elem(2, 0), get_elem(2, 1), get_elem(2, 2));
        std::fprintf(stream, "\t\t\tMATRIX_POS %g %g %g\n",
                     vv.k[0], vv.k[1], vv.k[2]);
        std::fprintf(stream, "\t\tMATRIX_END\n");
        return nullptr;
    }
    IVP_ERROR_STRING read_from_file(FILE *stream) {
        if (!stream)
            return "ERROR in read Matrix";
        char command[64]{};
        while (std::fscanf(stream, "%63s", command) == 1) {
            if (std::strcmp(command, "MATRIX_START") == 0)
                continue;
            if (std::strcmp(command, "MATRIX_ROT") == 0) {
                IVP_DOUBLE value = 0.0;
                for (int row = 0; row < 3; ++row) {
                    for (int column = 0; column < 3; ++column) {
                        if (std::fscanf(stream, "%lf", &value) != 1)
                            return "ERROR in read Matrix";
                        set_elem(row, column, value);
                    }
                }
                continue;
            }
            if (std::strcmp(command, "MATRIX_POS") == 0) {
                if (std::fscanf(stream, "%lf %lf %lf", &vv.k[0], &vv.k[1],
                                &vv.k[2]) != 3)
                    return "ERROR in read Matrix";
                continue;
            }
            if (std::strcmp(command, "MATRIX_END") == 0)
                return nullptr;
            return "ERROR in read Matrix";
        }
        return nullptr;
    }
    void byte_swap() {
        vv.byte_swap();
        IVP_U_Matrix3::byte_swap();
    }
};

enum IVP_MATRIX_CATEGORY : std::int32_t {
    IVP_MC_UNDEFINED = 0,
    IVP_MC_RIGHT_HANDED = 1,
    IVP_MC_LEFT_HANDED = 2,
};

struct IVP_U_Mapping {
    char k[3];

    IVP_U_Mapping() {
        k[0] = static_cast<char>(IVP_INDEX_X);
        k[1] = static_cast<char>(IVP_INDEX_Y);
        k[2] = static_cast<char>(IVP_INDEX_Z);
    }
    IVP_COORDINATE_INDEX operator[](IVP_COORDINATE_INDEX index) {
        return static_cast<IVP_COORDINATE_INDEX>(k[index]);
    }
    void vapply(IVP_U_Point *input, IVP_U_Point *output) {
        const IVP_U_Point source = *input;
        output->k[0] = source.k[static_cast<int>(k[0])];
        output->k[1] = source.k[static_cast<int>(k[1])];
        output->k[2] = source.k[static_cast<int>(k[2])];
    }
    void viapply(IVP_U_Point *input, IVP_U_Point *output) {
        const IVP_U_Point source = *input;
        output->k[static_cast<int>(k[0])] = source.k[0];
        output->k[static_cast<int>(k[1])] = source.k[1];
        output->k[static_cast<int>(k[2])] = source.k[2];
    }
    void mapply(IVP_U_Matrix3 *input, IVP_U_Matrix3 *output) {
        const IVP_U_Matrix3 source = *input;
        IVP_U_Point row;
        source.get_row(static_cast<IVP_COORDINATE_INDEX>(k[0]), &row);
        output->set_row(IVP_INDEX_X, &row);
        source.get_row(static_cast<IVP_COORDINATE_INDEX>(k[1]), &row);
        output->set_row(IVP_INDEX_Y, &row);
        source.get_row(static_cast<IVP_COORDINATE_INDEX>(k[2]), &row);
        output->set_row(IVP_INDEX_Z, &row);
    }
    void miapply(IVP_U_Matrix3 *input, IVP_U_Matrix3 *output) {
        const IVP_U_Matrix3 source = *input;
        IVP_U_Point row;
        source.get_row(IVP_INDEX_X, &row);
        output->set_row(static_cast<IVP_COORDINATE_INDEX>(k[0]), &row);
        source.get_row(IVP_INDEX_Y, &row);
        output->set_row(static_cast<IVP_COORDINATE_INDEX>(k[1]), &row);
        source.get_row(IVP_INDEX_Z, &row);
        output->set_row(static_cast<IVP_COORDINATE_INDEX>(k[2]), &row);
    }
    void mi2apply(IVP_U_Matrix3 *input, IVP_U_Matrix3 *output) {
        const IVP_U_Matrix3 source = *input;
        IVP_U_Point column;
        source.get_col(static_cast<IVP_COORDINATE_INDEX>(k[0]), &column);
        output->set_col(IVP_INDEX_X, &column);
        source.get_col(static_cast<IVP_COORDINATE_INDEX>(k[1]), &column);
        output->set_col(IVP_INDEX_Y, &column);
        source.get_col(static_cast<IVP_COORDINATE_INDEX>(k[2]), &column);
        output->set_col(IVP_INDEX_Z, &column);
    }
    IVP_MATRIX_CATEGORY mapping_type() {
        bool used[3] = {false, false, false};
        int inversions = 0;
        for (int i = 0; i < 3; ++i) {
            const int value = static_cast<int>(k[i]);
            if (value < 0 || value >= 3 || used[value])
                return IVP_MC_UNDEFINED;
            used[value] = true;
            for (int j = 0; j < i; ++j) {
                if (static_cast<int>(k[j]) > value)
                    ++inversions;
            }
        }
        return (inversions & 1) ? IVP_MC_LEFT_HANDED
                                : IVP_MC_RIGHT_HANDED;
    }
};

struct IVP_U_Float_Quat {
    IVP_FLOAT x;
    IVP_FLOAT y;
    IVP_FLOAT z;
    IVP_FLOAT w;

    void set(const IVP_U_Quat *source);
    void byte_swap();
};

struct alignas(8) IVP_U_Quat {
    IVP_DOUBLE x;
    IVP_DOUBLE y;
    IVP_DOUBLE z;
    IVP_DOUBLE w;

    IVP_U_Quat() = default;
    IVP_U_Quat(const IVP_U_Point &rotation) {
        set_fast_multiple(&rotation, 1.0);
    }
    IVP_U_Quat(const IVP_U_Matrix3 *matrix) { set_quaternion(matrix); }

    void set_identity() { x = y = z = 0.0; w = 1.0; }
    void init() { set_identity(); }
    void set(IVP_DOUBLE nx, IVP_DOUBLE ny, IVP_DOUBLE nz, IVP_DOUBLE nw) {
        x = nx; y = ny; z = nz; w = nw;
    }
    void set(IVP_DOUBLE rotationX, IVP_DOUBLE rotationY,
             IVP_DOUBLE rotationZ) {
        x = std::sin(rotationX * 0.5);
        y = std::sin(rotationY * 0.5);
        z = std::sin(rotationZ * 0.5);
        w = std::sqrt(1.0 - (x * x + y * y + z * z));
    }
    void set_fast_multiple(const IVP_U_Point *angles, IVP_DOUBLE factor) {
        const IVP_DOUBLE halfFactor = factor * 0.5;
        x = std::sin(angles->k[0] * halfFactor);
        y = std::sin(angles->k[1] * halfFactor);
        z = std::sin(angles->k[2] * halfFactor);
        const IVP_DOUBLE squared = std::min(1.0, x * x + y * y + z * z);
        w = std::sqrt(1.0 - squared);
    }
    void set_fast_multiple_with_clip(const IVP_U_Float_Point *angles,
                                     IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::QuaternionSetFastMultipleWithClip,
            this, angles, factor);
    }
    void set_very_fast_multiple(const IVP_U_Float_Point *angles,
                                IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::QuaternionSetVeryFastMultiple,
            this, angles, factor);
    }
    void set_matrix(IVP_U_Matrix3 *matrix) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::QuaternionWriteMatrix,
            const_cast<IVP_U_Quat *>(this), matrix);
    }
    void set_matrix(IVP_DOUBLE output[4][4]) const {
        const IVP_DOUBLE x2 = x + x;
        const IVP_DOUBLE y2 = y + y;
        const IVP_DOUBLE z2 = z + z;
        const IVP_DOUBLE xx = x * x2;
        const IVP_DOUBLE xy = x * y2;
        const IVP_DOUBLE xz = x * z2;
        const IVP_DOUBLE yy = y * y2;
        const IVP_DOUBLE yz = y * z2;
        const IVP_DOUBLE zz = z * z2;
        const IVP_DOUBLE wx = w * x2;
        const IVP_DOUBLE wy = w * y2;
        const IVP_DOUBLE wz = w * z2;
        output[0][0] = 1.0 - (yy + zz);
        output[0][1] = xy - wz;
        output[0][2] = xz + wy;
        output[0][3] = 0.0;
        output[1][0] = xy + wz;
        output[1][1] = 1.0 - (xx + zz);
        output[1][2] = yz - wx;
        output[1][3] = 0.0;
        output[2][0] = xz - wy;
        output[2][1] = yz + wx;
        output[2][2] = 1.0 - (xx + yy);
        output[2][3] = 0.0;
        output[3][0] = output[3][1] = output[3][2] = 0.0;
        output[3][3] = 1.0;
    }
    void set_quaternion(const IVP_U_Matrix3 *matrix) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::QuaternionSetFromMatrix, this, matrix);
    }
    void set_quaternion(const IVP_DOUBLE input[4][4]) {
        const IVP_DOUBLE trace = input[0][0] + input[1][1] + input[2][2];
        if (trace > 0.0) {
            IVP_DOUBLE scale = std::sqrt(trace + 1.0);
            w = scale * 0.5;
            scale = 0.5 / scale;
            x = (input[1][2] - input[2][1]) * scale;
            y = (input[2][0] - input[0][2]) * scale;
            z = (input[0][1] - input[1][0]) * scale;
            return;
        }
        static constexpr int next[3] = {1, 2, 0};
        int i = 0;
        if (input[1][1] > input[0][0])
            i = 1;
        if (input[2][2] > input[i][i])
            i = 2;
        const int j = next[i];
        const int k = next[j];
        IVP_DOUBLE values[4]{};
        IVP_DOUBLE scale = std::sqrt(
            input[i][i] - input[j][j] - input[k][k] + 1.0);
        values[i] = scale * 0.5;
        if (scale != 0.0)
            scale = 0.5 / scale;
        values[3] = (input[j][k] - input[k][j]) * scale;
        values[j] = (input[i][j] + input[j][i]) * scale;
        values[k] = (input[i][k] + input[k][i]) * scale;
        x = values[0];
        y = values[1];
        z = values[2];
        w = values[3];
    }
    void set_interpolate_smoothly(const IVP_U_Quat *from,
                                  const IVP_U_Quat *to, IVP_DOUBLE factor) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::QuaternionInterpolateSmoothly,
            this, from, to, factor);
    }
    void set_interpolate_linear(const IVP_U_Quat *from,
                                const IVP_U_Quat *to,
                                IVP_DOUBLE factor) {
        const IVP_DOUBLE sign = acos_quat_pair(from, to) < 0.0 ? -1.0 : 1.0;
        const IVP_DOUBLE inverse = 1.0 - factor;
        x = inverse * from->x + factor * sign * to->x;
        y = inverse * from->y + factor * sign * to->y;
        z = inverse * from->z + factor * sign * to->z;
        w = inverse * from->w + factor * sign * to->w;
    }
    void normize_quat() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::QuaternionNormalize, this);
    }
    void fast_normize_quat() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::QuaternionFastNormalize, this);
    }
    void normize_correct_step(int steps) {
        IVP_DOUBLE square = (x * x + y * y + z * z + w * w) * 0.5;
        IVP_DOUBLE factor = 1.5 - square;
        for (int step = 1; step < steps; ++step)
            factor += 0.5 - factor * factor * square;
        x *= factor;
        y *= factor;
        z *= factor;
        w *= factor;
    }
    void invert_quat() {
        const IVP_DOUBLE inverseNorm =
            1.0 / (x * x + y * y + z * z + w * w);
        x = -x * inverseNorm;
        y = -y * inverseNorm;
        z = -z * inverseNorm;
        w = w * inverseNorm;
    }
    void get_angles(IVP_U_Float_Point *angles) {
        angles->k[0] = static_cast<IVP_FLOAT>(2.0 * std::asin(x));
        angles->k[1] = static_cast<IVP_FLOAT>(2.0 * std::asin(y));
        angles->k[2] = static_cast<IVP_FLOAT>(2.0 * std::asin(z));
    }
    void set_from_rotation_vectors(IVP_DOUBLE x1, IVP_DOUBLE y1,
                                   IVP_DOUBLE z1, IVP_DOUBLE x2,
                                   IVP_DOUBLE y2, IVP_DOUBLE z2) {
        const IVP_DOUBLE cosine = x1 * x2 + y1 * y2 + z1 * z2;
        if (cosine > 0.99999) {
            init();
            return;
        }
        if (cosine < -0.99999) {
            IVP_DOUBLE axisX = 0.0;
            IVP_DOUBLE axisY = x1;
            IVP_DOUBLE axisZ = -y1;
            if (std::sqrt(axisY * axisY + axisZ * axisZ) < 1.0e-6) {
                axisX = -z1;
                axisY = 0.0;
                axisZ = x1;
            }
            const IVP_DOUBLE inverse =
                1.0 / std::sqrt(axisX * axisX + axisY * axisY +
                                axisZ * axisZ);
            x = axisX * inverse;
            y = axisY * inverse;
            z = axisZ * inverse;
            w = 0.0;
            return;
        }
        IVP_DOUBLE axisX = y1 * z2 - z1 * y2;
        IVP_DOUBLE axisY = z1 * x2 - x1 * z2;
        IVP_DOUBLE axisZ = x1 * y2 - y1 * x2;
        const IVP_DOUBLE inverse =
            1.0 / std::sqrt(axisX * axisX + axisY * axisY + axisZ * axisZ);
        const IVP_DOUBLE sineHalf = std::sqrt(0.5 * (1.0 - cosine));
        x = axisX * inverse * sineHalf;
        y = axisY * inverse * sineHalf;
        z = axisZ * inverse * sineHalf;
        w = std::sqrt(0.5 * (1.0 + cosine));
    }
    void inline_set_mult_quat(const IVP_U_Quat *left,
                              const IVP_U_Quat *right) {
        const IVP_DOUBLE newX = left->w * right->x + left->x * right->w +
                                left->y * right->z - left->z * right->y;
        const IVP_DOUBLE newY = left->w * right->y + left->y * right->w +
                                left->z * right->x - left->x * right->z;
        const IVP_DOUBLE newZ = left->w * right->z + left->z * right->w +
                                left->x * right->y - left->y * right->x;
        const IVP_DOUBLE newW = left->w * right->w - left->x * right->x -
                                left->y * right->y - left->z * right->z;
        set(newX, newY, newZ, newW);
    }
    void inline_set_mult_quat(const IVP_U_Quat *left,
                              const IVP_U_Float_Quat *right) {
        const IVP_DOUBLE newX = left->w * right->x + left->x * right->w +
                                left->y * right->z - left->z * right->y;
        const IVP_DOUBLE newY = left->w * right->y + left->y * right->w +
                                left->z * right->x - left->x * right->z;
        const IVP_DOUBLE newZ = left->w * right->z + left->z * right->w +
                                left->x * right->y - left->y * right->x;
        const IVP_DOUBLE newW = left->w * right->w - left->x * right->x -
                                left->y * right->y - left->z * right->z;
        set(newX, newY, newZ, newW);
    }
    IVP_DOUBLE acos_quat(const IVP_U_Quat *other) const {
        return x * other->x + y * other->y + z * other->z + w * other->w;
    }
    void set_div_unit_quat(const IVP_U_Quat *left,
                           const IVP_U_Quat *right) {
        IVP_U_Quat inverse;
        inverse.set_invert_unit_quat(right);
        inline_set_mult_quat(left, &inverse);
    }
    void set_invert_unit_quat(const IVP_U_Quat *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::QuaternionSetInverseUnit, this, value);
    }
    void set_invert_mult(const IVP_U_Quat *left, const IVP_U_Quat *right) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::QuaternionSetInverseMultiply,
            this, left, right);
    }
    void set_mult_quat(const IVP_U_Quat *left, const IVP_U_Quat *right) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::QuaternionSetMultiply, this, left, right);
    }
    IVP_DOUBLE inline_estimate_q_diff_to(
        const IVP_U_Float_Quat *reference) const {
        const IVP_DOUBLE cosineHalf = x * reference->x + y * reference->y +
                                      z * reference->z + w * reference->w;
        return (1.0 - cosineHalf * cosineHalf) * 2.0;
    }
    void byte_swap() {
        BML::IVP::Detail::ByteSwapDouble(x);
        BML::IVP::Detail::ByteSwapDouble(y);
        BML::IVP::Detail::ByteSwapDouble(z);
        BML::IVP::Detail::ByteSwapDouble(w);
    }

private:
    static IVP_DOUBLE acos_quat_pair(const IVP_U_Quat *left,
                                     const IVP_U_Quat *right) {
        return left->x * right->x + left->y * right->y +
               left->z * right->z + left->w * right->w;
    }
};

inline void IVP_U_Point::set(const IVP_U_Quat *source) {
    set(source->x, source->y, source->z);
}

inline void IVP_U_Point::set_multiple(const IVP_U_Quat *source,
                                      IVP_DOUBLE factor) {
    set(source);
    mult(factor);
}

inline void IVP_U_Float_Point::set_multiple(const IVP_U_Quat *source,
                                            IVP_DOUBLE factor) {
    set(static_cast<IVP_FLOAT>(source->x * factor),
        static_cast<IVP_FLOAT>(source->y * factor),
        static_cast<IVP_FLOAT>(source->z * factor));
}

inline void IVP_U_Float_Quat::set(const IVP_U_Quat *source) {
    x = static_cast<IVP_FLOAT>(source->x);
    y = static_cast<IVP_FLOAT>(source->y);
    z = static_cast<IVP_FLOAT>(source->z);
    w = static_cast<IVP_FLOAT>(source->w);
}

inline void IVP_U_Float_Quat::byte_swap() {
    BML::IVP::Detail::ByteSwapFloat(x);
    BML::IVP::Detail::ByteSwapFloat(y);
    BML::IVP::Detail::ByteSwapFloat(z);
    BML::IVP::Detail::ByteSwapFloat(w);
}

struct IVP_U_Vector_Base {
    std::uint16_t memsize;
    std::uint16_t n_elems;
    void **elems;

    void increment_mem() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::VectorIncrementMemory, this);
    }
};

template <class T>
class IVP_U_Vector : public IVP_U_Vector_Base {
protected:
    IVP_U_Vector(void **storage, int size) {
        elems = storage;
        memsize = static_cast<std::uint16_t>(size);
        n_elems = 0;
    }

public:
    explicit IVP_U_Vector(int size = 0) {
        memsize = static_cast<std::uint16_t>(size);
        n_elems = 0;
        if (size != 0) {
            elems = static_cast<void **>(BML::IVP::ABI::Invoke<void *>(
                BML::IVP::ABI::Address::Allocate,
                static_cast<unsigned int>(size) * sizeof(void *)));
        } else {
            elems = nullptr;
        }
    }
    ~IVP_U_Vector() { clear(); }

    void ensure_capacity() {
        if (n_elems >= memsize)
            increment_mem();
    }
    void clear() {
        if (elems != reinterpret_cast<void **>(this + 1)) {
            if (elems) {
                BML::IVP::ABI::Invoke<void>(
                    BML::IVP::ABI::Address::Free, elems);
            }
            elems = nullptr;
            memsize = 0;
        }
        n_elems = 0;
    }
    void remove_all() { n_elems = 0; }
    int len() const { return n_elems; }
    T *element_at(int index) const { return static_cast<T *>(elems[index]); }
    int index_of(T *element) {
        for (int index = static_cast<int>(n_elems) - 1; index >= 0; --index) {
            if (elems[index] == element)
                return index;
        }
        return -1;
    }
    int add(T *element) {
        ensure_capacity();
        elems[n_elems] = element;
        return n_elems++;
    }
    int install(T *element) {
        const int current = index_of(element);
        return current >= 0 ? current : add(element);
    }
    void remove_at(int index) {
        for (int current = index; current + 1 < n_elems; ++current)
            elems[current] = elems[current + 1];
        --n_elems;
    }
    void remove_at_and_allow_resort(int index) {
        --n_elems;
        elems[index] = elems[n_elems];
    }
    void swap_elems(int first, int second) {
        std::swap(elems[first], elems[second]);
    }
    void insert_after(int index, T *element) {
        ++index;
        ensure_capacity();
        for (int current = n_elems; current > index; --current)
            elems[current] = elems[current - 1];
        elems[index] = element;
        ++n_elems;
    }
    void reverse() {
        for (int index = 0; index < n_elems / 2; ++index)
            swap_elems(index, n_elems - 1 - index);
    }
    void remove(T *element) { remove_at(index_of(element)); }
    void remove_allow_resort(T *element) {
        remove_at_and_allow_resort(index_of(element));
    }
};

template <class T>
class IVP_U_FVector : public IVP_U_Vector_Base {
protected:
    IVP_U_FVector(void **storage, int size) {
        elems = storage;
        memsize = static_cast<std::uint16_t>(size);
        n_elems = 0;
    }

public:
    explicit IVP_U_FVector(int size = 0) {
        memsize = static_cast<std::uint16_t>(size);
        n_elems = 0;
        if (size != 0) {
            elems = static_cast<void **>(BML::IVP::ABI::Invoke<void *>(
                BML::IVP::ABI::Address::Allocate,
                static_cast<unsigned int>(size) * sizeof(void *)));
        } else {
            elems = nullptr;
        }
    }
    ~IVP_U_FVector() { clear(); }

    void clear() {
        if (elems != reinterpret_cast<void **>(this + 1)) {
            if (elems) {
                BML::IVP::ABI::Invoke<void>(
                    BML::IVP::ABI::Address::Free, elems);
            }
            elems = nullptr;
            memsize = 0;
        }
        n_elems = 0;
    }
    void remove_all() { n_elems = 0; }
    int len() const { return n_elems; }
    T *element_at(int index) const { return static_cast<T *>(elems[index]); }
    int index_of(T *element) {
        const int first = element->get_fvector_index(0);
        if (first >= 0 && first < n_elems && element_at(first) == element)
            return first;
        const int second = element->get_fvector_index(1);
        return second >= 0 && second < n_elems &&
                       element_at(second) == element
                   ? second
                   : -1;
    }
    int add(T *element) {
        if (n_elems >= memsize)
            increment_mem();
        elems[n_elems] = element;
        element->set_fvector_index(-1, n_elems);
        return n_elems++;
    }
    void swap_elems(int first, int second) {
        T *firstElement = element_at(first);
        T *secondElement = element_at(second);
        elems[first] = secondElement;
        elems[second] = firstElement;
        firstElement->set_fvector_index(first, second);
        secondElement->set_fvector_index(second, first);
    }
    void remove_allow_resort(T *element) {
        const int index = index_of(element);
        if (index < 0)
            return;
        --n_elems;
        if (index < n_elems) {
            T *replacement = element_at(n_elems);
            elems[index] = replacement;
            replacement->set_fvector_index(n_elems, index);
        }
        element->set_fvector_index(index, -1);
    }
};

struct IVP_U_BigVector_Base {
    int memsize;
    int n_elems;
    void **elems;

    void increment_mem() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::BigVectorIncrementMemory, this);
    }
};

template <class T>
class IVP_U_BigVector : public IVP_U_BigVector_Base {
protected:
    IVP_U_BigVector(void **storage, int size) {
        elems = storage;
        memsize = size;
        n_elems = 0;
    }

public:
    explicit IVP_U_BigVector(int size = 0) {
        memsize = size;
        n_elems = 0;
        if (size != 0) {
            elems = static_cast<void **>(BML::IVP::ABI::Invoke<void *>(
                BML::IVP::ABI::Address::Allocate,
                static_cast<unsigned int>(size) * sizeof(void *)));
        } else {
            elems = nullptr;
        }
    }
    ~IVP_U_BigVector() { clear(); }

    void ensure_capacity() {
        if (n_elems >= memsize)
            increment_mem();
    }
    void clear() {
        if (elems != reinterpret_cast<void **>(this + 1)) {
            if (elems) {
                BML::IVP::ABI::Invoke<void>(
                    BML::IVP::ABI::Address::Free, elems);
            }
            elems = nullptr;
            memsize = 0;
        }
        n_elems = 0;
    }
    void remove_all() { n_elems = 0; }
    int len() const { return n_elems; }
    T *element_at(int index) const { return static_cast<T *>(elems[index]); }
    int index_of(T *element) {
        for (int index = n_elems - 1; index >= 0; --index) {
            if (elems[index] == element)
                return index;
        }
        return -1;
    }
    int add(T *element) {
        ensure_capacity();
        elems[n_elems] = element;
        return n_elems++;
    }
    int install(T *element) {
        const int current = index_of(element);
        return current >= 0 ? current : add(element);
    }
    void remove_at(int index) {
        for (int current = index; current + 1 < n_elems; ++current)
            elems[current] = elems[current + 1];
        --n_elems;
    }
    void remove(T *element) { remove_at(index_of(element)); }
    void exchange_vector_elems(int first, int second) {
        std::swap(elems[first], elems[second]);
    }
};

// The nearby header accidentally initializes these enumerators at zero and
// then decrements without checking for a negative index. The commented code
// and all call sites show the intended operation: walk a snapshot backwards.
template <class T>
class IVP_U_Vector_Enumerator {
public:
    explicit IVP_U_Vector_Enumerator(IVP_U_Vector<T> *vector)
        : index(vector ? vector->len() - 1 : -1) {}

    T *get_next_element(IVP_U_Vector<T> *vector) {
        if (!vector || index < 0 || index >= vector->len())
            return nullptr;
        return vector->element_at(index--);
    }

private:
    int index;
};

template <class T>
class IVP_U_BigVector_Enumerator {
public:
    explicit IVP_U_BigVector_Enumerator(IVP_U_BigVector<T> *vector)
        : index(vector ? vector->len() - 1 : -1) {}

    T *get_next_element(IVP_U_BigVector<T> *vector) {
        if (!vector || index < 0 || index >= vector->len())
            return nullptr;
        return vector->element_at(index--);
    }

private:
    int index;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(void *) == 4, "Ballance IVP is an x86 ABI");
static_assert(sizeof(IVP_Time) == 0x08);
static_assert(std::is_trivially_default_constructible_v<IVP_Time>);
static_assert(BML_IvpTimeLayoutCheck::seconds == 0x00);
static_assert(sizeof(IVP_U_Float_Point3) == 0x0C);
static_assert(std::is_trivially_default_constructible_v<IVP_U_Float_Point3>);
static_assert(sizeof(IVP_U_Float_Point) == 0x10);
static_assert(std::is_trivially_default_constructible_v<IVP_U_Float_Point>);
static_assert(offsetof(IVP_U_Float_Point, k) == 0x00);
static_assert(offsetof(IVP_U_Float_Point, hesse_val) == 0x0C);
static_assert(sizeof(IVP_U_Float_Hesse) == 0x10);
static_assert(sizeof(IVP_U_Point) == 0x20);
static_assert(std::is_trivially_default_constructible_v<IVP_U_Point>);
static_assert(offsetof(IVP_U_Point, k) == 0x00);
static_assert(offsetof(IVP_U_Point, hesse_val) == 0x18);
static_assert(sizeof(IVP_U_Matrix3) == 0x60);
static_assert(std::is_trivially_default_constructible_v<IVP_U_Matrix3>);
static_assert(offsetof(IVP_U_Matrix3, rows) == 0x00);
static_assert(sizeof(IVP_U_Matrix) == 0x80);
static_assert(std::is_trivially_default_constructible_v<IVP_U_Matrix>);
static_assert(offsetof(IVP_U_Matrix, vv) == 0x60);
static_assert(sizeof(IVP_U_Quat) == 0x20);
static_assert(std::is_trivially_default_constructible_v<IVP_U_Quat>);
static_assert(offsetof(IVP_U_Quat, x) == 0x00);
static_assert(offsetof(IVP_U_Quat, w) == 0x18);
static_assert(sizeof(IVP_U_Float_Quat) == 0x10);
static_assert(std::is_trivially_default_constructible_v<IVP_U_Float_Quat>);
static_assert(sizeof(IVP_U_Vector_Base) == 0x08);
static_assert(std::is_trivially_default_constructible_v<IVP_U_Vector_Base>);
static_assert(offsetof(IVP_U_Vector_Base, memsize) == 0x00);
static_assert(offsetof(IVP_U_Vector_Base, n_elems) == 0x02);
static_assert(offsetof(IVP_U_Vector_Base, elems) == 0x04);
static_assert(sizeof(IVP_U_BigVector_Base) == 0x0C);
static_assert(
    std::is_trivially_default_constructible_v<IVP_U_BigVector_Base>);
static_assert(offsetof(IVP_U_BigVector_Base, memsize) == 0x00);
static_assert(offsetof(IVP_U_BigVector_Base, n_elems) == 0x04);
static_assert(offsetof(IVP_U_BigVector_Base, elems) == 0x08);
static_assert(sizeof(IVP_U_Vector<IVP_U_Point>) == 0x08);
static_assert(sizeof(IVP_U_FVector<IVP_U_Point>) == 0x08);
static_assert(sizeof(IVP_U_BigVector<IVP_U_Point>) == 0x0C);
static_assert(sizeof(IVP_U_Vector_Enumerator<IVP_U_Point>) == 0x04);
static_assert(sizeof(IVP_U_BigVector_Enumerator<IVP_U_Point>) == 0x04);
static_assert(sizeof(IVP_Inline_Math) == 0x01);
#endif

#endif // BML_IVP_TYPES_H
