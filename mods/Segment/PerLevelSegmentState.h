#pragma once
#include <vector>
#include <algorithm>

class PerLevelSegmentState {
public:
    explicit PerLevelSegmentState(const size_t segment_count)
        : segment_time_(segment_count, 0.0f),
          segment_time_to_compare_(segment_count, -1.0f) {}

    PerLevelSegmentState(PerLevelSegmentState &&other) noexcept
        : is_counting_(other.is_counting_),
          current_segment_(other.current_segment_),
          segment_time_(std::move(other.segment_time_)) {}

    void enable_counting(bool enabled) {
        is_counting_ = enabled;
    }

    void update(const float dt) {
        if (!is_counting_) return;
        segment_time_[current_segment_] += dt;
    }

    void update_target_figures() {
        if (!is_saving_)
            return;
        //std::copy(segment_time_.begin(), segment_time_.begin() + current_segment_, segment_time_to_compare_.begin());
        for (int i = 0; i < current_segment_; ++i) {
            if (segment_time_[i] > 0 && (segment_time_[i] < segment_time_to_compare_[i] || segment_time_to_compare_[i] < 0))
                segment_time_to_compare_[i] = segment_time_[i];
        }
    }

    void reset() {
        update_target_figures();
        is_counting_ = false;
        current_segment_ = 0;
        std::fill(segment_time_.begin(), segment_time_.end(), 0.0f);
    }

    void clear_history() {
        std::fill(segment_time_to_compare_.begin(), segment_time_to_compare_.end(), -1.0f);
    }

    void reset_segment(const int seg) {
        segment(seg) = 0.0f;
    }

    void change_segment(const int seg) {
        if (0 < seg && seg < static_cast<int>(size()))
            reset_segment(seg);
        else
            enable_counting(false);
        current_segment_ = seg;
    }

    float &segment(const size_t segment) {
        return segment_time_[segment];
    }

    float &segment_target(const size_t segment) {
        return segment_time_to_compare_[segment];
    }

    const float &segment(const size_t segment) const {
        return segment_time_[segment];
    }

    const float &segment_target(const size_t segment) const {
        return segment_time_to_compare_[segment];
    }

    int get_current_segment() const {
        return current_segment_;
    }

    // Iterators
    using iterator = std::vector<float>::iterator;
    using const_iterator = std::vector<float>::const_iterator;

    iterator begin() { return segment_time_.begin(); }
    iterator end() { return segment_time_.end(); }

    constexpr const_iterator cbegin() const { return segment_time_.cbegin(); }
    constexpr const_iterator cend() const { return segment_time_.cend(); }
    constexpr size_t size() const { return segment_time_.size(); }

    bool is_saving_ = true;

private:
    bool is_counting_ = false;
    int current_segment_ = 0;
    std::vector<float> segment_time_;
    std::vector<float> segment_time_to_compare_;
};
