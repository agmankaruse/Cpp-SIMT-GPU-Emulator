#include "invariant_checker.hpp"

#include "gpu.hpp"

#include <sstream>

namespace simt {
namespace {

void addViolation(InvariantReport& report, const std::string& rule, const std::string& detail) {
    report.violations.push_back({rule, detail});
}

} // namespace

bool InvariantReport::passed() const {
    return violations.empty();
}

std::string InvariantReport::summary() const {
    std::ostringstream out;
    out << (passed() ? "PASS" : "FAIL") << " invariants";
    if (!passed()) {
        out << " violations=" << violations.size();
    }
    out << '\n';
    for (const auto& violation : violations) {
        out << violation.rule << ": " << violation.detail << '\n';
    }
    return out.str();
}

InvariantReport InvariantChecker::check(const GPU& gpu) {
    InvariantReport report;
    const auto& config = gpu.config_;

    for (std::size_t smIndex = 0; smIndex < gpu.sms_.size(); ++smIndex) {
        const auto& sm = gpu.sms_[smIndex];
        for (const auto& request : sm.memoryRequests_) {
            if (request.warpIndex < 0 || static_cast<std::size_t>(request.warpIndex) >= sm.warps_.size()) {
                addViolation(report, "memory_request_warp", "SM" + std::to_string(smIndex) +
                             " has memory request for invalid warp");
            }
            for (const auto address : request.addresses) {
                if (request.space == MemorySpace::Global &&
                    static_cast<std::size_t>(address) + 4 > gpu.globalMemory_.size()) {
                    addViolation(report, "global_memory_bounds", "address " + std::to_string(address));
                }
                if (request.space == MemorySpace::Shared &&
                    static_cast<std::size_t>(address) + 4 > sm.sharedMemory_.size()) {
                    addViolation(report, "shared_memory_bounds", "address " + std::to_string(address));
                }
            }
        }

        for (std::size_t warpIndex = 0; warpIndex < sm.warps_.size(); ++warpIndex) {
            const auto& warp = sm.warps_[warpIndex];
            if (warp.activeMask().size() > config.lanesPerWarp) {
                addViolation(report, "active_mask_width", "warp " + std::to_string(warp.id()) +
                             " mask is wider than configured warp size");
            }
            if (warp.halted() && warp.anyActive()) {
                addViolation(report, "halted_warp_inactive", "warp " + std::to_string(warp.id()) +
                             " is halted but still has active lanes");
            }
            if (warp.waitingOnBarrier()) {
                bool listed = false;
                for (const auto& [cta, barrier] : sm.barriers_) {
                    (void)cta;
                    listed = listed || barrier.waitingWarps.count(static_cast<int>(warpIndex)) != 0;
                }
                if (!listed) {
                    addViolation(report, "barrier_wait_registered", "warp " + std::to_string(warp.id()) +
                                 " waits on a barrier but is not registered");
                }
            }
            if (warp.pc() < gpu.program_.instructions.size()) {
                const auto& inst = gpu.program_.instructions[warp.pc()];
                if (!warp.scoreboard().canRead(inst.sourceRegisters()) && !warp.waitingOnGlobalMemory() &&
                    !warp.waitingOnBarrier()) {
                    for (const auto pending : warp.scoreboard().pendingRegisters()) {
                        if (pending < 0 || static_cast<std::size_t>(pending) >= config.registersPerLane) {
                            addViolation(report, "scoreboard_register_range",
                                         "warp " + std::to_string(warp.id()) + " has invalid pending register");
                        }
                    }
                }
            }
            if (warp.divergenceStack().size() > gpu.program_.instructions.size()) {
                addViolation(report, "divergence_stack_depth", "warp " + std::to_string(warp.id()) +
                             " divergence stack is deeper than program length");
            }
        }
    }

    return report;
}

} // namespace simt
