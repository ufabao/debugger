#pragma once

#include <string_view>

namespace mdb
{
std::string_view syscall_id_to_name(int id);
int              syscall_name_to_id(std::string_view name);
}  // namespace mdb