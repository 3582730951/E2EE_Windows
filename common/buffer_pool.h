#ifndef MI_E2EE_BUFFER_POOL_H
#define MI_E2EE_BUFFER_POOL_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include "secure_buffer.h"

namespace mi::common {

class ByteBufferPool {
 public:
  explicit ByteBufferPool(std::size_t max_buffers = 64,
                          std::size_t max_capacity = 2 * 1024 * 1024)
      : max_buffers_(max_buffers), max_capacity_(max_capacity) {}

  std::vector<std::uint8_t> Acquire(std::size_t min_capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = pool_.begin(); it != pool_.end(); ++it) {
      if (it->capacity() >= min_capacity) {
        std::vector<std::uint8_t> out = std::move(*it);
        pool_.erase(it);
        out.clear();
        return out;
      }
    }
    std::vector<std::uint8_t> out;
    if (min_capacity > 0) {
      out.reserve(min_capacity);
    }
    return out;
  }

  void Release(std::vector<std::uint8_t>&& buf) {
    if (buf.capacity() > max_capacity_) {
      return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (pool_.size() >= max_buffers_) {
      return;
    }
    buf.clear();
    pool_.push_back(std::move(buf));
  }

 private:
  std::mutex mutex_;
  std::vector<std::vector<std::uint8_t>> pool_;
  std::size_t max_buffers_{64};
  std::size_t max_capacity_{2 * 1024 * 1024};
};

inline ByteBufferPool& GlobalByteBufferPool() {
  static ByteBufferPool pool;
  return pool;
}

class ScopedBuffer {
 public:
  ScopedBuffer(ByteBufferPool& pool, std::size_t min_capacity,
               bool wipe_on_release)
      : pool_(&pool),
        buffer_(pool.Acquire(min_capacity)),
        wipe_on_release_(wipe_on_release) {}

  ~ScopedBuffer() {
    if (pool_) {
      if (wipe_on_release_ && !buffer_.empty()) {
        SecureWipe(buffer_);
      }
      pool_->Release(std::move(buffer_));
    }
  }

  std::vector<std::uint8_t>& get() { return buffer_; }

  std::vector<std::uint8_t> Release() {
    if (!pool_) {
      return std::move(buffer_);
    }
    if (wipe_on_release_ && !buffer_.empty()) {
      SecureWipe(buffer_);
    }
    pool_->Release(std::move(buffer_));
    pool_ = nullptr;
    return {};
  }

 private:
  ByteBufferPool* pool_{nullptr};
  std::vector<std::uint8_t> buffer_;
  bool wipe_on_release_{false};
};

}  // namespace mi::common

#endif  // MI_E2EE_BUFFER_POOL_H
