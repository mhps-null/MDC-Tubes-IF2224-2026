// lexer.cpp
// Implementasi Lexer buat bahasa Arion.
//
// Cara kerjanya:
// 1. Mulai di STATE_START
// 2. Baca satu karakter dari source code
// 3. Tanya DFA: state berikutnya apa?
// 4. Kalau transisi valid → lanjut, akumulasi lexeme
// 5. Kalau DEAD dan state sekarang accepting → emit token, reset ke START
// 6. Kalau DEAD dan non-accepting → backtrack ke accepting terakhir, atau error
// 7. Ulangi sampai EOF
//
// Backtracking dibutuhkan buat kasus kayak:
//   "123." → INT(123) + PERIOD, bukan error di REAL_DOT

#include "lexer.hpp"
#include "utils.hpp"
#include <iostream>

Lexer::Lexer(const std::string& source)
    : source(source), pos(0), line(1), col(1) {}

// Loop utama scan.
// Baca source code karakter per karakter, pakai DFA buat tokenisasi.
std::vector<Token> Lexer::scan() {
    std::vector<Token> tokens;
    State state = STATE_START;
    std::string lexeme;

    // buat backtracking: simpan posisi accepting state terakhir yang ketemu
    State lastAccept = STATE_DEAD;
    size_t lastAcceptPos = 0;
    int lastAcceptLine = 1;
    int lastAcceptCol = 1;
    std::string lastAcceptLexeme;

    // posisi awal token yang lagi dibaca
    int tokenStartLine = line;
    int tokenStartCol = col;

    while (pos <= source.size()) {
        // ambil karakter sekarang, atau '\0' kalau udah EOF
        char c = (pos < source.size()) ? source[pos] : '\0';

        // tanya DFA state berikutnya
        State next;
        if (c == '\0' && state == STATE_START) {
            break;  // EOF pas lagi di START → selesai
        } else if (c == '\0') {
            next = STATE_FINAL;  // EOF di tengah token → paksa emit/error
        } else {
            next = DFA::getNextState(state, c);
        }

        if (next == STATE_START) {
            // whitespace dari START → skip aja
            if (c == '\n') { line++; col = 1; }
            else           { col++; }
            pos++;

        } else if (next == STATE_DEAD) {
            // karakter ga dikenal dari START
            std::cerr << "Lexer error (line " << line << ", col " << col
                      << "): karakter tidak dikenali '" << c << "'" << std::endl;
            tokens.push_back({TOKEN_ERROR, std::string(1, c), line, col});
            pos++;
            if (c == '\n') { line++; col = 1; }
            else           { col++; }

        } else if (next != STATE_FINAL) {
            // transisi valid → akumulasi lexeme
            if (state == STATE_START) {
                tokenStartLine = line;
                tokenStartCol = col;
            }

            state = next;
            lexeme += c;
            if (c == '\n') { line++; col = 1; }
            else           { col++; }
            pos++;

            // kalau state baru accepting → simpan sebagai backtrack point
            if (DFA::isAccepting(state)) {
                lastAccept = state;
                lastAcceptPos = pos;
                lastAcceptLine = line;
                lastAcceptCol = col;
                lastAcceptLexeme = lexeme;
            }

            // kalau udah masuk komentar atau string, hapus backtrack point
            // biar ga bisa mundur ke '(' atau state lain di luar
            // kalau komentar/string ga ditutup → error, bukan backtrack
            if (state == STATE_COMMENT_STAR || state == STATE_COMMENT_CURLY) {
                lastAccept = STATE_FINAL;
            }

        } else {
            // STATE_DEAD: ga ada transisi valid

            if (DFA::isAccepting(state)) {
                // state sekarang accepting → emit token, karakter ini diproses ulang
                emitToken(tokens, state, lexeme, tokenStartLine, tokenStartCol);
                state = STATE_START;
                lexeme.clear();
                lastAccept = STATE_FINAL;
                // pos TIDAK di-advance → karakter ini diproses ulang dari START

            } else if (lastAccept != STATE_FINAL) {
                // backtrack ke accepting state terakhir yang pernah ketemu
                emitToken(tokens, lastAccept, lastAcceptLexeme,
                          tokenStartLine, tokenStartCol);
                pos = lastAcceptPos;
                line = lastAcceptLine;
                col = lastAcceptCol;
                state = STATE_START;
                lexeme.clear();
                lastAccept = STATE_FINAL;

            } else {
                // ga ada accepting state sama sekali → error
                std::string errLexeme = lexeme.empty() ? std::string(1, c) : lexeme;
                std::cerr << "Lexer error (line " << tokenStartLine
                          << ", col " << tokenStartCol
                          << "): token tidak valid '" << errLexeme << "'" << std::endl;
                tokens.push_back({TOKEN_ERROR, errLexeme, tokenStartLine, tokenStartCol});

                // skip satu karakter dan reset
                if (lexeme.empty()) {
                    pos++;
                    if (c == '\n') { line++; col = 1; }
                    else           { col++; }
                }
                state = STATE_START;
                lexeme.clear();
                lastAccept = STATE_FINAL;
            }
        }
    }

    // kalau masih ada sisa token pas EOF
    if (state != STATE_START) {
        if (DFA::isAccepting(state)) {
            emitToken(tokens, state, lexeme, tokenStartLine, tokenStartCol);
        } else if (lastAccept != STATE_FINAL) {
            emitToken(tokens, lastAccept, lastAcceptLexeme,
                      tokenStartLine, tokenStartCol);
        } else if (!lexeme.empty()) {
            std::cerr << "Lexer error (line " << tokenStartLine
                      << ", col " << tokenStartCol
                      << "): unexpected end of file, token '" << lexeme << "'" << std::endl;
            tokens.push_back({TOKEN_ERROR, lexeme, tokenStartLine, tokenStartCol});
        }
    }

    return tokens;
}

// Proses lexeme yang udah terkumpul dan emit token yang sesuai.
//
// Logika khusus:
// - IDENT: cek dulu ke keyword table, bisa jadi keyword kayak begin, end, dll
// - END_STRING: kalau isinya 1 karakter → charcon, lebih → string
// - COMMENT: di-skip, ga masuk output
void Lexer::emitToken(std::vector<Token>& tokens, State state,
                      const std::string& lexeme, int tokenLine, int tokenCol) {

    TokenType type = DFA::stateToTokenType(state);

    switch (state) {
        case STATE_IDENT: {
            // cek apakah ini keyword atau identifier biasa
            TokenType kwType = lookupKeyword(lexeme);
            if (kwType != TOKEN_IDENT) {
                // keyword → emit tanpa value
                tokens.push_back({kwType, "", tokenLine, tokenCol});
            } else {
                // identifier biasa → emit dengan nama aslinya
                tokens.push_back({TOKEN_IDENT, lexeme, tokenLine, tokenCol});
            }
            break;
        }

        case STATE_END_STRING: {
            // ambil isi di antara petik: "'content'" → content = lexeme[1..n-2]
            std::string content = "";
            if (lexeme.size() >= 2) {
                content = lexeme.substr(1, lexeme.size() - 2);
            }
           std::string evaluatedContent = "";
            for (size_t i = 0; i < content.size(); ++i) {
                if (content[i] == '\'' && i + 1 < content.size() && content[i+1] == '\'') {
                    evaluatedContent += '\'';
                    i++; // skip petik kedua
                } else {
                    evaluatedContent += content[i];
                }
            }

            // BUAT LEXEME BARU: gabungkan petik luar dengan isi yang sudah dievaluasi
            // Contoh: jika evaluatedContent adalah ' maka finalLexeme menjadi '''
            std::string finalLexeme = "'" + evaluatedContent + "'";

            // CEK menggunakan evaluatedContent.size() bukan content.size()
            // Catatan: Jika Anda WAJIB menjadikannya TOKEN_STRING (dan bukan CHARCON) 
            // meskipun panjangnya 1, Anda bisa mengubah TOKEN_CHARCON di bawah menjadi TOKEN_STRING.
            if (evaluatedContent.size() == 1) {
                // satu karakter → charcon (atau ubah ke TOKEN_STRING sesuai kebutuhan spesifikasi Anda)
                tokens.push_back({TOKEN_STRING, finalLexeme, tokenLine, tokenCol});
            } else {
                // lebih dari satu karakter → string
                tokens.push_back({TOKEN_STRING, finalLexeme, tokenLine, tokenCol});
            }
            break;
        }

        case STATE_INT:
        case STATE_REAL: {
            // angka → emit dengan valuenya
            tokens.push_back({type, lexeme, tokenLine, tokenCol});
            break;
        }

        case STATE_COMMENT_FINAL: {
            // komentar → skip aja, ga perlu masuk output
            break;
        }

        default: {
            // operator dan delimiter lainnya → emit tanpa value
            tokens.push_back({type, "", tokenLine, tokenCol});
            break;
        }
    }
}
