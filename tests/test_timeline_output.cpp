#include "gpu.hpp"
#include "parser.hpp"

#include <cassert>
#include <fstream>
#include <string>

using namespace simt;

int main() {
    GPUConfig cfg;
    cfg.smCount = 1;
    cfg.warpsPerSM = 1;
    cfg.lanesPerWarp = 4;

    const Program program = Parser::parseText(R"(
.kernel timeline
MOVI r1, 1
ADDI r2, r1, 2
HALT
)");

    const std::string path = "timeline_test.csv";
    GPU gpu(cfg);
    gpu.loadProgram(program);
    gpu.run(false, 100000, path);

    std::ifstream file(path);
    std::string header;
    std::getline(file, header);
    assert(header == "cycle,sm,cta,warp,pc,instruction,event,active_mask,eligible,selected,stall_reason,memory_transaction_count,cache_result,divergence_depth");
    std::string row;
    std::getline(file, row);
    assert(row.find("ISSUE") != std::string::npos);
}
