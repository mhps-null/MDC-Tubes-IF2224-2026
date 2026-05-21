#include "ast.hpp"
#include <functional>
#include <iostream>

static std::string extractValue(const std::string& label) {
    auto pos1 = label.find('(');
    auto pos2 = label.rfind(')');
    if (pos1 != std::string::npos && pos2 != std::string::npos && pos2 > pos1) {
        return label.substr(pos1 + 1, pos2 - pos1 - 1);
    }
    return "";
}

static std::vector<std::string> splitLines(const std::string& str) {
    std::vector<std::string> res;
    size_t start = 0;
    size_t pos = str.find('\n');
    while (pos != std::string::npos) {
        res.push_back(str.substr(start, pos - start));
        start = pos + 1;
        pos = str.find('\n', start);
    }
    res.push_back(str.substr(start));
    return res;
}

static std::shared_ptr<ExprNode> buildSimpleExpression(std::shared_ptr<ParseNode> node);
static std::shared_ptr<ExprNode> buildTerm(std::shared_ptr<ParseNode> node);
static std::shared_ptr<ExprNode> buildFactor(std::shared_ptr<ParseNode> node);
static std::shared_ptr<ExprNode> buildConstant(std::shared_ptr<ParseNode> node);
static std::string buildTypeText(std::shared_ptr<ParseNode> node);
static std::shared_ptr<VarNode> buildVariable(std::shared_ptr<ParseNode> node);

std::shared_ptr<ASTNode> buildAST(std::shared_ptr<ParseNode> root) {
    if (!root) return nullptr;
    if (root->label == "<program>") {
        return buildProgram(root);
    }
    return nullptr;
}

static std::vector<std::shared_ptr<ParamNode>> buildFormalParams(std::shared_ptr<ParseNode> paramList) {
    std::vector<std::shared_ptr<ParamNode>> params;
    if (!paramList) return params;
    
    for (auto& child : paramList->children) {
        if (child->label == "<parameter-group>") {
            if (child->children.size() >= 3) {
                auto idList = child->children[0];
                std::vector<std::string> names;
                for (auto& idNode : idList->children) {
                    if (idNode->label.find("ident(") == 0) {
                        names.push_back(extractValue(idNode->label));
                    }
                }
                
                std::string typeName = "unknown";
                auto typeNode = child->children[2];
                if (typeNode->label == "<type>") {
                    if (!typeNode->children.empty()) {
                        typeName = extractValue(typeNode->children[0]->label);
                    }
                } else if (typeNode->label.find("ident(") == 0) {
                    typeName = extractValue(typeNode->label);
                }
                
                for (const auto& name : names) {
                    params.push_back(std::make_shared<ParamNode>(name, typeName));
                }
            }
        }
    }
    return params;
}

std::shared_ptr<ProgramNode> buildProgram(std::shared_ptr<ParseNode> node) {
    auto prog = std::make_shared<ProgramNode>();
    
    if (node->children.size() >= 3) {
        auto header = node->children[0];
        if (header->children.size() >= 2) {
            prog->name = extractValue(header->children[1]->label);
        }
        
        auto declPart = node->children[1];
        for (auto& decl : declPart->children) {
            if (decl->label == "<const-declaration>") {
                for (size_t i = 1; i + 2 < decl->children.size(); i += 4) {
                    auto name = extractValue(decl->children[i]->label);
                    prog->declarations.push_back(std::make_shared<ConstDeclNode>(name, buildConstant(decl->children[i + 2])));
                }
            }
            else if (decl->label == "<type-declaration>") {
                for (size_t i = 1; i + 2 < decl->children.size(); i += 4) {
                    auto name = extractValue(decl->children[i]->label);
                    prog->declarations.push_back(std::make_shared<TypeDeclNode>(name, buildTypeText(decl->children[i + 2])));
                }
            }
            else if (decl->label == "<var-declaration>") {
                for (size_t i = 1; i < decl->children.size(); i++) {
                    if (decl->children[i]->label == "<identifier-list>") {
                        auto idList = decl->children[i];
                        std::vector<std::string> vars;
                        for (auto& idNode : idList->children) {
                            if (idNode->label.find("ident(") == 0) {
                                vars.push_back(extractValue(idNode->label));
                            }
                        }
                        
                        std::string typeName = "unknown";
                        if (i + 2 < decl->children.size() && decl->children[i+2]->label == "<type>") {
                            auto typeNode = decl->children[i+2];
                            if (!typeNode->children.empty()) {
                                typeName = buildTypeText(typeNode);
                            }
                        }
                        
                        for (const auto& var : vars) {
                            prog->declarations.push_back(std::make_shared<VarDeclNode>(var, typeName));
                        }
                    }
                }
            }
            else if (decl->label == "<subprogram-declaration>") {
                if (!decl->children.empty()) {
                    auto sub = decl->children[0];
                    if (sub->label == "<procedure-declaration>") {
                        std::string procName;
                        std::vector<std::shared_ptr<ParamNode>> params;
                        std::shared_ptr<BlockNode> body;
                        
                        for (auto& child : sub->children) {
                            if (child->label.find("ident(") == 0) {
                                procName = extractValue(child->label);
                            } else if (child->label == "<formal-parameter-list>") {
                                params = buildFormalParams(child);
                            } else if (child->label == "<block>") {
                                if (child->children.size() >= 2) {
                                    body = buildBlock(child->children[1]); 
                                }
                            }
                        }
                        prog->declarations.push_back(std::make_shared<ProcDeclNode>(procName, params, body));
                    } 
                    else if (sub->label == "<function-declaration>") {
                        std::string funcName;
                        std::string retType;
                        std::vector<std::shared_ptr<ParamNode>> params;
                        std::shared_ptr<BlockNode> body;
                        
                        bool colonFound = false;
                        for (auto& child : sub->children) {
                            if (child->label.find("ident(") == 0) {
                                if (!colonFound) funcName = extractValue(child->label);
                                else retType = extractValue(child->label);
                            } else if (child->label == "colon") {
                                colonFound = true;
                            } else if (child->label == "<formal-parameter-list>") {
                                params = buildFormalParams(child);
                            } else if (child->label == "<block>") {
                                if (child->children.size() >= 2) {
                                    body = buildBlock(child->children[1]);
                                }
                            }
                        }
                        prog->declarations.push_back(std::make_shared<FuncDeclNode>(funcName, params, retType, body));
                    }
                }
            }
        }
        
        prog->main_block = buildBlock(node->children[2]);
    }
    
    return prog;
}

std::shared_ptr<BlockNode> buildBlock(std::shared_ptr<ParseNode> node) {
    auto block = std::make_shared<BlockNode>();
    if (node->children.size() >= 2) {
        auto stmtList = node->children[1];
        for (auto& stmtNode : stmtList->children) {
            if (stmtNode->label.find("<") == 0 && stmtNode->label != "<statement-list>") {
                auto astStmt = buildStatement(stmtNode);
                if (astStmt) {
                    block->statements.push_back(astStmt);
                }
            }
        }
    }
    return block;
}

std::shared_ptr<StmtNode> buildStatement(std::shared_ptr<ParseNode> node) {
    if (!node) return nullptr;
    
    std::shared_ptr<ParseNode> actualStmt = node;
    if (node->label == "<statement>" && !node->children.empty()) {
        actualStmt = node->children[0];
    }

    if (actualStmt->label == "<assignment-statement>") {
        if (actualStmt->children.size() >= 3) {
            auto target = buildVariable(actualStmt->children[0]);
            auto value = buildExpression(actualStmt->children[2]);
            return std::make_shared<AssignNode>(target, value);
        }
    }
    else if (actualStmt->label == "<procedure/function-call>") {
        auto name = extractValue(actualStmt->children[0]->label);
        std::vector<std::shared_ptr<ExprNode>> args;
        for (auto& child : actualStmt->children) {
            if (child->label == "<parameter-list>") {
                for (auto& p : child->children) {
                    if (p->label == "<expression>") {
                        args.push_back(buildExpression(p));
                    }
                }
            }
        }
        return std::make_shared<ProcCallNode>(name, args);
    }
    else if (actualStmt->label == "<while-statement>") {
        if (actualStmt->children.size() >= 4) {
            auto cond = buildExpression(actualStmt->children[1]);
            auto body = buildBlock(actualStmt->children[3]);
            return std::make_shared<WhileNode>(cond, body);
        }
    }
    else if (actualStmt->label == "<for-statement>") {
        if (actualStmt->children.size() >= 8) {
            auto varName = extractValue(actualStmt->children[1]->label);
            auto target = std::make_shared<VarNode>(varName);
            auto startExpr = buildExpression(actualStmt->children[3]);
            auto assign = std::make_shared<AssignNode>(target, startExpr);
            
            std::string dir = (actualStmt->children[4]->label == "tosy") ? "to" : "downto";
            auto endExpr = buildExpression(actualStmt->children[5]);
            auto body = buildBlock(actualStmt->children[7]);
            
            return std::make_shared<ForNode>(assign, dir, endExpr, body);
        }
    }
    else if (actualStmt->label == "<if-statement>") {
        if (actualStmt->children.size() >= 4) {
            auto cond = buildExpression(actualStmt->children[1]);
            
            auto trueStmtNode = actualStmt->children[3];
            auto trueBlock = std::make_shared<BlockNode>();
            if (trueStmtNode->label == "<compound-statement>") {
                trueBlock = buildBlock(trueStmtNode);
            } else {
                auto stmtAst = buildStatement(trueStmtNode);
                if (stmtAst) trueBlock->statements.push_back(stmtAst);
            }

            std::shared_ptr<BlockNode> falseBlock = nullptr;
            if (actualStmt->children.size() >= 6) {
                auto falseStmtNode = actualStmt->children[5];
                falseBlock = std::make_shared<BlockNode>();
                if (falseStmtNode->label == "<compound-statement>") {
                    falseBlock = buildBlock(falseStmtNode);
                } else {
                    auto stmtAst = buildStatement(falseStmtNode);
                    if (stmtAst) falseBlock->statements.push_back(stmtAst);
                }
            }
            return std::make_shared<IfNode>(cond, trueBlock, falseBlock);
        }
    }
    else if (actualStmt->label == "<repeat-statement>") {
        if (actualStmt->children.size() >= 4) {
            auto body = std::make_shared<BlockNode>();
            auto stmtList = actualStmt->children[1];
            if (stmtList && stmtList->label == "<statement-list>") {
                for (auto& child : stmtList->children) {
                    auto stmt = buildStatement(child);
                    if (stmt) body->statements.push_back(stmt);
                }
            }
            return std::make_shared<RepeatNode>(body, buildExpression(actualStmt->children[3]));
        }
    }
    else if (actualStmt->label == "<case-statement>") {
        if (actualStmt->children.size() >= 4) {
            auto node = std::make_shared<CaseNode>(buildExpression(actualStmt->children[1]));
            std::function<void(std::shared_ptr<ParseNode>)> collectBranches = [&](std::shared_ptr<ParseNode> caseBlock) {
                if (!caseBlock || caseBlock->label != "<case-block>") return;
                auto branch = std::make_shared<CaseBranchNode>();
                for (auto& child : caseBlock->children) {
                    if (child->label == "<constant>") branch->labels.push_back(buildConstant(child));
                    else if (child->label == "<case-block>") collectBranches(child);
                    else {
                        auto stmt = buildStatement(child);
                        if (stmt) branch->statement = stmt;
                    }
                }
                if (!branch->labels.empty() || branch->statement) node->branches.push_back(branch);
            };
            collectBranches(actualStmt->children[3]);
            return node;
        }
    }
    else if (actualStmt->label == "<compound-statement>") {
        return buildBlock(actualStmt);
    }
    return nullptr;
}

static std::shared_ptr<VarNode> buildVariable(std::shared_ptr<ParseNode> node) {
    if (!node || node->children.empty()) return std::make_shared<VarNode>("unknown_var");
    auto var = std::make_shared<VarNode>(extractValue(node->children[0]->label));
    for (size_t i = 1; i < node->children.size(); ++i) {
        auto comp = node->children[i];
        if (!comp || comp->label != "<component-variable>" || comp->children.empty()) continue;
        if (comp->children[0]->label == "period" && comp->children.size() > 1) {
            var->selectors.push_back("." + extractValue(comp->children[1]->label));
        } else if (comp->children[0]->label == "lbrack") {
            var->selectors.push_back("[]");
        }
    }
    return var;
}

std::shared_ptr<ExprNode> buildExpression(std::shared_ptr<ParseNode> node) {
    if (!node || node->children.empty()) return nullptr;
    
    auto left = buildSimpleExpression(node->children[0]);
    if (node->children.size() >= 3) {
        std::string op = node->children[1]->label;
        auto right = buildSimpleExpression(node->children[2]);
        return std::make_shared<BinOpNode>(op, left, right);
    }
    return left; // Passthrough empty chain
}

static std::shared_ptr<ExprNode> buildSimpleExpression(std::shared_ptr<ParseNode> node) {
    if (!node || node->children.empty()) return nullptr;
    
    size_t i = 0;
    std::string sign = "";
    if (node->children[0]->label == "plus" || node->children[0]->label == "minus") {
        sign = node->children[0]->label;
        i++;
    }
    
    auto left = buildTerm(node->children[i]);
    if (sign == "minus" || sign == "plus") {
        left = std::make_shared<UnaryOpNode>(sign, left);
    }
    i++;
    
    while (i + 1 < node->children.size()) {
        std::string op = node->children[i]->label; 
        auto right = buildTerm(node->children[i+1]);
        left = std::make_shared<BinOpNode>(op, left, right);
        i += 2;
    }
    
    return left; // Passthrough empty chain
}

static std::shared_ptr<ExprNode> buildTerm(std::shared_ptr<ParseNode> node) {
    if (!node || node->children.empty()) return nullptr;
    
    auto left = buildFactor(node->children[0]);
    size_t i = 1;
    
    while (i + 1 < node->children.size()) {
        std::string op = node->children[i]->label; 
        auto right = buildFactor(node->children[i+1]);
        left = std::make_shared<BinOpNode>(op, left, right);
        i += 2;
    }
    
    return left; // Passthrough empty chain
}

static std::shared_ptr<ExprNode> buildFactor(std::shared_ptr<ParseNode> node) {
    if (!node || node->children.empty()) return nullptr;
    
    auto child = node->children[0];
    
    if (child->label.find("ident(") == 0) {
        return std::make_shared<VarNode>(extractValue(child->label));
    }
    else if (child->label == "<variable>") {
        return buildVariable(child);
    }
    else if (child->label.find("intcon(") == 0 || child->label.find("realcon(") == 0) {
        return std::make_shared<NumberNode>(std::stod(extractValue(child->label)));
    }
    else if (child->label.find("string(") == 0 || child->label.find("charcon(") == 0) {
        return std::make_shared<StringNode>(extractValue(child->label));
    }
    else if (child->label == "<expression>") {
        return buildExpression(child);
    }
    else if (child->label == "lparent" && node->children.size() >= 3) {
         return buildExpression(node->children[1]);
    }
    else if (child->label == "notsy" && node->children.size() >= 2) {
        auto expr = buildFactor(node->children[1]);
        return std::make_shared<UnaryOpNode>("notsy", expr);
    }
    else if (child->label == "<procedure/function-call>") {
        auto name = extractValue(child->children[0]->label);
        std::vector<std::shared_ptr<ExprNode>> args;
        for (auto& pList : child->children) {
            if (pList->label == "<parameter-list>") {
                for (auto& p : pList->children) {
                    if (p->label == "<expression>") {
                        args.push_back(buildExpression(p));
                    }
                }
            }
        }
        return std::make_shared<FuncCallNode>(name, args);
    }
    
    return std::make_shared<VarNode>("unknown_factor");
}

static std::shared_ptr<ExprNode> buildConstant(std::shared_ptr<ParseNode> node) {
    if (!node) return nullptr;
    if (node->label == "<constant>") {
        if (node->children.size() == 2) {
            return std::make_shared<UnaryOpNode>(node->children[0]->label, buildConstant(node->children[1]));
        }
        if (!node->children.empty()) return buildConstant(node->children.back());
    }
    if (node->label.find("intcon(") == 0 || node->label.find("realcon(") == 0) {
        return std::make_shared<NumberNode>(std::stod(extractValue(node->label)));
    }
    if (node->label.find("string(") == 0 || node->label.find("charcon(") == 0) {
        return std::make_shared<StringNode>(extractValue(node->label));
    }
    if (node->label.find("ident(") == 0) {
        return std::make_shared<VarNode>(extractValue(node->label));
    }
    if (node->label == "<expression>") return buildExpression(node);
    return nullptr;
}

static std::string buildTypeText(std::shared_ptr<ParseNode> node) {
    if (!node) return "unknown";
    if (node->label == "<type>" && !node->children.empty()) return buildTypeText(node->children[0]);
    if (node->label.find("ident(") == 0) return extractValue(node->label);
    if (node->label == "<range>") return "subrange";
    if (node->label == "<enumerated>") return "enumerated";
    if (node->label == "<record-type>") return "record";
    if (node->label == "<array-type>") {
        std::string elem = "unknown";
        for (auto& child : node->children) {
            if (child->label == "<type>") elem = buildTypeText(child);
        }
        return "array of " + elem;
    }
    return "unknown";
}

// ==========================================
// AST FORMATTER & PRINTER
// ==========================================

std::string formatExpr(std::shared_ptr<ExprNode> node, const std::string& baseIndent) {
    if (!node) return "null";
    if (auto v = std::dynamic_pointer_cast<VarNode>(node)) {
        std::string name = v->name;
        for (const auto& selector : v->selectors) name += selector;
        return "Var('" + name + "')";
    }
    if (auto n = std::dynamic_pointer_cast<NumberNode>(node)) {
        std::string val = std::to_string(n->value);
        val.erase(val.find_last_not_of('0') + 1, std::string::npos);
        if(val.back() == '.') val.pop_back();
        return "Num(" + val + ")";
    }
    if (auto s = std::dynamic_pointer_cast<StringNode>(node)) return "String('" + s->value + "')";
    
    if (auto b = std::dynamic_pointer_cast<BinOpNode>(node)) {
        std::string res = "BinOp(op: '" + b->op + "',\n";
        res += baseIndent + "      left: " + formatExpr(b->left, baseIndent + "            ") + ",\n";
        res += baseIndent + "      right: " + formatExpr(b->right, baseIndent + "             ") + ")";
        return res;
    }
    if (auto u = std::dynamic_pointer_cast<UnaryOpNode>(node)) {
        std::string res = "UnaryOp(op: '" + u->op + "',\n";
        res += baseIndent + "        expr: " + formatExpr(u->expr, baseIndent + "              ") + ")";
        return res;
    }
    if (auto fc = std::dynamic_pointer_cast<FuncCallNode>(node)) {
        std::string res = "FunctionCall(name: '" + fc->name + "',\n";
        res += baseIndent + "             args: [";
        std::string nextIndent = baseIndent + "                    ";
        for (size_t i = 0; i < fc->args.size(); i++) {
            res += formatExpr(fc->args[i], nextIndent);
            if (i + 1 < fc->args.size()) res += ", ";
        }
        res += "])";
        return res;
    }
    return "UnknownExpr";
}

std::vector<std::string> formatStmt(std::shared_ptr<StmtNode> node) {
    if (!node) return {};
    if (auto a = std::dynamic_pointer_cast<AssignNode>(node)) {
        std::string line1 = "Assign(target: " + formatExpr(a->target, "") + ",";
        std::string valStr = formatExpr(a->value, "       ");
        std::string full = line1 + "\n       value: " + valStr + ")";
        return splitLines(full);
    }
    if (auto pc = std::dynamic_pointer_cast<ProcCallNode>(node)) {
        std::string line1 = "ProcedureCall(name: '" + pc->name + "',";
        std::string full = line1 + "\n              args: [";
        std::string nextIndent = "                     ";
        for(size_t i=0; i<pc->args.size(); i++){
            full += formatExpr(pc->args[i], nextIndent);
            if(i+1 < pc->args.size()) full += ", ";
        }
        full += "])";
        return splitLines(full);
    }
    if (auto w = std::dynamic_pointer_cast<WhileNode>(node)) {
        std::string full = "While(cond: " + formatExpr(w->condition, "") + ")";
        return splitLines(full);
    }
    if (auto f = std::dynamic_pointer_cast<ForNode>(node)) {
        std::string full = "For(dir: '" + f->direction + "',\n    end: " + formatExpr(f->end_expr, "         ") + ")";
        return splitLines(full);
    }
    if (auto i = std::dynamic_pointer_cast<IfNode>(node)) {
        std::string full = "If(cond: " + formatExpr(i->condition, "") + ")";
        return splitLines(full);
    }
    if (auto r = std::dynamic_pointer_cast<RepeatNode>(node)) {
        std::string full = "Repeat(until: " + formatExpr(r->condition, "") + ")";
        return splitLines(full);
    }
    if (auto c = std::dynamic_pointer_cast<CaseNode>(node)) {
        std::string full = "Case(selector: " + formatExpr(c->selector, "") + ")";
        return splitLines(full);
    }
    return {"UnknownStmt"};
}

std::string getInnerPref(const std::string& prefix, bool isLast) {
    return prefix + "  " + (isLast ? "    " : "|   ");
}

void printSection(std::ostream& os, const std::string& title, const std::string& prefix, bool isLastTitle) {
    os << prefix << "  |\n";
    os << prefix << "  " << (isLastTitle ? "\\-- " : "+-- ") << title << "\n";
}

void printChild(std::ostream& os, const std::string& prefix, const std::vector<std::string>& lines, bool isLast) {
    if (lines.empty()) return;
    os << prefix << "  |\n";
    std::string currentPrefix = prefix + "  " + (isLast ? "\\-- " : "+-- ");
    std::string nextPrefix    = prefix + "  " + (isLast ? "    " : "|   ");
    
    os << currentPrefix << lines[0] << "\n";
    for (size_t i = 1; i < lines.size(); i++) {
        os << nextPrefix << lines[i] << "\n";
    }
}

void printStmt(std::shared_ptr<StmtNode> node, std::ostream& os, const std::string& prefix, bool isLast);

void printChildBlock(std::ostream& os, const std::string& title, const std::vector<std::shared_ptr<StmtNode>>& stmts, const std::string& prefix, bool isLastTitle) {
    printSection(os, title, prefix, isLastTitle);
    std::string childPrefix = getInnerPref(prefix, isLastTitle);
    
    for (size_t i = 0; i < stmts.size(); i++) {
        bool lastStmt = (i == stmts.size() - 1);
        printStmt(stmts[i], os, childPrefix, lastStmt);
    }
}

void printStmt(std::shared_ptr<StmtNode> node, std::ostream& os, const std::string& prefix, bool isLast) {
    if (!node) return;
    
    if (auto a = std::dynamic_pointer_cast<AssignNode>(node)) {
        printChild(os, prefix, formatStmt(a), isLast);
    }
    else if (auto pc = std::dynamic_pointer_cast<ProcCallNode>(node)) {
        printChild(os, prefix, formatStmt(pc), isLast);
    }
    else if (auto w = std::dynamic_pointer_cast<WhileNode>(node)) {
        printChild(os, prefix, formatStmt(w), isLast);
        std::string innerPrefix = getInnerPref(prefix, isLast);
        printChildBlock(os, "Body", w->body ? w->body->statements : std::vector<std::shared_ptr<StmtNode>>(), innerPrefix, true);
    }
    else if (auto f = std::dynamic_pointer_cast<ForNode>(node)) {
        printChild(os, prefix, formatStmt(f), isLast);
        std::string innerPrefix = getInnerPref(prefix, isLast);
        printStmt(f->assign, os, innerPrefix, false);
        printChildBlock(os, "Body", f->body ? f->body->statements : std::vector<std::shared_ptr<StmtNode>>(), innerPrefix, true);
    }
    else if (auto i = std::dynamic_pointer_cast<IfNode>(node)) {
        printChild(os, prefix, formatStmt(i), isLast);
        std::string innerPrefix = getInnerPref(prefix, isLast);
        
        bool hasFalse = i->false_branch != nullptr;
        printChildBlock(os, "TrueBranch", i->true_branch ? i->true_branch->statements : std::vector<std::shared_ptr<StmtNode>>(), innerPrefix, !hasFalse);
        if (hasFalse) {
            printChildBlock(os, "FalseBranch", i->false_branch->statements, innerPrefix, true);
        }
    }
    else if (auto r = std::dynamic_pointer_cast<RepeatNode>(node)) {
        printChild(os, prefix, formatStmt(r), isLast);
        std::string innerPrefix = getInnerPref(prefix, isLast);
        printChildBlock(os, "Body", r->body ? r->body->statements : std::vector<std::shared_ptr<StmtNode>>(), innerPrefix, true);
    }
    else if (auto c = std::dynamic_pointer_cast<CaseNode>(node)) {
        printChild(os, prefix, formatStmt(c), isLast);
        std::string innerPrefix = getInnerPref(prefix, isLast);
        for (size_t i = 0; i < c->branches.size(); ++i) {
            auto branch = c->branches[i];
            std::vector<std::string> lines;
            std::string line = "Branch(labels: [";
            for (size_t j = 0; j < branch->labels.size(); ++j) {
                if (j) line += ", ";
                line += formatExpr(branch->labels[j], "");
            }
            line += "])";
            lines.push_back(line);
            printChild(os, innerPrefix, lines, i + 1 == c->branches.size());
            if (branch->statement) {
                printStmt(branch->statement, os, getInnerPref(innerPrefix, i + 1 == c->branches.size()), true);
            }
        }
    }
}

void printAST(std::shared_ptr<ASTNode> node, std::ostream& os) {
    if (!node) return;
    
    if (auto p = std::dynamic_pointer_cast<ProgramNode>(node)) {
        os << "ProgramNode(name: '" << p->name << "')\n";
        
        bool hasBlock = p->main_block && !p->main_block->statements.empty();
        
        if (!p->declarations.empty()) {
            printSection(os, "Declarations", "", !hasBlock);
            std::string declPrefix = getInnerPref("", !hasBlock);
            for (size_t i = 0; i < p->declarations.size(); i++) {
                bool lastDecl = (i == p->declarations.size() - 1);
                auto declNode = p->declarations[i];
                
                if (auto v = std::dynamic_pointer_cast<VarDeclNode>(declNode)) {
                    printChild(os, declPrefix, {"VarDecl(name: '" + v->name + "', type: '" + v->type + "')"}, lastDecl);
                }
                else if (auto c = std::dynamic_pointer_cast<ConstDeclNode>(declNode)) {
                    printChild(os, declPrefix, {"ConstDecl(name: '" + c->name + "', value: " + formatExpr(c->value, "") + ")"}, lastDecl);
                }
                else if (auto t = std::dynamic_pointer_cast<TypeDeclNode>(declNode)) {
                    printChild(os, declPrefix, {"TypeDecl(name: '" + t->name + "', type: '" + t->type + "')"}, lastDecl);
                }
                else if (auto pd = std::dynamic_pointer_cast<ProcDeclNode>(declNode)) {
                    printChild(os, declPrefix, {"ProcedureDecl(name: '" + pd->name + "')"}, lastDecl);
                    std::string innerPref = getInnerPref(declPrefix, lastDecl);
                    
                    if (!pd->params.empty()) {
                        printSection(os, "Params", innerPref, false);
                        std::string paramPref = getInnerPref(innerPref, false);
                        for(size_t j=0; j<pd->params.size(); j++){
                            bool lastP = (j == pd->params.size()-1);
                            printChild(os, paramPref, {"Param(name: '" + pd->params[j]->name + "', type: '" + pd->params[j]->type + "')"}, lastP);
                        }
                    }
                    printChildBlock(os, "Body", pd->body ? pd->body->statements : std::vector<std::shared_ptr<StmtNode>>(), innerPref, true);
                }
                else if (auto fd = std::dynamic_pointer_cast<FuncDeclNode>(declNode)) {
                    printChild(os, declPrefix, {"FunctionDecl(name: '" + fd->name + "', returnType: '" + fd->return_type + "')"}, lastDecl);
                    std::string innerPref = getInnerPref(declPrefix, lastDecl);
                    
                    if (!fd->params.empty()) {
                        printSection(os, "Params", innerPref, false);
                        std::string paramPref = getInnerPref(innerPref, false);
                        for(size_t j=0; j<fd->params.size(); j++){
                            bool lastP = (j == fd->params.size()-1);
                            printChild(os, paramPref, {"Param(name: '" + fd->params[j]->name + "', type: '" + fd->params[j]->type + "')"}, lastP);
                        }
                    }
                    printChildBlock(os, "Body", fd->body ? fd->body->statements : std::vector<std::shared_ptr<StmtNode>>(), innerPref, true);
                }
            }
        }
        
        if (hasBlock) {
            printSection(os, "Block", "", true);
            std::string blockPrefix = getInnerPref("", true);
            for (size_t i = 0; i < p->main_block->statements.size(); i++) {
                bool lastStmt = (i == p->main_block->statements.size() - 1);
                printStmt(p->main_block->statements[i], os, blockPrefix, lastStmt);
            }
        }
    }
}
