#pragma once
#include <string>
#include <vector>
#include <memory>

struct ParseNode {
    std::string label;
    std::vector<std::shared_ptr<ParseNode>> children;

    explicit ParseNode(const std::string &label) : label(label) {}
};
