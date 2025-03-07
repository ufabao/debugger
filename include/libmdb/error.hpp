#pragma once

#include <cstring>
#include <stdexcept>

namespace mdb {

class Error : public std::runtime_error {
 public:
  [[noreturn]]
  static void send(const std::string& what) {
    throw Error(what);
  }

  [[noreturn]]
  static void send_errno(const std::string& prefix) {
    throw Error(prefix + ": " + std::strerror(errno));
  }

 private:
  Error(const std::string& what) : std::runtime_error(what) {}
};
}  // namespace mdb