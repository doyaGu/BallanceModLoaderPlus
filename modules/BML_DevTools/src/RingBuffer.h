#ifndef BML_DEVTOOLS_RINGBUFFER_H
#define BML_DEVTOOLS_RINGBUFFER_H

#include <cstddef>

namespace devtools {

template <typename T, size_t N>
class RingBuffer {
    T m_Data[N]{};
    size_t m_Head = 0;
    size_t m_Count = 0;

public:
    void Push(T value) {
        m_Data[m_Head] = value;
        m_Head = (m_Head + 1) % N;
        if (m_Count < N) ++m_Count;
    }

    void Clear() { m_Head = 0; m_Count = 0; }
    size_t Count() const { return m_Count; }
    size_t Capacity() const { return N; }

    T operator[](size_t i) const {
        size_t start = (m_Count < N) ? 0 : m_Head;
        return m_Data[(start + i) % N];
    }

    const T *Data() const { return m_Data; }
    int Offset() const { return static_cast<int>(m_Head); }
    int Size() const { return static_cast<int>(m_Count < N ? m_Count : N); }
};

} // namespace devtools

#endif // BML_DEVTOOLS_RINGBUFFER_H
