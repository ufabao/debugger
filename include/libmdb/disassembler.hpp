#pragma once

#include <libmdb/process.hpp>
#include <optional>

namespace mdb
{
class disassembler
{
  struct instruction
  {
    virt_addr   address;
    std::string text;
  };

 public:
  disassembler(process& proc) : process_(&proc) {}

  std::vector<instruction> disassemble(std::size_t              n_instructions,
                                       std::optional<virt_addr> address = std::nullopt);

 private:
  process* process_;
};
}  // namespace mdb