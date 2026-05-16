#pragma once

#include "config.hpp"
#include "global_memory.hpp"
#include "parser.hpp"
#include "sm.hpp"
#include "stats.hpp"
#include "trace.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace simt {

class GPU {
public:
    explicit GPU(const GPUConfig& config = GPUConfig{});

    void loadProgram(const Program& program);
    Stats run(bool traceEnabled = false, std::uint64_t maxCycles = 1'000'000,
              const std::string& timelinePath = {});

    GlobalMemory& globalMemory() { return globalMemory_; }
    const GlobalMemory& globalMemory() const { return globalMemory_; }

    StreamingMultiprocessor& sm(std::size_t index) { return sms_.at(index); }
    const StreamingMultiprocessor& sm(std::size_t index) const { return sms_.at(index); }
    std::size_t smCount() const { return sms_.size(); }

    const Trace& trace() const { return trace_; }
    const GPUConfig& config() const { return config_; }

private:
    bool active() const;
    void configureLaunchWarps();

    GPUConfig config_;
    Program program_;
    GlobalMemory globalMemory_;
    std::vector<StreamingMultiprocessor> sms_;
    Trace trace_;
};

} // namespace simt
