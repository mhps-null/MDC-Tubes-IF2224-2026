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
    /**
     * Menentukan state berikutnya berdasarkan state saat ini dan karakter input.
     * Mengikuti aturan transisi DFA, dan mengembalikan STATE_DEAD jika tidak valid.
     */
    State getNextState(State current, char c);

    /**
     * Mengecek apakah suatu state adalah accepting state
     * (artinya lexeme bisa dijadikan token di state ini).
     */
    bool isAccepting(State state);

    /**
     * Mengonversi state DFA menjadi TokenType yang sesuai.
     */
    TokenType stateToTokenType(State state);
}
