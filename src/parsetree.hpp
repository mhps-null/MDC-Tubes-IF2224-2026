#pragma once

#include "node.hpp"
#include <ostream>
#include <memory>

// Menggunakan karakter box-drawing: ├──, └──, │
void printTree(const std::shared_ptr<ParseNode> &root, std::ostream &out);