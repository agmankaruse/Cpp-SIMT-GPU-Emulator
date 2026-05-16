#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace simt {

SchedulerPolicy schedulerPolicyFromString(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (lower == "round-robin" || lower == "rr") {
        return SchedulerPolicy::RoundRobin;
    }
    if (lower == "oldest-ready" || lower == "oldest" || lower == "gto" ||
        lower == "greedy-then-oldest") {
        return SchedulerPolicy::OldestReady;
    }
    throw std::runtime_error("unknown scheduler policy: " + text);
}

std::string toString(SchedulerPolicy policy) {
    switch (policy) {
    case SchedulerPolicy::RoundRobin:
        return "round-robin";
    case SchedulerPolicy::OldestReady:
        return "oldest-ready";
    }
    return "unknown";
}

} // namespace simt
