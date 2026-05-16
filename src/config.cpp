#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace simt {

SchedulerPolicy schedulerPolicyFromString(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    std::replace(lower.begin(), lower.end(), '_', '-');

    if (lower == "round-robin" || lower == "rr") {
        return SchedulerPolicy::RoundRobin;
    }
    if (lower == "greedy-then-oldest" || lower == "gto") {
        return SchedulerPolicy::GreedyThenOldest;
    }
    if (lower == "oldest-ready" || lower == "oldest" || lower == "gto" ||
        lower == "oldestready") {
        return SchedulerPolicy::OldestReady;
    }
    if (lower == "two-level" || lower == "twolevel") {
        return SchedulerPolicy::TwoLevel;
    }
    throw std::runtime_error("unknown scheduler policy: " + text);
}

std::string toString(SchedulerPolicy policy) {
    switch (policy) {
    case SchedulerPolicy::RoundRobin:
        return "round-robin";
    case SchedulerPolicy::GreedyThenOldest:
        return "greedy-then-oldest";
    case SchedulerPolicy::OldestReady:
        return "oldest-ready";
    case SchedulerPolicy::TwoLevel:
        return "two-level";
    }
    return "unknown";
}

namespace {

std::string trim(const std::string& text) {
    const auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c);
    });
    const auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) {
        return std::isspace(c);
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::size_t parseSizeValue(const std::string& value) {
    std::string text = trim(value);
    std::size_t multiplier = 1;
    if (!text.empty()) {
        const char suffix = static_cast<char>(std::tolower(text.back()));
        if (suffix == 'k' || suffix == 'm') {
            multiplier = suffix == 'k' ? 1024u : 1024u * 1024u;
            text.pop_back();
        }
    }
    return static_cast<std::size_t>(std::stoull(text, nullptr, 0) * multiplier);
}

} // namespace

GPUConfig loadGPUConfigFile(const std::string& path, GPUConfig base) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("unable to open GPU config file: " + path);
    }

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            throw std::runtime_error("bad config line " + std::to_string(lineNumber) +
                                     ": expected key=value");
        }
        std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        std::replace(key.begin(), key.end(), '-', '_');

        const auto size = parseSizeValue(value);
        if (key == "sms" || key == "sm_count") {
            base.smCount = size;
        } else if (key == "warps_per_sm") {
            base.warpsPerSM = size;
        } else if (key == "lanes" || key == "lanes_per_warp") {
            base.lanesPerWarp = size;
        } else if (key == "registers_per_lane") {
            base.registersPerLane = size;
        } else if (key == "predicates_per_lane") {
            base.predicatesPerLane = size;
        } else if (key == "shared_memory_size" || key == "shared_memory_bytes_per_sm") {
            base.sharedMemoryBytesPerSM = size;
        } else if (key == "global_memory_size" || key == "global_memory_bytes") {
            base.globalMemoryBytes = size;
        } else if (key == "alu_pipelines" || key == "int_alu_pipelines_per_sm") {
            base.intAluPipelinesPerSM = size;
        } else if (key == "sfu_pipelines" || key == "sfu_pipelines_per_sm") {
            base.sfuPipelinesPerSM = size;
        } else if (key == "lsu_count" || key == "lsu_pipelines_per_sm") {
            base.lsuPipelinesPerSM = size;
        } else if (key == "global_memory_latency") {
            base.globalMemoryLatency = size;
        } else if (key == "shared_memory_latency") {
            base.sharedMemoryLatency = size;
        } else if (key == "max_outstanding_memory_requests") {
            base.maxOutstandingMemoryRequestsPerSM = size;
        } else if (key == "scheduler_policy" || key == "scheduler") {
            base.schedulerPolicy = schedulerPolicyFromString(value);
        } else if (key == "cache_size" || key == "cache_size_bytes") {
            base.cacheSizeBytes = size;
        } else if (key == "cache_line_size" || key == "cache_line_bytes") {
            base.cacheLineBytes = size;
            base.memoryLineBytes = size;
        } else if (key == "cache_associativity") {
            base.cacheAssociativity = size;
        } else if (key == "l1_hit_latency") {
            base.l1HitLatency = size;
        } else if (key == "l1_miss_latency") {
            base.l1MissLatency = size;
            base.globalMemoryLatency = size;
        } else if (key == "grid" || key == "grid_dim") {
            base.gridDim = size;
        } else if (key == "block" || key == "block_dim") {
            base.blockDim = size;
        } else if (key == "register_file_entries_per_sm") {
            base.registerFileEntriesPerSM = size;
        } else if (key == "shared_memory_per_cta" || key == "max_shared_memory_per_cta") {
            base.maxSharedMemoryPerCTA = size;
        } else {
            throw std::runtime_error("unknown config key on line " + std::to_string(lineNumber) +
                                     ": " + key);
        }
    }

    return base;
}

} // namespace simt
