#pragma once

#include "token.hpp"
#include "node.hpp"
#include <vector>
#include <memory>

// inisialisasi parser dengan token dari lexer
void initParser(const std::vector<Token> &tokens);

// mulai parsing dan balikin root parse tree
std::shared_ptr<ParseNode> parse();

// cek ada error sintaks selama parsing atau tidak
bool hadError();
