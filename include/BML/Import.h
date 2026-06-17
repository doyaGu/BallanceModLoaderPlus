#ifndef BML_IMPORT_H
#define BML_IMPORT_H

#include "BML/Interop.h"

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

namespace BML {

namespace Detail {

template <typename T>
struct InteropArgument;

template <>
struct InteropArgument<bool> {
    static int Set(BML_CallFrame *frame, size_t index, bool value) {
        return BML_CallFrame_SetBool(frame, index, value ? 1 : 0);
    }
};

template <>
struct InteropArgument<int> {
    static int Set(BML_CallFrame *frame, size_t index, int value) {
        return BML_CallFrame_SetInt(frame, index, value);
    }
};

template <>
struct InteropArgument<float> {
    static int Set(BML_CallFrame *frame, size_t index, float value) {
        return BML_CallFrame_SetFloat(frame, index, value);
    }
};

template <>
struct InteropArgument<const char *> {
    static int Set(BML_CallFrame *frame, size_t index, const char *value) {
        return BML_CallFrame_SetString(frame, index, value ? value : "");
    }
};

template <>
struct InteropArgument<char *> {
    static int Set(BML_CallFrame *frame, size_t index, const char *value) {
        return BML_CallFrame_SetString(frame, index, value ? value : "");
    }
};

template <>
struct InteropArgument<std::string> {
    static int Set(BML_CallFrame *frame, size_t index, const std::string &value) {
        return BML_CallFrame_SetString(frame, index, value.c_str());
    }
};

template <typename T>
struct InteropResult;

template <>
struct InteropResult<bool> {
    static int Get(BML_CallFrame *frame, bool &out) {
        int value = 0;
        const int status = BML_CallFrame_GetResultBool(frame, &value);
        if (status == BML_OK)
            out = value != 0;
        return status;
    }
};

template <>
struct InteropResult<int> {
    static int Get(BML_CallFrame *frame, int &out) {
        return BML_CallFrame_GetResultInt(frame, &out);
    }
};

template <>
struct InteropResult<float> {
    static int Get(BML_CallFrame *frame, float &out) {
        return BML_CallFrame_GetResultFloat(frame, &out);
    }
};

template <typename... Args>
struct InteropArgumentWriter;

template <>
struct InteropArgumentWriter<> {
    static int Set(BML_CallFrame *, size_t) {
        return BML_OK;
    }
};

template <typename First, typename... Rest>
struct InteropArgumentWriter<First, Rest...> {
    static int Set(BML_CallFrame *frame, size_t index, First &&first, Rest &&...rest) {
        using Decayed = typename std::decay<First>::type;
        const int status = InteropArgument<Decayed>::Set(frame, index, std::forward<First>(first));
        return status == BML_OK
                   ? InteropArgumentWriter<Rest...>::Set(frame, index + 1, std::forward<Rest>(rest)...)
                   : status;
    }
};

} // namespace Detail

template <typename Signature>
class Import;

// Import caches one reusable CallFrame for low-overhead calls. Instances are not
// thread-safe or reentrant; use one Import per concurrent caller.
template <typename... Args>
class Import<void(Args...)> {
public:
    Import() = default;

    Import(const char *modId, const char *name, const char *signature) {
        Bind(modId, name, signature);
    }

    Import(const Import &) = delete;
    Import &operator=(const Import &) = delete;

    Import(Import &&other) noexcept {
        MoveFrom(other);
    }

    Import &operator=(Import &&other) noexcept {
        if (this != &other) {
            Release();
            MoveFrom(other);
        }
        return *this;
    }

    ~Import() {
        Release();
    }

    int Bind(const char *modId, const char *name, const char *signature) {
        Release();
        m_Status = BML_FindModExportEx(modId, name, signature, &m_Handle);
        if (m_Status != BML_OK)
            return m_Status;
        m_Frame = BML_CreateCallFrame();
        if (!m_Frame) {
            Release();
            m_Status = BML_ERROR_OUT_OF_MEMORY;
        }
        return m_Status;
    }

    bool IsBound() const {
        return m_Handle && m_Frame;
    }

    int GetStatus() const {
        return m_Status;
    }

    int Invoke(Args... args) {
        if (!m_Handle || !m_Frame)
            return m_Status == BML_OK ? BML_ERROR_INTEROP_HANDLE_STALE : m_Status;
        BML_CallFrame_Clear(m_Frame);
        const int setStatus = Detail::InteropArgumentWriter<Args...>::Set(m_Frame, 0, std::forward<Args>(args)...);
        if (setStatus != BML_OK)
            return setStatus;
        m_Status = BML_CallModExport(m_Handle, m_Frame);
        if (m_Status == BML_ERROR_INTEROP_HANDLE_STALE) {
            Release();
            return BML_ERROR_INTEROP_HANDLE_STALE;
        }
        return m_Status;
    }

private:
    void Release() {
        if (m_Handle) {
            BML_ReleaseModExport(m_Handle);
            m_Handle = nullptr;
        }
        if (m_Frame) {
            BML_DestroyCallFrame(m_Frame);
            m_Frame = nullptr;
        }
        m_Status = BML_ERROR_NOT_FOUND;
    }

    void MoveFrom(Import &other) {
        m_Handle = other.m_Handle;
        m_Frame = other.m_Frame;
        m_Status = other.m_Status;
        other.m_Handle = nullptr;
        other.m_Frame = nullptr;
        other.m_Status = BML_ERROR_NOT_FOUND;
    }

    BML_ModExport *m_Handle = nullptr;
    BML_CallFrame *m_Frame = nullptr;
    int m_Status = BML_ERROR_NOT_FOUND;
};

template <typename Result, typename... Args>
class Import<Result(Args...)> {
public:
    Import() = default;

    Import(const char *modId, const char *name, const char *signature) {
        Bind(modId, name, signature);
    }

    Import(const Import &) = delete;
    Import &operator=(const Import &) = delete;

    Import(Import &&other) noexcept {
        MoveFrom(other);
    }

    Import &operator=(Import &&other) noexcept {
        if (this != &other) {
            Release();
            MoveFrom(other);
        }
        return *this;
    }

    ~Import() {
        Release();
    }

    int Bind(const char *modId, const char *name, const char *signature) {
        Release();
        m_Status = BML_FindModExportEx(modId, name, signature, &m_Handle);
        if (m_Status != BML_OK)
            return m_Status;
        m_Frame = BML_CreateCallFrame();
        if (!m_Frame) {
            Release();
            m_Status = BML_ERROR_OUT_OF_MEMORY;
        }
        return m_Status;
    }

    bool IsBound() const {
        return m_Handle && m_Frame;
    }

    int GetStatus() const {
        return m_Status;
    }

    int Invoke(Result &out, Args... args) {
        if (!m_Handle || !m_Frame)
            return m_Status == BML_OK ? BML_ERROR_INTEROP_HANDLE_STALE : m_Status;
        BML_CallFrame_Clear(m_Frame);
        const int setStatus = Detail::InteropArgumentWriter<Args...>::Set(m_Frame, 0, std::forward<Args>(args)...);
        if (setStatus != BML_OK)
            return setStatus;
        m_Status = BML_CallModExport(m_Handle, m_Frame);
        if (m_Status == BML_ERROR_INTEROP_HANDLE_STALE) {
            Release();
            return BML_ERROR_INTEROP_HANDLE_STALE;
        }
        if (m_Status != BML_OK)
            return m_Status;
        m_Status = Detail::InteropResult<Result>::Get(m_Frame, out);
        return m_Status;
    }

    Result InvokeOr(Result fallback, Args... args) {
        Result result = fallback;
        return Invoke(result, std::forward<Args>(args)...) == BML_OK ? result : fallback;
    }

private:
    void Release() {
        if (m_Handle) {
            BML_ReleaseModExport(m_Handle);
            m_Handle = nullptr;
        }
        if (m_Frame) {
            BML_DestroyCallFrame(m_Frame);
            m_Frame = nullptr;
        }
        m_Status = BML_ERROR_NOT_FOUND;
    }

    void MoveFrom(Import &other) {
        m_Handle = other.m_Handle;
        m_Frame = other.m_Frame;
        m_Status = other.m_Status;
        other.m_Handle = nullptr;
        other.m_Frame = nullptr;
        other.m_Status = BML_ERROR_NOT_FOUND;
    }

    BML_ModExport *m_Handle = nullptr;
    BML_CallFrame *m_Frame = nullptr;
    int m_Status = BML_ERROR_NOT_FOUND;
};

} // namespace BML

#endif // BML_IMPORT_H
