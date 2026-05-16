#pragma once

#include <cstdint>
#include <iosfwd>

namespace simt {

struct Stats {
    std::uint64_t totalCycles = 0;
    std::uint64_t instructionsIssued = 0;
    std::uint64_t instructionsRetired = 0;
    std::uint64_t residentWarps = 0;
    std::uint64_t maxResidentWarps = 0;

    std::uint64_t memoryRequests = 0;
    std::uint64_t coalescedTransactions = 0;
    std::uint64_t memoryLaneAccesses = 0;
    std::uint64_t globalMemoryStallCycles = 0;
    std::uint64_t sharedMemoryBankConflicts = 0;
    std::uint64_t branchDivergenceEvents = 0;
    std::uint64_t branchReconvergenceEvents = 0;
    std::uint64_t schedulerIdleCycles = 0;
    std::uint64_t scoreboardStallCycles = 0;

    double ipc() const;
    double warpOccupancy() const;
    double coalescingEfficiency(std::uint64_t wordsPerTransaction) const;
    void print(std::ostream& os, std::uint64_t wordsPerTransaction) const;
};

} // namespace simt
