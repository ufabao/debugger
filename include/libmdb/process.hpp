#pragma once

#include <sys/types.h>

#include <cstdint>
#include <filesystem>
#include <libmdb/registers.hpp>
#include <memory>

namespace mdb {
enum class process_state { stopped, running, exited, terminated };

struct stop_reason {
  stop_reason(int wait_status);

  process_state reason;
  std::uint8_t  info;
};

class Process {
 public:
  static std::unique_ptr<Process> launch(std::filesystem::path path, bool debug = true);
  static std::unique_ptr<Process> attach(pid_t pid);

  void        resume();
  stop_reason wait_on_signal();

  [[nodiscard]]
  pid_t pid() const {
    return pid_;
  }

  [[nodiscard]]
  Registers& get_registers() {
    return *registers_;
  }

  [[nodiscard]]
  const Registers& get_registers() const {
    return *registers_;
  }

  void write_user_area(std::size_t offset, std::uint64_t data);

  void write_fprs(const user_fpregs_struct& fprs);
  void write_gprs(const user_regs_struct& gprs);

  ~Process();

  Process()                          = delete;
  Process(const Process&)            = delete;
  Process& operator=(const Process&) = delete;

 private:
  Process(pid_t pid, bool terminate_on_end, bool is_attached)
      : pid_(pid),
        terminate_on_end_(terminate_on_end),
        is_attached_(is_attached),
        registers_(new Registers(*this)) {}

  void                       read_all_registers();
  pid_t                      pid_              = 0;
  bool                       terminate_on_end_ = true;
  bool                       is_attached_      = true;
  std::unique_ptr<Registers> registers_;
  process_state              state_ = process_state::stopped;
};
}  // namespace mdb