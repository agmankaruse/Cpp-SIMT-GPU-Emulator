#pragma once

#include <string>
#include <vector>

namespace simt {

class GPU;

struct InvariantViolation {
    std::string rule;
    std::string detail;
};

struct InvariantReport {
    std::vector<InvariantViolation> violations;

    bool passed() const;
    std::string summary() const;
};

class InvariantChecker {
public:
    static InvariantReport check(const GPU& gpu);
};

} // namespace simt
