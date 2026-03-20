#ifndef CK_SCRIPTDATABUFFER_H
#define CK_SCRIPTDATABUFFER_H

#include <string>

#include <angelscript.h>

#include "RefCount.h"
#include "ScriptNativePointer.h"

class NativeBuffer {
public:
    static NativeBuffer *Create(size_t size) {
        void *self = asAllocMem(sizeof(NativeBuffer));
        return new(self) NativeBuffer(size);
    }

    static NativeBuffer *Create(void *buffer, size_t size) {
        void *self = asAllocMem(sizeof(NativeBuffer));
        return new(self) NativeBuffer(buffer, size);
    }

    explicit NativeBuffer(size_t size);
    NativeBuffer(void *buffer, size_t size);
    ~NativeBuffer();

    NativeBuffer(const NativeBuffer &other) = delete;
    NativeBuffer(NativeBuffer &&other) noexcept = delete;
    NativeBuffer &operator=(const NativeBuffer &) = delete;
    NativeBuffer &operator=(NativeBuffer &&) noexcept = delete;

    bool operator==(const NativeBuffer &rhs) const {
        return m_Buffer == rhs.m_Buffer && m_Size == rhs.m_Size;
    }

    bool operator!=(const NativeBuffer &rhs) const {
        return !(*this == rhs);
    }

    int AddRef() const;
    int Release() const;
    asILockableSharedBool *GetWeakRefFlag();

    char &operator[](size_t index);
    const char &operator[](size_t index) const;

    size_t Write(void *x, size_t size);
    size_t Read(void *x, size_t size);

    template <typename T>
    size_t WriteValue(T &value) { return Write(&value, sizeof(T)); }
    template <typename T>
    size_t ReadValue(T &value) { return Read(&value, sizeof(T)); }

    size_t WriteInt(int value) { return WriteValue(value); }
    size_t WriteUInt(unsigned int value) { return WriteValue(value); }
    size_t WriteFloat(float value) { return WriteValue(value); }
    size_t WriteDouble(double value) { return WriteValue(value); }
    size_t WriteShort(short value) { return WriteValue(value); }
    size_t WriteChar(char value) { return WriteValue(value); }
    size_t WriteUChar(unsigned char value) { return WriteValue(value); }
    size_t WriteLong(long value) { return WriteValue(value); }
    size_t WriteULong(unsigned long value) { return WriteValue(value); }
    size_t WriteString(const char *str);
    size_t WriteString(const std::string &str);

    size_t ReadInt(int &value) { return ReadValue(value); }
    size_t ReadUInt(unsigned int &value) { return ReadValue(value); }
    size_t ReadFloat(float &value) { return ReadValue(value); }
    size_t ReadDouble(double &value) { return ReadValue(value); }
    size_t ReadShort(short &value) { return ReadValue(value); }
    size_t ReadChar(char &value) { return ReadValue(value); }
    size_t ReadUChar(unsigned char &value) { return ReadValue(value); }
    size_t ReadLong(long &value) { return ReadValue(value); }
    size_t ReadULong(unsigned long &value) { return ReadValue(value); }
    size_t ReadString(char *outStr, size_t maxSize);
    size_t ReadString(std::string &str);

    bool Fill(int value, size_t size);

    bool Seek(size_t pos);
    bool Skip(size_t offset);
    void Reset() { m_CursorPos = 0; }

    char *Data() const { return m_Buffer; }
    size_t Size() const { return m_Size; }
    char *Cursor() const { return &m_Buffer[m_CursorPos]; }
    size_t CursorPos() const { return m_CursorPos; }

    bool IsValid() const { return m_Buffer != nullptr && m_Size > 0; }
    bool IsEmpty() const { return m_Buffer == nullptr || m_Size == 0; }

    int Compare(const NativeBuffer &other, size_t size) const;
    size_t Merge(const NativeBuffer &other, bool truncate = false);
    NativeBuffer *Extract(size_t size);
    NativePointer ToPointer() const;

    size_t Load(const char *filename, size_t size, int offset = 0);
    size_t Save(const char *filename, size_t size);

private:
    char *m_Buffer;
    size_t m_Size;
    size_t m_CursorPos;
    bool m_Owned;
    mutable RefCount m_RefCount;
    asILockableSharedBool *m_WeakRefFlag = nullptr;
};

void RegisterNativeBuffer(asIScriptEngine *engine);

#endif // CK_SCRIPTDATABUFFER_H
