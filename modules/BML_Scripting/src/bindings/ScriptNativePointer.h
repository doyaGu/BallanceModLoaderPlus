#ifndef CK_SCRIPTNATIVEPOINTER_H
#define CK_SCRIPTNATIVEPOINTER_H

#include <cstdint>
#include <string>

#include <angelscript.h>

class NativePointer {
public:
    NativePointer() = default;
    NativePointer(void *ptr) { m_Ptr = static_cast<char *>(ptr); }
    NativePointer(intptr_t addr) { m_Ptr = reinterpret_cast<char *>(addr); }
    NativePointer(uintptr_t addr) { m_Ptr = reinterpret_cast<char *>(addr); }
    NativePointer(const NativePointer &rhs) { m_Ptr = rhs.m_Ptr; }

    NativePointer &operator=(const NativePointer &rhs) {
        if (this != &rhs) {
            m_Ptr = rhs.m_Ptr;
        }
        return *this;
    }

    NativePointer &operator=(void *ptr) {
        m_Ptr = static_cast<char *>(ptr);
        return *this;
    }

    NativePointer &operator=(intptr_t addr) {
        m_Ptr = reinterpret_cast<char *>(addr);
        return *this;
    }

    NativePointer &operator=(uintptr_t addr) {
        m_Ptr = reinterpret_cast<char *>(addr);
        return *this;
    }

    bool operator==(const NativePointer &rhs) const {
        return m_Ptr == rhs.m_Ptr;
    }

    bool operator!=(const NativePointer &rhs) const {
        return !(*this == rhs);
    }

    bool operator<(const NativePointer &rhs) const {
        return m_Ptr < rhs.m_Ptr;
    }

    bool operator>=(const NativePointer &rhs) const {
        return !(*this < rhs);
    }

    bool operator>(const NativePointer &rhs) const {
        return *this < rhs;
    }

    bool operator<=(const NativePointer &rhs) const {
        return !(*this > rhs);
    }

    int Compare(const NativePointer &rhs) const {
        return m_Ptr - rhs.m_Ptr;
    }

    NativePointer &operator+=(int rhs) {
        m_Ptr += rhs;
        return *this;
    }

    NativePointer &operator-=(int rhs) {
        m_Ptr -= rhs;
        return *this;
    }

    NativePointer &operator&=(int rhs) {
        m_Ptr = reinterpret_cast<char *>(reinterpret_cast<intptr_t>(m_Ptr) & rhs);
        return *this;
    }

    NativePointer &operator|=(int rhs) {
        m_Ptr = reinterpret_cast<char *>(reinterpret_cast<intptr_t>(m_Ptr) | rhs);
        return *this;
    }

    NativePointer &operator^=(int rhs) {
        m_Ptr = reinterpret_cast<char *>(reinterpret_cast<intptr_t>(m_Ptr) ^ rhs);
        return *this;
    }

    NativePointer &operator<<=(int rhs) {
        m_Ptr = reinterpret_cast<char *>(reinterpret_cast<intptr_t>(m_Ptr) << rhs);
        return *this;
    }

    NativePointer &operator>>=(int rhs) {
        m_Ptr = reinterpret_cast<char *>(reinterpret_cast<intptr_t>(m_Ptr) >> rhs);
        return *this;
    }

    NativePointer operator+(int rhs) const {
        return {m_Ptr + rhs};
    }

    NativePointer operator-(int rhs) const {
        return {m_Ptr - rhs};
    }

    NativePointer operator&(int rhs) const {
        return {reinterpret_cast<uint8_t *>(reinterpret_cast<intptr_t>(m_Ptr) & rhs)};
    }

    NativePointer operator|(int rhs) const {
        return {reinterpret_cast<uint8_t *>(reinterpret_cast<intptr_t>(m_Ptr) | rhs)};
    }

    NativePointer operator^(int rhs) const {
        return {reinterpret_cast<uint8_t *>(reinterpret_cast<intptr_t>(m_Ptr) ^ rhs)};
    }

    NativePointer operator<<(int rhs) const {
        return {reinterpret_cast<uint8_t *>(reinterpret_cast<intptr_t>(m_Ptr) << rhs)};
    }

    NativePointer operator>>(int rhs) const {
        return {reinterpret_cast<uint8_t *>(reinterpret_cast<intptr_t>(m_Ptr) >> rhs)};
    }

    NativePointer &operator++() {
        ++m_Ptr;
        return *this;
    }

    NativePointer operator++(int) {
        NativePointer tmp = *this;
        ++*this;
        return tmp;
    }

    NativePointer &operator--() {
        --m_Ptr;
        return *this;
    }

    NativePointer operator--(int) {
        NativePointer tmp = *this;
        --*this;
        return tmp;
    }

    NativePointer operator-() const {
        return {reinterpret_cast<uint8_t *>(-reinterpret_cast<intptr_t>(m_Ptr))};
    }

    NativePointer operator~() const {
        return {reinterpret_cast<uint8_t *>(~reinterpret_cast<intptr_t>(m_Ptr))};
    }

    NativePointer operator+() const { return *this; }

    NativePointer operator*() const { return *this; }

    explicit operator void *() const { return m_Ptr; }

    char *Get() const { return m_Ptr; }

    template<typename T>
    T *As() const { return reinterpret_cast<T *>(m_Ptr); }

    uintptr_t ToUInt() const { return reinterpret_cast<uintptr_t>(m_Ptr); }
    intptr_t ToInt() const { return reinterpret_cast<intptr_t>(m_Ptr); }

    std::string ToString() const;

    NativePointer ReadPointer() const {
        return {*reinterpret_cast<char **>(m_Ptr)};
    }

    void WritePointer(const NativePointer &ptr) {
        *reinterpret_cast<char **>(m_Ptr) = ptr.m_Ptr;
    }

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

    bool IsNull() const { return m_Ptr == nullptr; }

private:
    char *m_Ptr = nullptr;
};

void RegisterNativePointer(asIScriptEngine *engine);

#endif // CK_SCRIPTNATIVEPOINTER_H
