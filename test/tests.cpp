#include <sys/types.h>

#include <catch2/catch_test_macros.hpp>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <libmdb/bit.hpp>
#include <libmdb/error.hpp>
#include <libmdb/pipe.hpp>
#include <libmdb/process.hpp>

using namespace sdb;

namespace fs = std::filesystem;
namespace
{
std::string getTargetPath(const std::string& target)
{
#ifdef NDEBUG
  return "../targets/Release/" + target;
#else
  return "../targets/Debug/" + target;
#endif
}
bool process_exists(pid_t pid)
{
  auto ret = kill(pid, 0);
  return ret != -1 and errno != ESRCH;
}

char get_process_status(pid_t pid)
{
  std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
  std::string   data;
  std::getline(stat, data);
  auto index_of_last_parenthesis = data.rfind(')');
  auto index_of_status_indicator = index_of_last_parenthesis + 2;
  return data[index_of_status_indicator];
}
}  // namespace

TEST_CASE("Process::launch success", "[Process]")
{
  auto proc = Process::launch("yes");
  REQUIRE(process_exists(proc->pid()));
}

TEST_CASE("Process::launch no such program", "[Process]")
{
  REQUIRE_THROWS_AS(Process::launch("you_do_not_have_to_be_good"), Error);
}

TEST_CASE("Process::attach success", "[Process]")
{
  auto target = Process::launch(getTargetPath("run_endlessly"), false);
  auto proc   = Process::attach(target->pid());
  REQUIRE(get_process_status(target->pid()) == 't');
}

TEST_CASE("Process::attach no such process", "[Process]")
{
  REQUIRE_THROWS_AS(Process::attach(0), Error);
}

TEST_CASE("process::resume success", "[Process]")
{
  {
    auto proc = Process::launch(getTargetPath("run_endlessly"));
    proc->resume();
    auto status  = get_process_status(proc->pid());
    auto success = status == 'R' or status == 'S';
    REQUIRE(success);
  }

  {
    auto target = Process::launch(getTargetPath("run_endlessly"), false);
    auto proc   = Process::attach(target->pid());
    proc->resume();
    auto status  = get_process_status(proc->pid());
    auto success = status == 'R' or status == 'S';
    REQUIRE(success);
  }
}

TEST_CASE("Process::resume already terminated", "[Process]")
{
  auto proc = Process::launch(getTargetPath("end_immediately"));
  proc->resume();
  proc->wait_on_signal();
  REQUIRE_THROWS_AS(proc->resume(), Error);
}

TEST_CASE("Write register works", "[register]")
{
  bool      close_on_exec = false;
  sdb::Pipe channel(close_on_exec);

  auto proc = Process::launch(getTargetPath("reg_write"), true, channel.get_write());
  channel.close_write();

  proc->resume();
  proc->wait_on_signal();

  auto& regs = proc->get_registers();
  regs.write_by_id(register_id::rsi, 0xcafecafe);

  proc->resume();
  proc->wait_on_signal();

  auto output = channel.read();
  REQUIRE(to_string_view(output) == "0xcafecafe");

  regs.write_by_id(register_id::mm0, 0xba5eba11);

  proc->resume();
  proc->wait_on_signal();

  output = channel.read();
  REQUIRE(to_string_view(output) == "0xba5eba11");

  regs.write_by_id(register_id::xmm0, 42.24);

  proc->resume();
  proc->wait_on_signal();

  output = channel.read();
  REQUIRE(to_string_view(output) == "42.24");

  // regs.write_by_id(register_id::st0, 42.24l);
  // regs.write_by_id(register_id::fsw, std::uint16_t{0b0011100000000000});
  // regs.write_by_id(register_id::ftw, std::uint16_t{0b0011111111111111});

  // proc->resume();
  // proc->wait_on_signal();

  // output = channel.read();
  // REQUIRE(to_string_view(output) == "42.24");
}

TEST_CASE("Read register works", "[Register]")
{
  auto  proc = Process::launch(getTargetPath("reg_read"));
  auto& regs = proc->get_registers();

  proc->resume();
  proc->wait_on_signal();

  REQUIRE(regs.read_by_id_as<std::uint64_t>(register_id::r13) == 0xcafecafe);

  proc->resume();
  proc->wait_on_signal();

  REQUIRE(regs.read_by_id_as<std::uint8_t>(register_id::r13b) == 42);

  proc->resume();
  proc->wait_on_signal();

  REQUIRE(regs.read_by_id_as<byte64>(register_id::mm0) == to_byte64(0xba5eba11ull));

  // proc->resume();
  // proc->wait_on_signal();

  // REQUIRE(regs.read_by_id_as<long double>(register_id::st0) == 64.125L);
}