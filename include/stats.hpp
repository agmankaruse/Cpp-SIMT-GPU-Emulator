#pragma once

#include <cstdint>
#include <iosfwd>
#include <vector>

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
    std::uint64_t requestedBytes = 0;
    std::uint64_t transferredBytes = 0;
    std::uint64_t wastedBytes = 0;
    std::uint64_t warpMemoryInstructions = 0;
    std::uint64_t globalMemoryStallCycles = 0;
    std::uint64_t cacheMissStallCycles = 0;
    std::uint64_t l1Hits = 0;
    std::uint64_t l1Misses = 0;
    std::uint64_t sharedMemoryBankConflicts = 0;
    std::uint64_t sharedMemoryAccesses = 0;
    std::uint64_t sharedMemoryBankConflictEvents = 0;
    std::uint64_t sharedMemoryMaxConflictDegree = 0;
    std::uint64_t sharedMemoryConflictDegreeTotal = 0;
    std::uint64_t branchDivergenceEvents = 0;
    std::uint64_t branchReconvergenceEvents = 0;
    std::uint64_t barrierCount = 0;
    std::uint64_t barrierStallCycles = 0;
    std::uint64_t schedulerIdleCycles = 0;
    std::uint64_t scoreboardStallCycles = 0;
    std::uint64_t eligibleWarpSamples = 0;
    std::uint64_t eligibleWarpTotal = 0;
    std::vector<std::uint64_t> selectedWarpIssues;

    double ipc() const;
    double warpOccupancy() const;
    double coalescingEfficiency(std::uint64_t wordsPerTransaction) const;
    double byteCoalescingEfficiency() const;
    double transactionsPerWarpMemoryInstruction() const;
    double l1HitRate() const;
    double issueEfficiency() const;
    double averageEligibleWarps() const;
    double averageSharedMemoryConflictDegree() const;
    void recordSelectedWarp(std::size_t warpId);
    void print(std::ostream& os, std::uint64_t wordsPerTransaction) const;
};

} // namespace simt
