#pragma once

#include "token.hpp"

enum State
{
    STATE_START,

    STATE_IDENT,

    STATE_INT,      // [0-9]+
    STATE_REAL_DOT, // [0-9]+'.'
    STATE_REAL,     // [0-9]+'.'[0-9]+

    STATE_STRING,
    STATE_END_STRING,

    STATE_COMMENT_CURLY,

    STATE_COMMENT_STAR,
    STATE_COMMENT_STAR_END,

    STATE_COMMENT_FINAL,

    STATE_COLON,      // ':'
    STATE_ASSIGNMENT, // ':='

    STATE_LESS,             // '<'
    STATE_LESS_EQUAL,       // '<='
    STATE_NOT_EQUAL,        // '<>'
    STATE_GREATER,          // '>'
    STATE_GREATER_EQUAL,    // '>='
    STATE_EQUAL_TRANSITION, // '='
    STATE_EQUAL,            // '=='

    STATE_LEFT_PAREN,     // '('
    STATE_RIGHT_PAREN,    // ')'
    STATE_PLUS,           // '+'
    STATE_MINUS,          // '-'
    STATE_MULTIPLICATION, // '*'
    STATE_RDIV,           // '/'
    STATE_COMMA,          // ','
    STATE_SEMICOLON,      // ';'
    STATE_PERIOD,         // '.'
    STATE_OPENBRACK,      // '['
    STATE_CLOSEBRACK,     // ']'

    STATE_DEAD,
    STATE_FINAL,
    STATE_STRING_QUOTE
};

namespace DFA
{
    // kasih state berikutnya berdasarkan state sekarang dan karakter yang dibaca
    // kalau ga ada transisi valid, return STATE_DEAD
    State getNextState(State current, char c);

    // cek apakah state ini accepting (artinya token bisa di-emit di sini)
    bool isAccepting(State state);

    // konversi state ke tipe token yang sesuai
    TokenType stateToTokenType(State state);
}
