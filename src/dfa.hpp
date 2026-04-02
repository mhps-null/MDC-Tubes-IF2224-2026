// dfa.hpp
// Definisi semua state DFA sama deklarasi fungsi-fungsinya.
// State-state ini ngikutin diagram transisi DFA buat lexer Arion.

#ifndef DFA_HPP
#define DFA_HPP

#include "token.hpp"

// Semua state yang ada di DFA
enum State {
    // mulai dari sini
    STATE_START,

    // identifier: huruf dulu, boleh diikuti huruf/angka
    STATE_IDENT,

    // angka: bisa integer doang atau real (ada titiknya)
    STATE_INT,          // [0-9]+
    STATE_REAL_DOT,     // udah ada titiknya tapi belum ada digit berikutnya (belum accepting)
    STATE_REAL,         // [0-9]+'.'[0-9]+

    // string dan charcon: diapit tanda petik tunggal
    STATE_STRING,       // lagi di dalam string (setelah buka petik)
    STATE_END_STRING,   // udah ketemu tutup petik

    // komentar kurung kurawal { ... }
    STATE_COMMENT_CURLY,

    // komentar bintang (* ... *)
    STATE_COMMENT_STAR,     // lagi baca isi komentar
    STATE_COMMENT_STAR_END, // ketemu '*', nunggu ')' buat nutup

    // komentar udah selesai (accepting)
    STATE_COMMENT_FINAL,

    // colon dan assignment
    STATE_COLON,        // ':' biasa (accepting)
    STATE_ASSIGNMENT,   // ':=' (accepting)

    // operator relasional yang lebih dari satu karakter
    STATE_LESS,         // '<' (accepting sebagai lss)
    STATE_LESS_EQUAL,   // '<=' (accepting sebagai leq)
    STATE_NOT_EQUAL,    // '<>' (accepting sebagai neq)
    STATE_GREATER,      // '>' (accepting sebagai gtr)
    STATE_GREATER_EQUAL,// '>=' (accepting sebagai geq)
    STATE_EQUAL_TRANSITION,  // '=' pertama (belum accepting, nunggu '=' kedua)
    STATE_EQUAL,        // '==' (accepting sebagai eql)

    // token satu karakter (semua accepting)
    STATE_LEFT_PAREN,   // '(' — tapi bisa lanjut jadi komentar kalau diikuti '*'
    STATE_RIGHT_PAREN,  // ')'
    STATE_PLUS,         // '+'
    STATE_MINUS,        // '-'
    STATE_MULTIPLICATION,        // '*'
    STATE_RDIV,         // '/'
    STATE_COMMA,        // ','
    STATE_SEMICOLON,    // ';'
    STATE_PERIOD,       // '.'
    STATE_OPENBRACK,       // '['
    STATE_CLOSEBRACK,       // ']'

    STATE_DEAD,
    STATE_FINAL,
    STATE_STRING_QUOTE
};

// fungsi-fungsi DFA
namespace DFA {
    // kasih state berikutnya berdasarkan state sekarang dan karakter yang dibaca
    // kalau ga ada transisi valid, return STATE_DEAD
    State getNextState(State current, char c);

    // cek apakah state ini accepting (artinya token bisa di-emit di sini)
    bool isAccepting(State state);

    // konversi state ke tipe token yang sesuai
    // catatan: STATE_IDENT masih perlu dicek keyword-nya di luar sini
    TokenType stateToTokenType(State state);
}

#endif // DFA_HPP
