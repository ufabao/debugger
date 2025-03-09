#ifndef mdb_ERROR_HPP
#define mdb_ERROR_HPP

#include <cstring>
#include <stdexcept>

namespace mdb
{
class error : public std::runtime_error
{
 public:
  static void send(const std::string& what)
  {
    throw error(what);
  }
  static void send_errno(const std::string& prefix)
  {
    throw error(prefix + ": " + std::strerror(errno));
  }

 private:
  error(const std::string& what) : std::runtime_error(what) {}
};
}  // namespace mdb

#endif