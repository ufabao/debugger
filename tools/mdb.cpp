// clang-format off
#include <cstdio>
// clang-format on
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <libmdb/error.hpp>
#include <libmdb/process.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::unique_ptr<mdb::Process> attach(int argc, const char** argv) {
  if (argc == 3 && argv[1] == std::string_view("-p")) {
    pid_t pid = std::atoi(argv[2]);
    return mdb::Process::attach(pid);
  }

  const char* program_path = argv[1];
  return mdb::Process::launch(program_path);
}

std::vector<std::string> split(std::string_view str, char delimiter) {
  std::vector<std::string> out{};
  std::stringstream        ss{std::string{str}};
  std::string              item;

  while (std::getline(ss, item, delimiter)) {
    out.push_back(item);
  }

  return out;
}
bool is_prefix(std::string_view str, std::string_view of) {
  if (str.size() > of.size()) {
    return false;
  }
  return std::equal(str.begin(), str.end(), of.begin());
}

void resume(pid_t pid) {
  if (ptrace(PTRACE_CONT, pid, nullptr, nullptr) < 0) {
    std::cerr << "Couldn't continue\n";
    std::exit(-1);
  }
}
void wait_on_signal(pid_t pid) {
  int wait_status = 0;
  int options     = 0;
  if (waitpid(pid, &wait_status, options) < 0) {
    std::perror("waitpid failed");
    std::exit(-1);
  }
}

void handle_command(std::unique_ptr<mdb::Process>& process, std::string_view line) {
  auto        args    = split(line, ' ');
  const auto& command = args[0];

  if (is_prefix(command, "continue")) {
    process->resume();
    process->wait_on_signal();
  } else {
    std::cerr << "Unknown command\n";
  }
}

void print_stop_reason(const mdb::Process& process, mdb::stop_reason reason) {
  std::cout << "Process " << process.pid() << " ";

  switch (reason.reason) {
    case mdb::process_state::exited:
      std::cout << "exited with status " << static_cast<int>(reason.info) << '\n';
      break;
    case mdb::process_state::stopped:
      std::cout << "stopped with signal " << static_cast<int>(reason.info) << '\n';
      break;
    case mdb::process_state::terminated:
      std::cout << "terminated with signal " << static_cast<int>(reason.info) << '\n';
      break;
  }

  std::cout << '\n' << std::flush;
}

void main_loop(std::unique_ptr<mdb::Process>& process) {
  char* line = nullptr;
  while ((line = readline("mdb> ")) != nullptr) {
    std::string line_str;

    if (line == std::string_view("")) {
      free(line);
      if (history_length > 0) {
        line_str = history_list()[history_length - 1]->line;
      }
    }

    else {
      line_str = line;
      add_history(line);
      free(line);
    }

    if (!line_str.empty()) {
      try {
        handle_command(process, line_str);
      } catch (const mdb::Error& err) {
        std::cout << err.what() << "\n";
      }
    }
  }
}

}  // namespace

int main(int argc, const char** argv) {
  if (argc == 1) {
    std::cerr << "No arguments given\n";
    return -1;
  }

  try {
    auto process = attach(argc, argv);
    main_loop(process);
  } catch (const mdb::Error& err) {
    std::cout << err.what() << "\n";
    return -1;
  }
}
