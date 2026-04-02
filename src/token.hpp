#pragma once

#include <string>

enum TokenType
{
    // literal
    TOKEN_INTCON,  // 1.  konstanta integer
    TOKEN_REALCON, // 2.  konstanta bilangan real
    TOKEN_CHARCON, // 3.  konstanta karakter tunggal ('x')
    TOKEN_STRING,  // 4.  string ('abc')

    // operator logika
    TOKEN_NOTSY, // 5.  not
    TOKEN_ANDSY, // 12. and
    TOKEN_ORSY,  // 13. or

    // operator aritmatika
    TOKEN_PLUS,  // 6.  +
    TOKEN_MINUS, // 7.  -
    TOKEN_TIMES, // 8.  *
    TOKEN_IDIV,  // 9.  div
    TOKEN_RDIV,  // 10. /
    TOKEN_IMOD,  // 11. mod

    // operator relasional
    TOKEN_EQL, // 14. ==
    TOKEN_NEQ, // 15. <>
    TOKEN_GTR, // 16. >
    TOKEN_GEQ, // 17. >=
    TOKEN_LSS, // 18. <
    TOKEN_LEQ, // 19. <=

    // delimiter
    TOKEN_LPARENT,   // 20. (
    TOKEN_RPARENT,   // 21. )
    TOKEN_LBRACK,    // 22. [
    TOKEN_RBRACK,    // 23. ]
    TOKEN_COMMA,     // 24. ,
    TOKEN_SEMICOLON, // 25. ;
    TOKEN_PERIOD,    // 26. .
    TOKEN_COLON,     // 27. :
    TOKEN_BECOMES,   // 28. :=

    // keyword deklarasi
    TOKEN_CONSTSY,     // 29. const
    TOKEN_TYPESY,      // 30. type
    TOKEN_VARSY,       // 31. var
    TOKEN_FUNCTIONSY,  // 32. function
    TOKEN_PROCEDURESY, // 33. procedure
    TOKEN_ARRAYSY,     // 34. array
    TOKEN_RECORDSY,    // 35. record
    TOKEN_PROGRAMSY,   // 36. program

    // identifier
    TOKEN_IDENT, // 37. identifier

    // keyword kontrol
    TOKEN_BEGINSY,  // 38. begin
    TOKEN_IFSY,     // 39. if
    TOKEN_CASESY,   // 40. case
    TOKEN_REPEATSY, // 41. repeat
    TOKEN_WHILESY,  // 42. while
    TOKEN_FORSY,    // 43. for
    TOKEN_ENDSY,    // 44. end
    TOKEN_ELSESY,   // 45. else
    TOKEN_UNTILSY,  // 46. until
    TOKEN_OFSY,     // 47. of
    TOKEN_DOSY,     // 48. do
    TOKEN_TOSY,     // 49. to
    TOKEN_DOWNTOSY, // 50. downto
    TOKEN_THENSY,   // 51. then

    // komentar
    TOKEN_COMMENT, // 52. { ... } atau (* ... *)

    // special
    TOKEN_ERROR, // token ga dikenali
    TOKEN_EOF    // end of file
};

struct Token
{
    TokenType type;
    std::string value;
    int line;
    int col;
};

inline std::string tokenTypeToString(TokenType type)
{
    switch (type)
    {
    case TOKEN_INTCON:
        return "intcon";
    case TOKEN_REALCON:
        return "realcon";
    case TOKEN_CHARCON:
        return "charcon";
    case TOKEN_STRING:
        return "string";
    case TOKEN_NOTSY:
        return "notsy";
    case TOKEN_PLUS:
        return "plus";
    case TOKEN_MINUS:
        return "minus";
    case TOKEN_TIMES:
        return "times";
    case TOKEN_IDIV:
        return "idiv";
    case TOKEN_RDIV:
        return "rdiv";
    case TOKEN_IMOD:
        return "imod";
    case TOKEN_ANDSY:
        return "andsy";
    case TOKEN_ORSY:
        return "orsy";
    case TOKEN_EQL:
        return "eql";
    case TOKEN_NEQ:
        return "neq";
    case TOKEN_GTR:
        return "gtr";
    case TOKEN_GEQ:
        return "geq";
    case TOKEN_LSS:
        return "lss";
    case TOKEN_LEQ:
        return "leq";
    case TOKEN_LPARENT:
        return "lparent";
    case TOKEN_RPARENT:
        return "rparent";
    case TOKEN_LBRACK:
        return "lbrack";
    case TOKEN_RBRACK:
        return "rbrack";
    case TOKEN_COMMA:
        return "comma";
    case TOKEN_SEMICOLON:
        return "semicolon";
    case TOKEN_PERIOD:
        return "period";
    case TOKEN_COLON:
        return "colon";
    case TOKEN_BECOMES:
        return "becomes";
    case TOKEN_CONSTSY:
        return "constsy";
    case TOKEN_TYPESY:
        return "typesy";
    case TOKEN_VARSY:
        return "varsy";
    case TOKEN_FUNCTIONSY:
        return "functionsy";
    case TOKEN_PROCEDURESY:
        return "proceduresy";
    case TOKEN_ARRAYSY:
        return "arraysy";
    case TOKEN_RECORDSY:
        return "recordsy";
    case TOKEN_PROGRAMSY:
        return "programsy";
    case TOKEN_IDENT:
        return "ident";
    case TOKEN_BEGINSY:
        return "beginsy";
    case TOKEN_IFSY:
        return "ifsy";
    case TOKEN_CASESY:
        return "casesy";
    case TOKEN_REPEATSY:
        return "repeatsy";
    case TOKEN_WHILESY:
        return "whilesy";
    case TOKEN_FORSY:
        return "forsy";
    case TOKEN_ENDSY:
        return "endsy";
    case TOKEN_ELSESY:
        return "elsesy";
    case TOKEN_UNTILSY:
        return "untilsy";
    case TOKEN_OFSY:
        return "ofsy";
    case TOKEN_DOSY:
        return "dosy";
    case TOKEN_TOSY:
        return "tosy";
    case TOKEN_DOWNTOSY:
        return "downtosy";
    case TOKEN_THENSY:
        return "thensy";
    case TOKEN_COMMENT:
        return "comment";
    case TOKEN_ERROR:
        return "ERROR";
    case TOKEN_EOF:
        return "EOF";
    default:
        return "UNKNOWN";
    }
}

inline bool tokenHasValue(TokenType type)
{
    return type == TOKEN_INTCON || type == TOKEN_REALCON ||
           type == TOKEN_CHARCON || type == TOKEN_STRING || type == TOKEN_COMMENT ||
           type == TOKEN_IDENT || type == TOKEN_ERROR;
}

inline std::string tokenToString(const Token &tok)
{
    std::string result = tokenTypeToString(tok.type);
    if (tokenHasValue(tok.type))
    {
        result += " (" + tok.value + ")";
    }
    return result;
}