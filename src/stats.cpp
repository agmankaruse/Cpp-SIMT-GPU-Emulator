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
    os << "Coalescing efficiency: "
       << (coalescingEfficiency(wordsPerTransaction) * 100.0) << "%\n";
    os << "Global memory stalls: " << globalMemoryStallCycles << '\n';
    os << "Shared memory bank conflicts: " << sharedMemoryBankConflicts << '\n';
    os << "Branch divergence events: " << branchDivergenceEvents << '\n';
    os << "Branch reconvergence events: " << branchReconvergenceEvents << '\n';
    os << "Scheduler idle cycles: " << schedulerIdleCycles << '\n';
    os << "Scoreboard stall cycles: " << scoreboardStallCycles << '\n';
}

} // namespace simt
