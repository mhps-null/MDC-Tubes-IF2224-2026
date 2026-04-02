#pragma once

#include "token.hpp"
#include "dfa.hpp"
#include <string>
#include <vector>

class Lexer
{
public:
    explicit Lexer(const std::string &source);

    std::vector<Token> scan();

private:
    std::string source;
    size_t pos;
    int line;
    int col;

    // proses lexeme yang udah terkumpul dan masukin ke list token
    // ngurusin keyword lookup, charcon vs string, sama skip komentar
    void emitToken(std::vector<Token> &tokens, State state,
                   const std::string &lexeme, int tokenLine, int tokenCol);
};