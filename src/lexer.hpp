// lexer.hpp
// Deklarasi class Lexer buat bahasa Arion.
// Lexer pakai DFA buat baca source code dan hasilkan daftar token.

#ifndef LEXER_HPP
#define LEXER_HPP

#include "token.hpp"
#include "dfa.hpp"
#include <string>
#include <vector>

class Lexer {
public:
    // konstruktor: dikasih source code-nya langsung
    explicit Lexer(const std::string& source);

    // jalankan lexical analysis, return semua token yang berhasil dikenali
    std::vector<Token> scan();

private:
    std::string source;  // source code yang mau di-scan
    size_t pos;          // posisi karakter saat ini
    int line;            // baris saat ini
    int col;             // kolom saat ini

    // proses lexeme yang udah terkumpul dan masukin ke list token
    // ngurusin keyword lookup, charcon vs string, sama skip komentar
    void emitToken(std::vector<Token>& tokens, State state,
                   const std::string& lexeme, int tokenLine, int tokenCol);
};

#endif // LEXER_HPP
