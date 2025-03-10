#include <fmt/format.h>
#include <fmt/ranges.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <charconv>
#include <iostream>
#include <libmdb/error.hpp>
#include <libmdb/process.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::unique_ptr<mdb::process> attach(int argc, const char** argv)
{
  // Passing PID
  if (argc == 3 && argv[1] == std::string_view("-p"))
  {
    pid_t pid = std::atoi(argv[2]);
    return mdb::process::attach(pid);
  }
  // Passing program name
  else
  {
    const auto* program_path = argv[1];
    auto        proc         = mdb::process::launch(program_path);
    fmt::print("Launched process with PID {}\n", proc->pid());
    return proc;
  }
}

std::vector<std::string> split(std::string_view str, char delimiter)
{
  std::vector<std::string> out{};
  std::stringstream        ss{std::string{str}};
  std::string              item;

  while (std::getline(ss, item, delimiter))
  {
    out.push_back(item);
  }

  return out;
}

bool is_prefix(std::string_view str, std::string_view of)
{
  if (str.size() > of.size())
    return false;
  return std::equal(str.begin(), str.end(), of.begin());
}

void print_stop_reason(const mdb::process& process, mdb::stop_reason reason)
{
  std::string message;
  switch (reason.reason)
  {
    case mdb::process_state::exited:
      message = fmt::format("exited with status {}", static_cast<int>(reason.info));
      break;
    case mdb::process_state::terminated:
      message = fmt::format("terminated with signal {}", sigabbrev_np(reason.info));
      break;
    case mdb::process_state::stopped:
      message = fmt::format(
          "stopped with signal {} at {:#x}", sigabbrev_np(reason.info), process.get_pc().addr());
      break;
  }

  fmt::print("Process {} {}\n", process.pid(), message);
}

void print_help(const std::vector<std::string>& args)
{
  if (args.size() == 1)
  {
    std::cerr << R"(Available commands:
    breakpoint  - Commands for operating on breakpoints
    continue    - Resume the process
    register    - Commands for operating on registers
    step        - Step over a single instruction
)";
  }

  else if (is_prefix(args[1], "register"))
  {
    std::cerr << R"(Available commands:
    read
    read <register>
    read all
    write <register> <value>
)";
  }
  else if (is_prefix(args[1], "breakpoint"))
  {
    std::cerr << R"(Available commands:
    list 
    delete <id>
    disable <id>
    enable <id>
    set <address>
    )";
  }
  else
  {
    std::cerr << "No help available on that\n";
  }
}

void handle_register_read(mdb::process& process, const std::vector<std::string>& args)
{
  auto format = [](auto t)
  {
    if constexpr (std::is_floating_point_v<decltype(t)>)
    {
      return fmt::format("{}", t);
    }
    else if constexpr (std::is_integral_v<decltype(t)>)
    {
      return fmt::format("{:#0{}x}", t, sizeof(t) * 2 + 2);
    }
    else
    {
      return fmt::format("[{:#04x}]", fmt::join(t, ","));
    }
  };

  if (args.size() == 2 or (args.size() == 3 and args[2] == "all"))
  {
    for (auto& info : mdb::g_register_infos)
    {
      auto should_print =
          (args.size() == 3 or info.type == mdb::register_type::gpr) and info.name != "orig_rax";
      if (!should_print)
        continue;
      auto value = process.get_registers().read(info);
      fmt::print("{}:\t{}\n", info.name, std::visit(format, value));
    }
  }
  else if (args.size() == 3)
  {
    try
    {
      auto info  = mdb::register_info_by_name(args[2]);
      auto value = process.get_registers().read(info);
      fmt::print("{}:\t{}\n", info.name, std::visit(format, value));
    }
    catch (mdb::error& err)
    {
      std::cerr << "No such register\n";
      return;
    }
  }
  else
  {
    print_help({"help", "register"});
  }
}

template <class I>
std::optional<I> to_integral(std::string_view sv, int base = 10)
{
  auto begin = sv.begin();
  if (base == 16 and sv.size() > 1 and begin[0] == '0' and begin[1] == 'x')
  {
    begin += 2;
  }

  I    ret;
  auto result = std::from_chars(begin, sv.end(), ret, base);

  if (result.ptr != sv.end())
  {
    return std::nullopt;
  }
  return ret;
}

template <>
std::optional<std::byte> to_integral(std::string_view sv, int base)
{
  auto uint8 = to_integral<std::uint8_t>(sv, base);
  if (uint8)
    return static_cast<std::byte>(*uint8);
  return std::nullopt;
}

template <std::size_t N>
auto parse_vector(std::string_view text)
{
  auto invalid = [] { mdb::error::send("Invalid format"); };

  std::array<std::byte, N> bytes;
  const char*              c = text.data();

  if (*c++ != '[')
    invalid();
  for (auto i = 0; i < N - 1; ++i)
  {
    bytes[i] = to_integral<std::byte>({c, 4}, 16).value();
    c += 4;
    if (*c++ != ',')
      invalid();
  }

  bytes[N - 1] = to_integral<std::byte>({c, 4}, 16).value();
  c += 4;

  if (*c++ != ']')
    invalid();
  if (c != text.end())
    invalid();

  return bytes;
}

template <class F>
std::optional<F> to_float(std::string_view sv)
{
  F    ret;
  auto result = std::from_chars(sv.begin(), sv.end(), ret);

  if (result.ptr != sv.end())
  {
    return std::nullopt;
  }
  return ret;
}

mdb::registers::value parse_register_value(mdb::register_info info, std::string_view text)
{
  try
  {
    if (info.format == mdb::register_format::uint)
    {
      switch (info.size)
      {
        case 1:
          return to_integral<std::uint8_t>(text, 16).value();
        case 2:
          return to_integral<std::uint16_t>(text, 16).value();
        case 4:
          return to_integral<std::uint32_t>(text, 16).value();
        case 8:
          return to_integral<std::uint64_t>(text, 16).value();
      }
    }
    else if (info.format == mdb::register_format::double_float)
    {
      return to_float<double>(text).value();
    }
    else if (info.format == mdb::register_format::long_double)
    {
      return to_float<long double>(text).value();
    }
    else if (info.format == mdb::register_format::vector)
    {
      if (info.size == 8)
      {
        return parse_vector<8>(text);
      }
      else if (info.size == 16)
      {
        return parse_vector<16>(text);
      }
    }
  }
  catch (...)
  {
  }
  mdb::error::send("Invalid format");
}

void handle_register_write(mdb::process& process, const std::vector<std::string>& args)
{
  if (args.size() != 4)
  {
    print_help({"help", "register"});
    return;
  }
  try
  {
    auto info  = mdb::register_info_by_name(args[2]);
    auto value = parse_register_value(info, args[3]);
    process.get_registers().write(info, value);
  }
  catch (mdb::error& err)
  {
    std::cerr << err.what() << '\n';
    return;
  }
}

void handle_register_command(mdb::process& process, const std::vector<std::string>& args)
{
  if (args.size() < 2)
  {
    print_help({"help", "register"});
    return;
  }

  if (is_prefix(args[1], "read"))
  {
    handle_register_read(process, args);
  }
  else if (is_prefix(args[1], "write"))
  {
    handle_register_write(process, args);
  }
  else
  {
    print_help({"help", "register"});
  }
}

void handle_breakpoint_command(mdb::process& process, const std::vector<std::string>& args)
{
  if (args.size() < 2)
  {
    print_help({"help", "breakpoint"});
    return;
  }

  auto command = args[1];

  if (is_prefix(command, "list"))
  {
    if (process.breakpoint_sites().empty())
    {
      fmt::print("No breakpoints set\n");
    }
    else
    {
      fmt::print("Current breakpoints:\n");
      process.breakpoint_sites().for_each(
          [](auto& bp)
          {
            fmt::print("{}: address = {:#x}, {}\n",
                       bp.id(),
                       bp.address().addr(),
                       bp.is_enabled() ? "enabled" : "disabled");
          });
    }

    return;
  }

  if (args.size() < 3)
  {
    print_help({"help", "breakpoint"});
    return;
  }

  if (is_prefix(command, "set"))
  {
    auto address = to_integral<std::uint64_t>(args[2], 16);

    if (!address)
    {
      fmt::print(stderr, "Breakpoint command expects address in hexadecimal, prefixed with '0x'\n");
      return;
    }

    process.create_breakpoint_site(mdb::virt_addr{*address}).enable();
    return;
  }

  auto id = to_integral<mdb::breakpoint_site::id_type>(args[2]);
  if (!id)
  {
    std::cerr << "Command expects breakpoint id";
    return;
  }

  if (is_prefix(command, "enable"))
  {
    process.breakpoint_sites().get_by_id(*id).enable();
  }
  else if (is_prefix(command, "disable"))
  {
    process.breakpoint_sites().get_by_id(*id).disable();
  }
  else if (is_prefix(command, "delete"))
  {
    process.breakpoint_sites().remove_by_id(*id);
  }
}

void handle_command(std::unique_ptr<mdb::process>& process, std::string_view line)
{
  auto args    = split(line, ' ');
  auto command = args[0];

  if (is_prefix(command, "continue"))
  {
    process->resume();
    auto reason = process->wait_on_signal();
    print_stop_reason(*process, reason);
  }
  else if (is_prefix(command, "register"))
  {
    handle_register_command(*process, args);
  }
  else if (is_prefix(command, "help"))
  {
    print_help(args);
  }
  else if (is_prefix(command, "breakpoint"))
  {
    handle_breakpoint_command(*process, args);
  }
  else if (is_prefix(command, "step"))
  {
    auto reason = process->step_instruction();
    print_stop_reason(*process, reason);
  }
  else
  {
    std::cerr << "Unknown command\n";
  }
}

void main_loop(std::unique_ptr<mdb::process>& process)
{
  char* line = nullptr;
  while ((line = readline("mdb> ")) != nullptr)
  {
    std::string line_str;

    if (line == std::string_view(""))
    {
      free(line);
      if (history_length > 0)
      {
        line_str = history_list()[history_length - 1]->line;
      }
    }
    else
    {
      line_str = line;
      add_history(line);
      free(line);
    }

    if (!line_str.empty())
    {
      try
      {
        handle_command(process, line_str);
      }
      catch (const mdb::error& err)
      {
        std::cout << err.what() << '\n';
      }
    }
  }
}
}  // namespace

int main(int argc, const char** argv)
{
  if (argc == 1)
  {
    std::cerr << "No arguments given\n";
    return -1;
  }

  try
  {
    auto process = attach(argc, argv);
    main_loop(process);
  }
  catch (const mdb::error& err)
  {
    std::cout << err.what() << '\n';
  }
}