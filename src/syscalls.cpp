#include <libmdb/error.hpp>
#include <libmdb/syscalls.hpp>
#include <unordered_map>

std::string_view mdb::syscall_id_to_name(int id)
{
  switch (id)
  {
#define DEFINE_SYSCALL(name, id) \
  case id:                       \
    return #name;
#include <libmdb/syscalls.inc>
#undef DEFINE_SYSCALL
    default:
      mdb::error::send("No such syscall");
  }
}

namespace
{
const std::unordered_map<std::string_view, int> g_syscall_name_map = {
#define DEFINE_SYSCALL(name, id) {#name, id},
#include <libmdb/syscalls.inc>
#undef DEFINE_SYSCALL
};
}  // namespace

int mdb::syscall_name_to_id(std::string_view name)
{
  if (g_syscall_name_map.count(name) != 1)
  {
    mdb::error::send("No such syscall");
  }

  return g_syscall_name_map.at(name);
}