#pragma once

#include <vector>
#include <atomic>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include <new>

namespace audio_engine {

/**
 * @brief 高性能单生产者单消费者 (SPSC) 无锁环形缓冲区
 * 专为发烧级低抖动音频流设计，基于 C++20 std::atomic 内存屏障，避免任何互斥锁与优先级反转。
 * 针对现代 CPU Cache Line (64字节) 对齐，彻底杜绝伪共享 (False Sharing)。
 */
template <typename T>
class LockFreeRingBuffer {
public:
    explicit LockFreeRingBuffer(size_t capacity = 131072) { // 默认可容纳约 1.5 秒 44.1k 双声道数据
        // 向上取整到最近的 2 的幂次方，便于位运算替代取模
        size_t cap = 1;
        while (cap < capacity) {
            cap <<= 1;
        }
        capacity_ = cap;
        mask_ = capacity_ - 1;
        buffer_.resize(capacity_);
        write_pos_.store(0, std::memory_order_relaxed);
        read_pos_.store(0, std::memory_order_relaxed);
    }

    ~LockFreeRingBuffer() = default;

    // 禁止拷贝与移动
    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer(LockFreeRingBuffer&&) = delete;
    LockFreeRingBuffer& operator=(LockFreeRingBuffer&&) = delete;

    /**
     * @brief 写入数据 (仅由生产者/解码线程调用)
     * @return 实际写入的元素个数
     */
    size_t write(const T* data, size_t count) {
        if (!data || count == 0) return 0;

        const size_t current_read = read_pos_.load(std::memory_order_acquire);
        const size_t current_write = write_pos_.load(std::memory_order_relaxed);

        const size_t occupied = (current_write >= current_read) ? (current_write - current_read) : 0;
        const size_t available = (capacity_ > occupied) ? (capacity_ - occupied) : 0;
        const size_t to_write = std::min(count, available);

        if (to_write == 0) return 0;

        const size_t write_idx = current_write & mask_;
        const size_t first_chunk = std::min(to_write, capacity_ - write_idx);
        const size_t second_chunk = to_write - first_chunk;

        std::memcpy(&buffer_[write_idx], data, first_chunk * sizeof(T));
        if (second_chunk > 0) {
            std::memcpy(&buffer_[0], data + first_chunk, second_chunk * sizeof(T));
        }

        write_pos_.store(current_write + to_write, std::memory_order_release);
        return to_write;
    }

    /**
     * @brief 读取数据 (仅由消费者/音频输出中断回调调用)
     * @return 实际读取的元素个数
     */
    size_t read(T* out_data, size_t count) {
        if (!out_data || count == 0) return 0;

        const size_t current_write = write_pos_.load(std::memory_order_acquire);
        const size_t current_read = read_pos_.load(std::memory_order_relaxed);

        const size_t occupied = (current_write >= current_read) ? (current_write - current_read) : 0;
        const size_t to_read = std::min(count, occupied);

        if (to_read == 0) return 0;

        const size_t read_idx = current_read & mask_;
        const size_t first_chunk = std::min(to_read, capacity_ - read_idx);
        const size_t second_chunk = to_read - first_chunk;

        std::memcpy(out_data, &buffer_[read_idx], first_chunk * sizeof(T));
        if (second_chunk > 0) {
            std::memcpy(out_data + first_chunk, &buffer_[0], second_chunk * sizeof(T));
        }

        read_pos_.store(current_read + to_read, std::memory_order_release);
        return to_read;
    }

    /**
     * @brief 可读元素数量
     */
    size_t available_read() const {
        const size_t w = write_pos_.load(std::memory_order_acquire);
        const size_t r = read_pos_.load(std::memory_order_acquire);
        return w >= r ? (w - r) : 0;
    }

    /**
     * @brief 可写元素数量
     */
    size_t available_write() const {
        return capacity_ - available_read();
    }

    /**
     * @brief 重置环形缓冲区
     */
    void reset() {
        write_pos_.store(0, std::memory_order_relaxed);
        read_pos_.store(0, std::memory_order_relaxed);
    }

    size_t capacity() const { return capacity_; }

private:
    std::vector<T> buffer_;
    size_t capacity_ = 0;
    size_t mask_ = 0;

    // 缓存行对齐，避免 write_pos_ 和 read_pos_ 在多核 CPU 上引起 Cache 竞争
    alignas(64) std::atomic<size_t> write_pos_{0};
    alignas(64) std::atomic<size_t> read_pos_{0};
};

} // namespace audio_engine
