#ifndef NEXKEYRUNTIME_NEXKEYRUNTIME_HPP
#define NEXKEYRUNTIME_NEXKEYRUNTIME_HPP

#include <nexkeyruntime/nexkeyruntime.h>

#include <stdexcept>
#include <utility>

namespace nexkeyruntime {

class Client {
public:
  explicit Client(const NexKeyRuntimeConfig &config)
      : handle_(nexkeyruntime_create(&config)) {
    if (!handle_) {
      throw std::invalid_argument("NexKeyRuntime configuration is invalid");
    }
  }

  ~Client() { nexkeyruntime_destroy(handle_); }

  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  Client(Client &&other) noexcept : handle_(other.handle_) {
    other.handle_ = nullptr;
  }

  Client &operator=(Client &&other) noexcept {
    if (this != &other) {
      nexkeyruntime_destroy(handle_);
      handle_ = other.handle_;
      other.handle_ = nullptr;
    }
    return *this;
  }

  NexKeyRuntimeResult requestCheck(bool force = false) {
    return nexkeyruntime_request_check(handle_, force ? 1 : 0);
  }

  NexKeyRuntimeUpdateSnapshot snapshot() const {
    NexKeyRuntimeUpdateSnapshot value{};
    value.struct_size = sizeof(value);
    const NexKeyRuntimeResult result = nexkeyruntime_get_snapshot(handle_, &value);
    if (result != NEXKEYRUNTIME_OK) {
      throw std::runtime_error("Unable to read NexKeyRuntime snapshot");
    }
    return value;
  }

private:
  NexKeyRuntimeHandle *handle_;
};

} // namespace nexkeyruntime

#endif
