#include "gpu.hpp"
#include "parser.hpp"
#include "scoreboard.hpp"

#include <cassert>

using namespace simt;

int main() {
    Scoreboard scoreboard(8);
    scoreboard.reserve(3);
    assert(scoreboard.isPending(3));
    assert(!scoreboard.canRead({3}));
    assert(scoreboard.canRead({1, 2}));
    scoreboard.release(3);
    assert(scoreboard.canRead({3}));

    GPUConfig cfg;
    cfg.smCount = 1;
    cfg.warpsPerSM = 1;
    cfg.lanesPerWarp = 4;

    const Program program = Parser::parseText(R"(
.kernel scoreboard
MOVI r1, 3
MOVI r2, 4
MUL r3, r1, r2
ADDI r4, r3, 1
HALT
)");

    GPU gpu(cfg);
    gpu.loadProgram(program);
    const Stats stats = gpu.run();
    assert(stats.scoreboardStallCycles > 0);

    const Warp& warp = gpu.sm(0).warp(0);
    for (std::size_t lane = 0; lane < cfg.lanesPerWarp; ++lane) {
        assert(warp.lane(lane).reg(4) == 13);
    }
}
