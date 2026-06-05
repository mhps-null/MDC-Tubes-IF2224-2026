#pragma once
#include "node.hpp"
#include <string>
#include <vector>
#include <memory>
#include <iostream>

// Base Class
struct ASTNode {
    virtual ~ASTNode() = default;
};

// --- Statement & Expression Nodes ---
struct ExprNode : public ASTNode {};
struct StmtNode : public ASTNode {};

struct NumberNode : public ExprNode {
    double value;
    explicit NumberNode(double val) : value(val) {}
};

struct StringNode : public ExprNode {
    std::string value;
    explicit StringNode(const std::string& val) : value(val) {}
};

struct VarNode : public ExprNode {
    std::string name;
    std::vector<std::string> selectors;
    explicit VarNode(const std::string& name) : name(name) {}
};

struct UnaryOpNode : public ExprNode {
    std::string op;
    std::shared_ptr<ExprNode> expr;
    UnaryOpNode(const std::string& op, std::shared_ptr<ExprNode> e) : op(op), expr(e) {}
};

struct BinOpNode : public ExprNode {
    std::string op;
    std::shared_ptr<ExprNode> left;
    std::shared_ptr<ExprNode> right;
    BinOpNode(const std::string& op, std::shared_ptr<ExprNode> l, std::shared_ptr<ExprNode> r)
        : op(op), left(l), right(r) {}
};

struct FuncCallNode : public ExprNode {
    std::string name;
    std::vector<std::shared_ptr<ExprNode>> args;
    FuncCallNode(const std::string& n, const std::vector<std::shared_ptr<ExprNode>>& a) 
        : name(n), args(a) {}
};

struct AssignNode : public StmtNode {
    std::shared_ptr<VarNode> target;
    std::shared_ptr<ExprNode> value;
    AssignNode(std::shared_ptr<VarNode> tgt, std::shared_ptr<ExprNode> val)
        : target(tgt), value(val) {}
};

struct BlockNode : public StmtNode {
    std::vector<std::shared_ptr<StmtNode>> statements;
};

struct WhileNode : public StmtNode {
    std::shared_ptr<ExprNode> condition;
    std::shared_ptr<BlockNode> body;
    WhileNode(std::shared_ptr<ExprNode> cond, std::shared_ptr<BlockNode> b)
        : condition(cond), body(b) {}
};

struct ForNode : public StmtNode {
    std::shared_ptr<AssignNode> assign;
    std::string direction; // "to" atau "downto"
    std::shared_ptr<ExprNode> end_expr;
    std::shared_ptr<BlockNode> body;
    ForNode(std::shared_ptr<AssignNode> assgn, const std::string& dir, 
            std::shared_ptr<ExprNode> endE, std::shared_ptr<BlockNode> b)
        : assign(assgn), direction(dir), end_expr(endE), body(b) {}
};

struct IfNode : public StmtNode {
    std::shared_ptr<ExprNode> condition;
    std::shared_ptr<BlockNode> true_branch;
    std::shared_ptr<BlockNode> false_branch;
    IfNode(std::shared_ptr<ExprNode> cond, std::shared_ptr<BlockNode> tb, std::shared_ptr<BlockNode> fb = nullptr)
        : condition(cond), true_branch(tb), false_branch(fb) {}
};

struct RepeatNode : public StmtNode {
    std::shared_ptr<BlockNode> body;
    std::shared_ptr<ExprNode> condition;
    RepeatNode(std::shared_ptr<BlockNode> b, std::shared_ptr<ExprNode> cond)
        : body(b), condition(cond) {}
};

struct CaseBranchNode : public ASTNode {
    std::vector<std::shared_ptr<ExprNode>> labels;
    std::shared_ptr<StmtNode> statement;
};

struct CaseNode : public StmtNode {
    std::shared_ptr<ExprNode> selector;
    std::vector<std::shared_ptr<CaseBranchNode>> branches;
    explicit CaseNode(std::shared_ptr<ExprNode> sel) : selector(sel) {}
};

struct ProcCallNode : public StmtNode {
    std::string name;
    std::vector<std::shared_ptr<ExprNode>> args;
    ProcCallNode(const std::string& n, const std::vector<std::shared_ptr<ExprNode>>& a) 
        : name(n), args(a) {}
};

// --- Top Level Nodes ---
struct VarDeclNode : public ASTNode {
    std::string name;
    std::string type;
    VarDeclNode(const std::string& n, const std::string& t)
        : name(n), type(t) {}
};

struct ConstDeclNode : public ASTNode {
    std::string name;
    std::shared_ptr<ExprNode> value;
    ConstDeclNode(const std::string& n, std::shared_ptr<ExprNode> v)
        : name(n), value(v) {}
};

struct TypeDeclNode : public ASTNode {
    std::string name;
    std::string type;
    TypeDeclNode(const std::string& n, const std::string& t)
        : name(n), type(t) {}
};

struct ParamNode : public ASTNode {
    std::string name;
    std::string type;
    ParamNode(const std::string& n, const std::string& t)
        : name(n), type(t) {}
};

struct ProcDeclNode : public ASTNode {
    std::string name;
    std::vector<std::shared_ptr<ParamNode>> params;
    std::shared_ptr<BlockNode> body;
    ProcDeclNode(const std::string& n, const std::vector<std::shared_ptr<ParamNode>>& p, std::shared_ptr<BlockNode> b)
        : name(n), params(p), body(b) {}
};

struct FuncDeclNode : public ASTNode {
    std::string name;
    std::vector<std::shared_ptr<ParamNode>> params;
    std::string return_type;
    std::shared_ptr<BlockNode> body;
    FuncDeclNode(const std::string& n, const std::vector<std::shared_ptr<ParamNode>>& p, const std::string& rt, std::shared_ptr<BlockNode> b)
        : name(n), params(p), return_type(rt), body(b) {}
};

struct ProgramNode : public ASTNode {
    std::string name;
    std::vector<std::shared_ptr<ASTNode>> declarations;
    std::shared_ptr<BlockNode> main_block;
};

// --- Fungsi Builder ---
std::shared_ptr<ASTNode> buildAST(std::shared_ptr<ParseNode> root);
std::shared_ptr<ProgramNode> buildProgram(std::shared_ptr<ParseNode> node);
std::shared_ptr<BlockNode> buildBlock(std::shared_ptr<ParseNode> node);
std::shared_ptr<StmtNode> buildStatement(std::shared_ptr<ParseNode> node);
std::shared_ptr<ExprNode> buildExpression(std::shared_ptr<ParseNode> node);

// --- Fungsi Helper untuk Testing ---
void printAST(std::shared_ptr<ASTNode> node, std::ostream& os = std::cout);
