#pragma once

#include <optional>
#include <string>

namespace cpmcprotocol {

enum class RuntimeCommandType {
    Run,
    Stop,
    Pause,
    Reset,
    LatchClear,
    Unlock,
    Lock
};

struct RuntimeRunOption {
    int clear_mode = 0;   // TODO: map to MC protocol clear modes
    bool force_exec = false;
};

struct RuntimeLockOption {
    std::optional<std::string> password;
};

struct RuntimeControl {
    RuntimeCommandType type = RuntimeCommandType::Run;
    std::optional<RuntimeRunOption> run_option;
    std::optional<RuntimeLockOption> lock_option;
};

struct CpuInfo {
    std::string cpu_type;
    std::string cpu_code;
};

} // namespace cpmcprotocol
