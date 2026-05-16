#pragma once

#include "config.hpp"
#include "parser.hpp"

#include <iosfwd>
#include <string>

namespace simt {

struct OccupancyReport {
    std::size_t registersPerThread = 0;
    std::size_t sharedMemoryPerCTA = 0;
    std::size_t warpsPerCTA = 0;
    std::size_t theoreticalActiveWarpsPerSM = 0;
    double occupancyPercent = 0.0;
    std::string limitingFactor = "warp slots";

    void print(std::ostream& os) const;
};

OccupancyReport calculateOccupancy(const GPUConfig& config, const Program& program);

} // namespace simt
