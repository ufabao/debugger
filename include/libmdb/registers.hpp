#pragma once

#include <sys/user.h>

#include <libmdb/register_info.hpp>
#include <libmdb/types.hpp>
#include <variant>

namespace mdb {
class Process;
class Registers {
 public:
  Registers()                            = delete;
  Registers(const Registers&)            = delete;
  Registers& operator=(const Registers&) = delete;
  using value                            = std::variant<std::uint8_t,
                                                        std::uint16_t,
                                                        std::uint32_t,
                                                        std::uint64_t,
                                                        std::int8_t,
                                                        std::int16_t,
                                                        std::int32_t,
                                                        std::int64_t,
                                                        float,
                                                        double,
                                                        long double,
                                                        byte64,
                                                        byte128>;

  [[nodiscard]]
  value read(const register_info& info) const;
  void  write(const register_info& info, value val);

  template <class T>
  T read_by_id_as(register_id id) const {
    return std::get<T>(read(register_info_by_id(id)));
  }

  void write_by_id(register_id id, value val) {
    write(register_info_by_id(id), val);
  }

 private:
  friend Process;
  Registers(Process& proc) : proc_(&proc) {}

  user     data_;
  Process* proc_;
};
}  // namespace mdb