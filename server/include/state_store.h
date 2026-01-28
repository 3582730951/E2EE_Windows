#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace mi::server {

struct BlobLoadResult {
  bool found{false};
  std::vector<std::uint8_t> data;
};

class StateStore {
 public:
  virtual ~StateStore() = default;

  virtual bool LoadBlob(const std::string& key, BlobLoadResult& out,
                        std::string& error) = 0;
  virtual bool SaveBlob(const std::string& key,
                        const std::vector<std::uint8_t>& data,
                        std::string& error) = 0;
  virtual bool DeleteBlob(const std::string& key, std::string& error) = 0;
  virtual bool AcquireLock(const std::string& key,
                           std::chrono::milliseconds timeout,
                           std::string& error) = 0;
  virtual void ReleaseLock(const std::string& key) = 0;
  virtual bool HasAnyData(bool& out_has_data, std::string& error) = 0;
};

class StateStoreLock {
 public:
  StateStoreLock(StateStore* store, const std::string& key,
                 std::chrono::milliseconds timeout, std::string& error)
      : store_(store), key_(key), locked_(false) {
    if (store_) {
      locked_ = store_->AcquireLock(key_, timeout, error);
    } else {
      locked_ = true;
    }
  }

  ~StateStoreLock() {
    if (store_ && locked_) {
      store_->ReleaseLock(key_);
    }
  }

  bool locked() const { return locked_; }

 private:
  StateStore* store_;
  std::string key_;
  bool locked_;
};

}  // namespace mi::server
