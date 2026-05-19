#pragma once

#include "node.hpp"
#include <memory>
#include <ostream>
#include <string>
#include <vector>

struct SemanticNode
{
    std::string label;
    std::vector<std::string> annotations;
    std::vector<std::shared_ptr<SemanticNode>> children;

    explicit SemanticNode(const std::string &label) : label(label) {}
};

struct TabEntry
{
    std::string identifier;
    int link = 0;
    std::string obj;
    std::string type;
    int ref = 0;
    int nrm = 1;
    int lev = 0;
    int adr = 0;
    bool initialized = false;
};

struct BTabEntry
{
    int last = 0;
    int lpar = 0;
    int psze = 0;
    int vsze = 0;
};

struct ATabEntry
{
    std::string xtyp;
    std::string etyp;
    int eref = 0;
    int low = 0;
    int high = 0;
    int elsz = 1;
    int size = 0;
};

struct SemanticResult
{
    std::shared_ptr<SemanticNode> ast;
    std::vector<TabEntry> tab;
    std::vector<BTabEntry> btab;
    std::vector<ATabEntry> atab;
    std::vector<std::string> errors;
};

SemanticResult analyzeSemantics(const std::shared_ptr<ParseNode> &root);
void printSemanticResult(const SemanticResult &result, std::ostream &out);
