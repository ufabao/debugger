#include <sys/types.h>

#include <catch2/catch_test_macros.hpp>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <libmdb/error.hpp>
#include <libmdb/process.hpp>

namespace fs = std::filesystem;
namespace {
std::string getTargetPath(const std::string& target) {
#ifdef NDEBUG
  return "../targets/Release/" + target;
#else
  return "../targets/Debug/" + target;
#endif
}
bool process_exists(pid_t pid) {
  auto ret = kill(pid, 0);
  return ret != -1 and errno != ESRCH;
}

char get_process_status(pid_t pid) {
  std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
  std::string   data;
  std::getline(stat, data);
  auto index_of_last_parenthesis = data.rfind(')');
  auto index_of_status_indicator = index_of_last_parenthesis + 2;
  return data[index_of_status_indicator];
}
}  // namespace

TEST_CASE("Process::launch success", "[Process]") {
  auto proc = mdb::Process::launch("yes");
  REQUIRE(process_exists(proc->pid()));
}

TEST_CASE("Process::launch no such program", "[Process]") {
  REQUIRE_THROWS_AS(mdb::Process::launch("you_do_not_have_to_be_good"), mdb::Error);
}

TEST_CASE("Process::attach success", "[Process]") {
  auto target = mdb::Process::launch(getTargetPath("run_endlessly"), false);
  auto proc   = mdb::Process::attach(target->pid());
  REQUIRE(get_process_status(target->pid()) == 't');
}

TEST_CASE("Process::attach no such process", "[Process]") {
  REQUIRE_THROWS_AS(mdb::Process::attach(0), mdb::Error);
}

TEST_CASE("process::resume success", "[Process]") {
  {
    auto proc = mdb::Process::launch(getTargetPath("run_endlessly"));
    proc->resume();
    auto status  = get_process_status(proc->pid());
    auto success = status == 'R' or status == 'S';
    REQUIRE(success);
  }

  {
    auto target = mdb::Process::launch(getTargetPath("run_endlessly"), false);
    auto proc   = mdb::Process::attach(target->pid());
    proc->resume();
    auto status  = get_process_status(proc->pid());
    auto success = status == 'R' or status == 'S';
    REQUIRE(success);
  }
}

TEST_CASE("Process::resume already terminated", "[Process]") {
  auto proc = mdb::Process::launch(getTargetPath("end_immediately"));
  proc->resume();
  proc->wait_on_signal();
  REQUIRE_THROWS_AS(proc->resume(), mdb::Error);
}