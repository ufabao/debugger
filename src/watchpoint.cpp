#include <libmdb/error.hpp>
#include <libmdb/process.hpp>
#include <libmdb/watchpoint.hpp>

namespace mdb
{
auto get_next_id()
{
  static mdb::watchpoint::id_type id = 0;
  return ++id;
}

mdb::watchpoint::watchpoint(process& proc, virt_addr address, stoppoint_mode mode, std::size_t size)
    : process_{&proc}, address_{address}, mode_{mode}, size_{size}, is_enabled_{false}
{
  if (address.addr() & (size - 1) != 0)
  {
    error::send("Watchpoint must be aligned to size");
  }

  id_ = get_next_id();
}

void mdb::watchpoint::enable()
{
  if (is_enabled_)
  {
    return;
  }

  hardware_register_index_ = process_->set_watchpoint(id_, address_, mode_, size_);
  is_enabled_              = true;
}

void mdb::watchpoint::disable()
{
  if (!is_enabled_)
  {
    return;
  }

  process_->clear_hardware_stoppoint(hardware_register_index_);
  is_enabled_ = false;
}

}  // namespace mdb