#ifndef BML_BEHAVIOR_FRAMES_HPP
#define BML_BEHAVIOR_FRAMES_HPP

#include "BML/Behavior/Prototype.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace BML::Behavior {

struct FramePolicy {
    std::uint32_t Kind = BML_BEHAVIOR_FRAMES_SIGNALS;
    std::uint32_t Limit = 64;
    std::uint32_t Flags = BML_BEHAVIOR_FRAME_POLICY_NONE;

    [[nodiscard]] FramePolicy Pouts(bool include = true) const noexcept {
        FramePolicy policy = *this;
        if (include)
            policy.Flags |= BML_BEHAVIOR_FRAME_POLICY_POUTS;
        else
            policy.Flags &= ~BML_BEHAVIOR_FRAME_POLICY_POUTS;
        return policy;
    }
    [[nodiscard]] bool IncludesPouts() const noexcept {
        return (Flags & BML_BEHAVIOR_FRAME_POLICY_POUTS) != 0;
    }
};

inline FramePolicy Signals(std::uint32_t limit = 64) {
    return {BML_BEHAVIOR_FRAMES_SIGNALS, limit,
            BML_BEHAVIOR_FRAME_POLICY_NONE};
}
inline FramePolicy EachFrame(std::uint32_t limit) {
    return {BML_BEHAVIOR_FRAMES_EACH_FRAME, limit,
            BML_BEHAVIOR_FRAME_POLICY_NONE};
}
inline FramePolicy Latest() {
    return {BML_BEHAVIOR_FRAMES_LATEST, 0,
            BML_BEHAVIOR_FRAME_POLICY_NONE};
}
inline FramePolicy Ignore() {
    return {BML_BEHAVIOR_FRAMES_NONE, 0,
            BML_BEHAVIOR_FRAME_POLICY_NONE};
}

enum class RunKind : std::uint32_t {
    Call = BML_BEHAVIOR_RUN_CALL,
    Task = BML_BEHAVIOR_RUN_TASK,
    Instance = BML_BEHAVIOR_RUN_INSTANCE,
};

enum class RunState : std::uint32_t {
    Ready = BML_BEHAVIOR_RUN_READY,
    Pending = BML_BEHAVIOR_RUN_PENDING,
    Failed = BML_BEHAVIOR_RUN_FAILED,
};

enum class PulseResult : std::uint32_t {
    Ran = BML_BEHAVIOR_ADMISSION_EXECUTED,
    Queued = BML_BEHAVIOR_ADMISSION_QUEUED,
};

enum class DetachedSupport {
    Verified,
    Unverified,
};

enum class Continuation : std::uint32_t {
    None = BML_BEHAVIOR_CONTINUATION_NONE,
    Native = BML_BEHAVIOR_CONTINUATION_NATIVE,
    QueuedInput = BML_BEHAVIOR_CONTINUATION_QUEUED_INPUT,
};

[[nodiscard]] constexpr Continuation operator|(Continuation left,
                                                Continuation right) noexcept {
    return static_cast<Continuation>(static_cast<std::uint32_t>(left) |
                                     static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr bool Has(Continuation value,
                                 Continuation flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(flag)) != 0;
}

struct RunInfo {
    RunKind Kind = RunKind::Instance;
    RunState State = RunState::Ready;
    DetachedSupport Detached = DetachedSupport::Verified;
    Prototype PrototypeRef;
    Status LastStatus;
};

using PoutData = std::variant<std::monostate, bool, std::int32_t, float,
                              std::string, BML_Vec2, BML_Vec3,
                              BML_Quaternion, BML_Euler, BML_Rect, BML_Color,
                              BML_Box, BML_Mat4, ObjectRef>;

class Frames;

class Out {
public:
    [[nodiscard]] std::int32_t Index() const noexcept { return m_Record.Index; }
    [[nodiscard]] std::int32_t Occurrence() const noexcept {
        return m_Record.Occurrence;
    }
    [[nodiscard]] std::string_view Name() const noexcept;

private:
    Out(const Frames *frames, BML_BehaviorOutRecord record) noexcept
        : m_Frames(frames), m_Record(record) {}

    const Frames *m_Frames = nullptr;
    BML_BehaviorOutRecord m_Record{};

    friend class Frame;
};

class Pout {
public:
    [[nodiscard]] std::int32_t Index() const noexcept { return m_Record.Index; }
    [[nodiscard]] std::int32_t Occurrence() const noexcept {
        return m_Record.Occurrence;
    }
    [[nodiscard]] CKGUID Type() const noexcept {
        return Detail::NativeGuid(m_Record.Type);
    }
    [[nodiscard]] ValueKind Kind() const noexcept {
        return static_cast<ValueKind>(m_Record.Kind);
    }
    [[nodiscard]] std::string_view Name() const noexcept;

    template <class T>
    [[nodiscard]] Result<T> Get() const;

private:
    Pout(const Frames *frames, BML_BehaviorPoutRecord record) noexcept
        : m_Frames(frames), m_Record(record) {}

    const Frames *m_Frames = nullptr;
    BML_BehaviorPoutRecord m_Record{};

    friend class Frame;
};

class Frame {
public:
    using PoutView = Behavior::Pout;

    [[nodiscard]] std::uint64_t Sequence() const noexcept;
    [[nodiscard]] std::uint64_t GameFrame() const noexcept;
    [[nodiscard]] std::int32_t NativeResult() const noexcept;
    [[nodiscard]] Behavior::Continuation Continuation() const noexcept;
    [[nodiscard]] Behavior::Error Error() const noexcept;
    [[nodiscard]] std::size_t OutCount() const noexcept;
    [[nodiscard]] std::size_t PoutCount() const noexcept;
    [[nodiscard]] std::size_t StatusCount() const noexcept;
    [[nodiscard]] Out GetOut(std::size_t index) const;
    [[nodiscard]] PoutView GetPout(std::size_t index) const;
    [[nodiscard]] Behavior::Status GetStatus(std::size_t index) const;
    [[nodiscard]] bool HasOut(const Selector &selector) const;
    [[nodiscard]] bool HasOut(std::string_view name) const;

    template <class T>
    [[nodiscard]] Result<T> Pout(const Selector &selector) const;
    template <class T>
    [[nodiscard]] Result<T> Pout(std::string_view name) const;

private:
    Frame(const Frames *frames, std::size_t index) noexcept
        : m_Frames(frames), m_Index(index) {}

    const Frames *m_Frames = nullptr;
    std::size_t m_Index = 0;

    friend class Frames;
};

class Frames {
public:
    class Iterator {
    public:
        class Arrow {
        public:
            explicit Arrow(Frame value) : m_Value(std::move(value)) {}
            [[nodiscard]] const Frame *operator->() const noexcept {
                return &m_Value;
            }

        private:
            Frame m_Value;
        };

        using difference_type = std::ptrdiff_t;
        using value_type = Frame;
        using pointer = Arrow;
        using reference = Frame;
        using iterator_category = std::random_access_iterator_tag;

        [[nodiscard]] Frame operator*() const { return (*m_Frames)[m_Index]; }
        [[nodiscard]] Arrow operator->() const { return Arrow(**this); }
        [[nodiscard]] Frame operator[](difference_type value) const {
            return (*m_Frames)[static_cast<std::size_t>(
                static_cast<difference_type>(m_Index) + value)];
        }
        Iterator &operator++() { ++m_Index; return *this; }
        Iterator operator++(int) { Iterator copy = *this; ++*this; return copy; }
        Iterator &operator--() { --m_Index; return *this; }
        Iterator operator--(int) { Iterator copy = *this; --*this; return copy; }
        Iterator &operator+=(difference_type value) {
            m_Index = static_cast<std::size_t>(
                static_cast<difference_type>(m_Index) + value);
            return *this;
        }
        Iterator &operator-=(difference_type value) { return *this += -value; }
        friend Iterator operator+(Iterator it, difference_type value) {
            return it += value;
        }
        friend Iterator operator+(difference_type value, Iterator it) {
            return it += value;
        }
        friend Iterator operator-(Iterator it, difference_type value) {
            return it -= value;
        }
        friend difference_type operator-(Iterator left, Iterator right) {
            return static_cast<difference_type>(left.m_Index) -
                   static_cast<difference_type>(right.m_Index);
        }
        friend bool operator==(Iterator left, Iterator right) {
            return left.m_Frames == right.m_Frames && left.m_Index == right.m_Index;
        }
        friend bool operator!=(Iterator left, Iterator right) { return !(left == right); }
        friend bool operator<(Iterator left, Iterator right) {
            return left.m_Index < right.m_Index;
        }
        friend bool operator>(Iterator left, Iterator right) { return right < left; }
        friend bool operator<=(Iterator left, Iterator right) { return !(right < left); }
        friend bool operator>=(Iterator left, Iterator right) { return !(left < right); }

    private:
        Iterator(const Frames *frames, std::size_t index)
            : m_Frames(frames), m_Index(index) {}
        const Frames *m_Frames = nullptr;
        std::size_t m_Index = 0;
        friend class Frames;
    };

    [[nodiscard]] bool Empty() const noexcept { return m_Count == 0; }
    [[nodiscard]] std::size_t Size() const noexcept { return m_Count; }
    [[nodiscard]] Frame operator[](std::size_t index) const & {
        if (index >= m_Count)
            throw std::out_of_range("Behavior Frame index is out of range.");
        return Frame(this, index);
    }
    [[nodiscard]] Frame operator[](std::size_t) const && = delete;
    [[nodiscard]] Iterator begin() const & noexcept { return Iterator(this, 0); }
    [[nodiscard]] Iterator end() const & noexcept { return Iterator(this, m_Count); }
    [[nodiscard]] Iterator begin() const && = delete;
    [[nodiscard]] Iterator end() const && = delete;
    void Clear() noexcept { m_Count = 0; m_PayloadSize = 0; }
    void Reserve(std::size_t frames, std::size_t payload) {
        if (frames > (std::numeric_limits<std::uint32_t>::max)() ||
            payload > (std::numeric_limits<std::uint32_t>::max)())
            throw std::length_error("Behavior Frames exceed the C interface limits.");
        if (frames > m_Headers.size())
            m_Headers.resize(frames);
        if (payload > m_Payload.size())
            m_Payload.resize(payload);
    }
    void ShrinkToFit() {
        m_Headers.resize(m_Count);
        m_Payload.resize(m_PayloadSize);
        m_Headers.shrink_to_fit();
        m_Payload.shrink_to_fit();
    }

private:
    [[nodiscard]] const BML_BehaviorRunFrame &Header(std::size_t index) const {
        return m_Headers[index];
    }
    [[nodiscard]] const std::uint8_t *Payload() const noexcept {
        return m_Payload.data();
    }
    template <class T>
    [[nodiscard]] bool Record(std::uint32_t offset, std::uint32_t index,
                              T &record) const noexcept {
        const std::uint64_t at = static_cast<std::uint64_t>(offset) +
            static_cast<std::uint64_t>(index) * sizeof(T);
        if (at + sizeof(T) > m_PayloadSize)
            return false;
        std::memcpy(&record, m_Payload.data() + at, sizeof(T));
        return record.StructSize >= sizeof(T);
    }
    [[nodiscard]] bool Bytes(std::uint32_t offset, std::uint32_t size,
                             const std::uint8_t *&data) const noexcept {
        if (static_cast<std::uint64_t>(offset) + size > m_PayloadSize)
            return false;
        data = m_Payload.data() + offset;
        return true;
    }
    [[nodiscard]] std::string_view Text(std::uint32_t offset,
                                        std::uint32_t size) const noexcept {
        const std::uint8_t *data = nullptr;
        return Bytes(offset, size, data)
            ? std::string_view(reinterpret_cast<const char *>(data), size)
            : std::string_view{};
    }
    [[nodiscard]] bool Accept(std::size_t count,
                              std::size_t payloadSize) noexcept;

    std::vector<BML_BehaviorRunFrame> m_Headers;
    std::vector<std::uint8_t> m_Payload;
    std::size_t m_Count = 0;
    std::size_t m_PayloadSize = 0;

    friend class Out;
    friend class Pout;
    friend class Frame;
    friend class Detail::Run;
};


} // namespace BML::Behavior

#endif // BML_BEHAVIOR_FRAMES_HPP
