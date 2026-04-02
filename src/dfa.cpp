#include "dfa.hpp"
#include <cctype>

State DFA::getNextState(State current, char c)
{
    switch (current)
    {

    case STATE_START:
        if (std::isspace(c))
            return STATE_START; // whitespace : skip
        if (std::isalpha(c))
            return STATE_IDENT; // huruf : mulai identifier
        if (std::isdigit(c))
            return STATE_INT; // angka : mulai integer
        switch (c)
        {
        case '\'':
            return STATE_STRING;
        case '{':
            return STATE_COMMENT_CURLY;
        case '(':
            return STATE_LEFT_PAREN;
        case ')':
            return STATE_RIGHT_PAREN;
        case '+':
            return STATE_PLUS;
        case '-':
            return STATE_MINUS;
        case '*':
            return STATE_MULTIPLICATION;
        case '/':
            return STATE_RDIV;
        case ',':
            return STATE_COMMA;
        case ';':
            return STATE_SEMICOLON;
        case '.':
            return STATE_PERIOD;
        case ':':
            return STATE_COLON;
        case '<':
            return STATE_LESS;
        case '>':
            return STATE_GREATER;
        case '=':
            return STATE_EQUAL_TRANSITION;
        case '[':
            return STATE_OPENBRACK;
        case ']':
            return STATE_CLOSEBRACK;
        default:
            return STATE_DEAD; // karakter tidak dikenal
        }

    case STATE_IDENT:
        if (std::isalnum(c))
            return STATE_IDENT;
        return STATE_FINAL;

    case STATE_INT:
        if (std::isdigit(c))
            return STATE_INT;
        if (c == '.')
            return STATE_REAL_DOT;
        return STATE_FINAL;

    case STATE_REAL_DOT:
        if (std::isdigit(c))
            return STATE_REAL;
        return STATE_FINAL;

    case STATE_REAL:
        if (std::isdigit(c))
            return STATE_REAL;
        return STATE_FINAL;

    case STATE_STRING:
        if (c == '\'')
            return STATE_END_STRING;
        if (c != '\n' && c != '\0')
            return STATE_STRING;
        return STATE_DEAD;
    case STATE_STRING_QUOTE:
        if (c == '\'')
            return STATE_END_STRING;
        if (c != '\n' && c != '\0')
            return STATE_STRING;
        return STATE_DEAD;

    case STATE_END_STRING:
        if (c == '\'')
        {
            return STATE_STRING_QUOTE;
        }
        return STATE_FINAL;

    case STATE_COMMENT_CURLY:
        if (c == '}')
            return STATE_COMMENT_FINAL;
        if (c != '\0')
            return STATE_COMMENT_CURLY;
        return STATE_DEAD;

    case STATE_LEFT_PAREN:
        if (c == '*')
            return STATE_COMMENT_STAR;
        return STATE_FINAL;

    case STATE_COMMENT_STAR:
        if (c == '*')
            return STATE_COMMENT_STAR_END;
        if (c != '\0')
            return STATE_COMMENT_STAR;
        return STATE_DEAD;

    case STATE_COMMENT_STAR_END:
        if (c == ')')
            return STATE_COMMENT_FINAL;
        if (c == '*')
            return STATE_COMMENT_STAR_END;
        if (c != '\0')
            return STATE_COMMENT_STAR;
        return STATE_DEAD;

    case STATE_COMMENT_FINAL:
        return STATE_FINAL;

    case STATE_COLON:
        if (c == '=')
            return STATE_ASSIGNMENT;
        return STATE_FINAL;

    case STATE_ASSIGNMENT:
        return STATE_FINAL;

    case STATE_LESS:
        if (c == '=')
            return STATE_LESS_EQUAL;
        if (c == '>')
            return STATE_NOT_EQUAL;
        return STATE_FINAL;

    case STATE_LESS_EQUAL:
    case STATE_NOT_EQUAL:
        return STATE_FINAL;

    case STATE_GREATER:
        if (c == '=')
            return STATE_GREATER_EQUAL;
        return STATE_FINAL;

    case STATE_GREATER_EQUAL:
        return STATE_FINAL;

    case STATE_EQUAL_TRANSITION:
        if (c == '=')
            return STATE_EQUAL;
        return STATE_FINAL;

    case STATE_EQUAL:
        return STATE_FINAL;

    case STATE_RIGHT_PAREN:
    case STATE_PLUS:
    case STATE_MINUS:
    case STATE_MULTIPLICATION:
    case STATE_RDIV:
    case STATE_COMMA:
    case STATE_SEMICOLON:
    case STATE_PERIOD:
    case STATE_OPENBRACK:
    case STATE_CLOSEBRACK:
        return STATE_FINAL;

    case STATE_FINAL:
        return STATE_FINAL;
    case STATE_DEAD:
    default:
        return STATE_DEAD;
    }
}

bool DFA::isAccepting(State state)
{
    switch (state)
    {
    case STATE_IDENT:
    case STATE_INT:
    case STATE_REAL:
    case STATE_END_STRING:
    case STATE_COMMENT_FINAL:
    case STATE_COLON:
    case STATE_ASSIGNMENT:
    case STATE_LESS:
    case STATE_LESS_EQUAL:
    case STATE_NOT_EQUAL:
    case STATE_GREATER:
    case STATE_GREATER_EQUAL:
    case STATE_EQUAL:
    case STATE_LEFT_PAREN:
    case STATE_RIGHT_PAREN:
    case STATE_PLUS:
    case STATE_MINUS:
    case STATE_MULTIPLICATION:
    case STATE_RDIV:
    case STATE_COMMA:
    case STATE_SEMICOLON:
    case STATE_PERIOD:
    case STATE_OPENBRACK:
    case STATE_CLOSEBRACK:
        return true;
    default:
        return false;
    }
}

TokenType DFA::stateToTokenType(State state)
{
    switch (state)
    {
    case STATE_IDENT:
        return TOKEN_IDENT;
    case STATE_INT:
        return TOKEN_INTCON;
    case STATE_REAL:
        return TOKEN_REALCON;
    case STATE_END_STRING:
        return TOKEN_STRING;
    case STATE_COMMENT_FINAL:
        return TOKEN_COMMENT;
    case STATE_COLON:
        return TOKEN_COLON;
    case STATE_ASSIGNMENT:
        return TOKEN_BECOMES;
    case STATE_LESS:
        return TOKEN_LSS;
    case STATE_LESS_EQUAL:
        return TOKEN_LEQ;
    case STATE_NOT_EQUAL:
        return TOKEN_NEQ;
    case STATE_GREATER:
        return TOKEN_GTR;
    case STATE_GREATER_EQUAL:
        return TOKEN_GEQ;
    case STATE_EQUAL:
        return TOKEN_EQL;
    case STATE_LEFT_PAREN:
        return TOKEN_LPARENT;
    case STATE_RIGHT_PAREN:
        return TOKEN_RPARENT;
    case STATE_PLUS:
        return TOKEN_PLUS;
    case STATE_MINUS:
        return TOKEN_MINUS;
    case STATE_MULTIPLICATION:
        return TOKEN_TIMES;
    case STATE_RDIV:
        return TOKEN_RDIV;
    case STATE_COMMA:
        return TOKEN_COMMA;
    case STATE_SEMICOLON:
        return TOKEN_SEMICOLON;
    case STATE_PERIOD:
        return TOKEN_PERIOD;
    case STATE_OPENBRACK:
        return TOKEN_LBRACK;
    case STATE_CLOSEBRACK:
        return TOKEN_RBRACK;
    default:
        return TOKEN_ERROR;
    }
}
