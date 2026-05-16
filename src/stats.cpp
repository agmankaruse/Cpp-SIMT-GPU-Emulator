#include "stats.hpp"

#include <iomanip>
#include <ostream>

namespace simt {

double Stats::ipc() const {
    if (totalCycles == 0) {
        return 0.0;
    }
    return static_cast<double>(instructionsIssued) / static_cast<double>(totalCycles);
}

double Stats::warpOccupancy() const {
    if (maxResidentWarps == 0) {
        return 0.0;
    }
    return static_cast<double>(residentWarps) / static_cast<double>(maxResidentWarps);
}

double Stats::coalescingEfficiency(std::uint64_t wordsPerTransaction) const {
    if (coalescedTransactions == 0 || wordsPerTransaction == 0) {
        return 1.0;
    }
    const double usefulWords = static_cast<double>(memoryLaneAccesses);
    const double transferredWords =
        static_cast<double>(coalescedTransactions * wordsPerTransaction);
    return usefulWords / transferredWords;
}

double Stats::byteCoalescingEfficiency() const {
    if (transferredBytes == 0) {
        return 1.0;
    }
    return static_cast<double>(requestedBytes) / static_cast<double>(transferredBytes);
}

double Stats::transactionsPerWarpMemoryInstruction() const {
    if (warpMemoryInstructions == 0) {
        return 0.0;
    }
    return static_cast<double>(coalescedTransactions) /
           static_cast<double>(warpMemoryInstructions);
}

double Stats::l1HitRate() const {
    const std::uint64_t total = l1Hits + l1Misses;
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(l1Hits) / static_cast<double>(total);
}

double Stats::issueEfficiency() const {
    if (totalCycles == 0 || residentWarps == 0) {
        return 0.0;
    }
    return static_cast<double>(instructionsIssued) /
           static_cast<double>(totalCycles * residentWarps);
}

double Stats::averageEligibleWarps() const {
    if (eligibleWarpSamples == 0) {
        return 0.0;
    }
    return static_cast<double>(eligibleWarpTotal) /
           static_cast<double>(eligibleWarpSamples);
}

double Stats::averageSharedMemoryConflictDegree() const {
    if (sharedMemoryAccesses == 0) {
        return 1.0;
    }
    return static_cast<double>(sharedMemoryConflictDegreeTotal) /
           static_cast<double>(sharedMemoryAccesses);
}

void Stats::recordSelectedWarp(std::size_t warpId) {
    if (selectedWarpIssues.size() <= warpId) {
        selectedWarpIssues.resize(warpId + 1, 0);
    }
    ++selectedWarpIssues[warpId];
}

void Stats::print(std::ostream& os, std::uint64_t wordsPerTransaction) const {
    os << std::fixed << std::setprecision(3);
    os << "\n=== SIMT GPU Statistics ===\n";
    os << "Total cycles: " << totalCycles << '\n';
    os << "Instructions issued: " << instructionsIssued << '\n';
    os << "Instructions retired: " << instructionsRetired << '\n';
    os << "IPC: " << ipc() << '\n';
    os << "Warp occupancy: " << (warpOccupancy() * 100.0) << "%\n";
    os << "Number of memory requests: " << memoryRequests << '\n';
    os << "Number of coalesced transactions: " << coalescedTransactions << '\n';
    os << "Requested bytes: " << requestedBytes << '\n';
    os << "Transferred bytes: " << transferredBytes << '\n';
    os << "Wasted bytes: " << wastedBytes << '\n';
    os << "Coalescing efficiency: "
       << (byteCoalescingEfficiency() * 100.0) << "%\n";
    os << "Transactions per warp memory instruction: "
       << transactionsPerWarpMemoryInstruction() << '\n';
    os << "L1 hits: " << l1Hits << '\n';
    os << "L1 misses: " << l1Misses << '\n';
    os << "L1 hit rate: " << (l1HitRate() * 100.0) << "%\n";
    os << "Cache miss stalls: " << cacheMissStallCycles << '\n';
    os << "Global memory stalls: " << globalMemoryStallCycles << '\n';
    os << "Shared memory bank conflicts: " << sharedMemoryBankConflicts << '\n';
    os << "Shared memory accesses: " << sharedMemoryAccesses << '\n';
    os << "Shared memory bank conflict events: " << sharedMemoryBankConflictEvents << '\n';
    os << "Shared memory max conflict degree: " << sharedMemoryMaxConflictDegree << '\n';
    os << "Shared memory average conflict degree: "
       << averageSharedMemoryConflictDegree() << '\n';
    os << "Branch divergence events: " << branchDivergenceEvents << '\n';
    os << "Branch reconvergence events: " << branchReconvergenceEvents << '\n';
    os << "Barrier count: " << barrierCount << '\n';
    os << "Barrier stall cycles: " << barrierStallCycles << '\n';
    os << "Scheduler idle cycles: " << schedulerIdleCycles << '\n';
    os << "Scoreboard stall cycles: " << scoreboardStallCycles << '\n';
    os << "Issue efficiency: " << (issueEfficiency() * 100.0) << "%\n";
    os << "Average eligible warps per cycle: " << averageEligibleWarps() << '\n';
    if (!selectedWarpIssues.empty()) {
        os << "Selected warp distribution:";
        for (std::size_t warp = 0; warp < selectedWarpIssues.size(); ++warp) {
            if (selectedWarpIssues[warp] != 0) {
                os << " W" << warp << '=' << selectedWarpIssues[warp];
            }
        }
        os << '\n';
    }
}

} // namespace simt
