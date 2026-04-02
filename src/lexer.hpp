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

    /**
     * Mengubah lexeme hasil DFA menjadi token dan menambahkannya ke list.
     *
     * - Identifier dicek apakah keyword atau bukan.
     * - String literal diproses (handle escape '' jadi ').
     * - Angka langsung dijadikan token sesuai tipe.
     * - Komentar diabaikan (tidak menghasilkan token).
     */
    void emitToken(std::vector<Token> &tokens, State state, const std::string &lexeme, int tokenLine, int tokenCol);
};