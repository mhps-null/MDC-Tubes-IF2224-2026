#include "parser.hpp"
#include <iostream>

static std::vector<Token> g_tokens;
static size_t g_pos;
static bool g_errorFlag;

static std::shared_ptr<ParseNode> parseProgram();
static std::shared_ptr<ParseNode> parseProgramHeader();
static std::shared_ptr<ParseNode> parseDeclarationPart();
static std::shared_ptr<ParseNode> parseConstDeclaration();
static std::shared_ptr<ParseNode> parseConstant();
static std::shared_ptr<ParseNode> parseTypeDeclaration();
static std::shared_ptr<ParseNode> parseType();
static std::shared_ptr<ParseNode> parseArrayType();
static std::shared_ptr<ParseNode> parseRange(std::shared_ptr<ParseNode> firstExpr);
static std::shared_ptr<ParseNode> parseEnumerated();
static std::shared_ptr<ParseNode> parseRecordType();
static std::shared_ptr<ParseNode> parseFieldList();
static std::shared_ptr<ParseNode> parseFieldPart();
static std::shared_ptr<ParseNode> parseVarDeclaration();
static std::shared_ptr<ParseNode> parseIdentifierList();
static std::shared_ptr<ParseNode> parseSubprogramDeclaration();
static std::shared_ptr<ParseNode> parseProcedureDeclaration();
static std::shared_ptr<ParseNode> parseFunctionDeclaration();
static std::shared_ptr<ParseNode> parseBlock();
static std::shared_ptr<ParseNode> parseFormalParameterList();
static std::shared_ptr<ParseNode> parseParameterGroup();
static std::shared_ptr<ParseNode> parseCompoundStatement();
static std::shared_ptr<ParseNode> parseStatementList();
static std::shared_ptr<ParseNode> parseStatement();
static std::shared_ptr<ParseNode> parseVariable(std::shared_ptr<ParseNode> identNode);
static std::shared_ptr<ParseNode> parseComponentVariable();
static std::shared_ptr<ParseNode> parseIndexList();
static std::shared_ptr<ParseNode> parseAssignmentStatement(std::shared_ptr<ParseNode> variableNode);
static std::shared_ptr<ParseNode> parseProcFuncCall(std::shared_ptr<ParseNode> identNode);
static std::shared_ptr<ParseNode> parseIfStatement();
static std::shared_ptr<ParseNode> parseCaseStatement();
static std::shared_ptr<ParseNode> parseCaseBlock();
static std::shared_ptr<ParseNode> parseWhileStatement();
static std::shared_ptr<ParseNode> parseRepeatStatement();
static std::shared_ptr<ParseNode> parseForStatement();
static std::shared_ptr<ParseNode> parseParameterList();
static std::shared_ptr<ParseNode> parseExpression();
static std::shared_ptr<ParseNode> parseSimpleExpression();
static std::shared_ptr<ParseNode> parseTerm();
static std::shared_ptr<ParseNode> parseFactor();

// ambil token saat ini
static const Token &current()
{
    if (g_pos < g_tokens.size())
        return g_tokens[g_pos];
    static Token eof{TOKEN_EOF, "", 0, 0};
    return eof;
}

static const Token &peekAt(int offset = 1)
{
    size_t idx = g_pos + offset;
    if (idx < g_tokens.size())
        return g_tokens[idx];
    static Token eof{TOKEN_EOF, "", 0, 0};
    return eof;
}

static bool check(TokenType t)
{
    return current().type == t;
}

// lewati token komentar
static void skipComments()
{
    while (g_pos < g_tokens.size() && g_tokens[g_pos].type == TOKEN_COMMENT)
        g_pos++;
}

// ubah token jadi node terminal
static std::shared_ptr<ParseNode> makeTerminal(const Token &tok)
{
    std::string label = tokenTypeToString(tok.type);
    if (tokenHasValue(tok.type) && tok.type != TOKEN_COMMENT)
        label += "(" + tok.value + ")";
    return std::make_shared<ParseNode>(label);
}

// konsumsi satu token lalu maju
static std::shared_ptr<ParseNode> consume()
{
    skipComments();
    auto node = makeTerminal(current());
    g_pos++;
    skipComments();
    return node;
}

// catat error sintaks
static void syntaxError(const std::string &expected)
{
    g_errorFlag = true;
    const Token &tok = current();
    std::cerr << "Syntax error (line " << tok.line << ", col " << tok.col
              << "): unexpected token '" << tokenToString(tok)
              << "', expected " << expected << "\n";
}

// pastikan token sesuai harapan
static std::shared_ptr<ParseNode> expect(TokenType t)
{
    skipComments();
    if (!check(t))
    {
        syntaxError(tokenTypeToString(t));
        return std::make_shared<ParseNode>("ERROR(expected:" + tokenTypeToString(t) + ")");
    }
    return consume();
}

// reset state parser
void initParser(const std::vector<Token> &tokens)
{
    g_tokens = tokens;
    g_pos = 0;
    g_errorFlag = false;
    skipComments();
}

// entry point parsing
std::shared_ptr<ParseNode> parse()
{
    return parseProgram();
}

// cek status error parser
bool hadError()
{
    return g_errorFlag;
}

static std::shared_ptr<ParseNode> parseProgram()
{
    auto node = std::make_shared<ParseNode>("<program>");
    node->children.push_back(parseProgramHeader());
    node->children.push_back(parseDeclarationPart());
    node->children.push_back(parseCompoundStatement());
    node->children.push_back(expect(TOKEN_PERIOD));
    return node;
}

static std::shared_ptr<ParseNode> parseProgramHeader()
{
    auto node = std::make_shared<ParseNode>("<program-header>");
    node->children.push_back(expect(TOKEN_PROGRAMSY));
    node->children.push_back(expect(TOKEN_IDENT));
    node->children.push_back(expect(TOKEN_SEMICOLON));
    return node;
}

static std::shared_ptr<ParseNode> parseDeclarationPart()
{
    auto node = std::make_shared<ParseNode>("<declaration-part>");

    while (check(TOKEN_CONSTSY))
        node->children.push_back(parseConstDeclaration());

    while (check(TOKEN_TYPESY))
        node->children.push_back(parseTypeDeclaration());

    while (check(TOKEN_VARSY))
        node->children.push_back(parseVarDeclaration());

    while (check(TOKEN_PROCEDURESY) || check(TOKEN_FUNCTIONSY))
        node->children.push_back(parseSubprogramDeclaration());

    return node;
}

static std::shared_ptr<ParseNode> parseConstDeclaration()
{
    auto node = std::make_shared<ParseNode>("<const-declaration>");
    node->children.push_back(expect(TOKEN_CONSTSY));
    do
    {
        node->children.push_back(expect(TOKEN_IDENT));
        node->children.push_back(expect(TOKEN_EQL));
        node->children.push_back(parseConstant());
        node->children.push_back(expect(TOKEN_SEMICOLON));
    } while (check(TOKEN_IDENT));
    return node;
}

static std::shared_ptr<ParseNode> parseConstant()
{
    auto node = std::make_shared<ParseNode>("<constant>");
    if (check(TOKEN_CHARCON) || check(TOKEN_STRING))
    {
        node->children.push_back(consume());
    }
    else
    {
        if (check(TOKEN_PLUS) || check(TOKEN_MINUS))
            node->children.push_back(consume());
        if (check(TOKEN_IDENT) || check(TOKEN_INTCON) || check(TOKEN_REALCON))
            node->children.push_back(consume());
        else
            syntaxError("constant value (ident|intcon|realcon|charcon|string)");
    }
    return node;
}

static std::shared_ptr<ParseNode> parseTypeDeclaration()
{
    auto node = std::make_shared<ParseNode>("<type-declaration>");
    node->children.push_back(expect(TOKEN_TYPESY));
    do
    {
        node->children.push_back(expect(TOKEN_IDENT));
        node->children.push_back(expect(TOKEN_EQL));
        node->children.push_back(parseType());
        node->children.push_back(expect(TOKEN_SEMICOLON));
    } while (check(TOKEN_IDENT));
    return node;
}

static std::shared_ptr<ParseNode> parseType()
{
    auto node = std::make_shared<ParseNode>("<type>");

    if (check(TOKEN_ARRAYSY))
    {
        node->children.push_back(parseArrayType());
    }
    else if (check(TOKEN_LPARENT))
    {
        node->children.push_back(parseEnumerated());
    }
    else if (check(TOKEN_RECORDSY))
    {
        node->children.push_back(parseRecordType());
    }
    else if (check(TOKEN_IDENT))
    {
        // ident bisa jadi awal range: a..b
        if (peekAt(1).type == TOKEN_PERIOD)
        {
            auto exprNode = parseExpression();
            node->children.push_back(parseRange(exprNode));
        }
        else
        {
            node->children.push_back(consume());
        }
    }
    else if (check(TOKEN_INTCON) || check(TOKEN_REALCON) || check(TOKEN_CHARCON) ||
             check(TOKEN_PLUS) || check(TOKEN_MINUS))
    {
        auto exprNode = parseExpression();
        node->children.push_back(parseRange(exprNode));
    }
    else
    {
        syntaxError("type (ident|array|enumerated|record|range)");
        node->children.push_back(std::make_shared<ParseNode>("ERROR"));
    }
    return node;
}

static std::shared_ptr<ParseNode> parseArrayType()
{
    auto node = std::make_shared<ParseNode>("<array-type>");
    node->children.push_back(expect(TOKEN_ARRAYSY));
    node->children.push_back(expect(TOKEN_LBRACK));

    if (check(TOKEN_IDENT) && peekAt(1).type == TOKEN_PERIOD)
    {
        auto exprNode = parseExpression();
        node->children.push_back(parseRange(exprNode));
    }
    else if (check(TOKEN_INTCON) || check(TOKEN_CHARCON) ||
             check(TOKEN_PLUS) || check(TOKEN_MINUS))
    {
        auto exprNode = parseExpression();
        node->children.push_back(parseRange(exprNode));
    }
    else if (check(TOKEN_IDENT))
    {
        node->children.push_back(consume());
    }
    else
    {
        syntaxError("range or ident for array index");
    }

    node->children.push_back(expect(TOKEN_RBRACK));
    node->children.push_back(expect(TOKEN_OFSY));
    node->children.push_back(parseType());
    return node;
}

static std::shared_ptr<ParseNode> parseRange(std::shared_ptr<ParseNode> firstExpr)
{
    auto node = std::make_shared<ParseNode>("<range>");
    node->children.push_back(firstExpr);
    node->children.push_back(expect(TOKEN_PERIOD));
    node->children.push_back(expect(TOKEN_PERIOD));
    node->children.push_back(parseExpression());
    return node;
}

static std::shared_ptr<ParseNode> parseEnumerated()
{
    auto node = std::make_shared<ParseNode>("<enumerated>");
    node->children.push_back(expect(TOKEN_LPARENT));
    node->children.push_back(expect(TOKEN_IDENT));
    while (check(TOKEN_COMMA))
    {
        node->children.push_back(consume());
        node->children.push_back(expect(TOKEN_IDENT));
    }
    node->children.push_back(expect(TOKEN_RPARENT));
    return node;
}

static std::shared_ptr<ParseNode> parseRecordType()
{
    auto node = std::make_shared<ParseNode>("<record-type>");
    node->children.push_back(expect(TOKEN_RECORDSY));
    node->children.push_back(parseFieldList());
    node->children.push_back(expect(TOKEN_ENDSY));
    return node;
}

static std::shared_ptr<ParseNode> parseFieldList()
{
    auto node = std::make_shared<ParseNode>("<field-list>");

    node->children.push_back(parseFieldPart());

    while (check(TOKEN_SEMICOLON))
    {

        auto semi = consume();

        if (check(TOKEN_ENDSY))
        {
            node->children.push_back(semi);
            break;
        }

        node->children.push_back(semi);
        node->children.push_back(parseFieldPart());
    }

    return node;
}

static std::shared_ptr<ParseNode> parseFieldPart()
{
    auto node = std::make_shared<ParseNode>("<field-part>");
    node->children.push_back(parseIdentifierList());
    node->children.push_back(expect(TOKEN_COLON));
    node->children.push_back(parseType());
    return node;
}

static std::shared_ptr<ParseNode> parseVarDeclaration()
{
    auto node = std::make_shared<ParseNode>("<var-declaration>");
    node->children.push_back(expect(TOKEN_VARSY));
    do
    {
        node->children.push_back(parseIdentifierList());
        node->children.push_back(expect(TOKEN_COLON));
        node->children.push_back(parseType());
        node->children.push_back(expect(TOKEN_SEMICOLON));
    } while (check(TOKEN_IDENT));
    return node;
}

static std::shared_ptr<ParseNode> parseIdentifierList()
{
    auto node = std::make_shared<ParseNode>("<identifier-list>");
    node->children.push_back(expect(TOKEN_IDENT));
    while (check(TOKEN_COMMA) && peekAt(1).type == TOKEN_IDENT)
    {
        node->children.push_back(consume());
        node->children.push_back(expect(TOKEN_IDENT));
    }
    return node;
}

static std::shared_ptr<ParseNode> parseSubprogramDeclaration()
{
    auto node = std::make_shared<ParseNode>("<subprogram-declaration>");
    if (check(TOKEN_PROCEDURESY))
        node->children.push_back(parseProcedureDeclaration());
    else
        node->children.push_back(parseFunctionDeclaration());
    return node;
}

static std::shared_ptr<ParseNode> parseProcedureDeclaration()
{
    auto node = std::make_shared<ParseNode>("<procedure-declaration>");
    node->children.push_back(expect(TOKEN_PROCEDURESY));
    node->children.push_back(expect(TOKEN_IDENT));
    if (check(TOKEN_LPARENT))
        node->children.push_back(parseFormalParameterList());
    node->children.push_back(expect(TOKEN_SEMICOLON));
    node->children.push_back(parseBlock());
    node->children.push_back(expect(TOKEN_SEMICOLON));
    return node;
}

static std::shared_ptr<ParseNode> parseFunctionDeclaration()
{
    auto node = std::make_shared<ParseNode>("<function-declaration>");
    node->children.push_back(expect(TOKEN_FUNCTIONSY));
    node->children.push_back(expect(TOKEN_IDENT));
    if (check(TOKEN_LPARENT))
        node->children.push_back(parseFormalParameterList());
    node->children.push_back(expect(TOKEN_COLON));
    node->children.push_back(expect(TOKEN_IDENT));
    node->children.push_back(expect(TOKEN_SEMICOLON));
    node->children.push_back(parseBlock());
    node->children.push_back(expect(TOKEN_SEMICOLON));
    return node;
}

static std::shared_ptr<ParseNode> parseBlock()
{
    auto node = std::make_shared<ParseNode>("<block>");
    node->children.push_back(parseDeclarationPart());
    node->children.push_back(parseCompoundStatement());
    return node;
}

static std::shared_ptr<ParseNode> parseFormalParameterList()
{
    auto node = std::make_shared<ParseNode>("<formal-parameter-list>");
    node->children.push_back(expect(TOKEN_LPARENT));
    node->children.push_back(parseParameterGroup());
    while (check(TOKEN_SEMICOLON))
    {
        node->children.push_back(consume());
        node->children.push_back(parseParameterGroup());
    }
    node->children.push_back(expect(TOKEN_RPARENT));
    return node;
}

static std::shared_ptr<ParseNode> parseParameterGroup()
{
    auto node = std::make_shared<ParseNode>("<parameter-group>");
    node->children.push_back(parseIdentifierList());
    node->children.push_back(expect(TOKEN_COLON));
    if (check(TOKEN_ARRAYSY))
        node->children.push_back(parseArrayType());
    else
        node->children.push_back(expect(TOKEN_IDENT));
    return node;
}

static std::shared_ptr<ParseNode> parseCompoundStatement()
{
    auto node = std::make_shared<ParseNode>("<compound-statement>");
    node->children.push_back(expect(TOKEN_BEGINSY));
    node->children.push_back(parseStatementList());
    node->children.push_back(expect(TOKEN_ENDSY));
    return node;
}

// token awal yang valid untuk statement
static bool isStatementStart(TokenType t)
{
    return t == TOKEN_IDENT || t == TOKEN_IFSY || t == TOKEN_CASESY ||
           t == TOKEN_WHILESY || t == TOKEN_REPEATSY || t == TOKEN_FORSY ||
           t == TOKEN_BEGINSY;
}

// trailing semicolon nempel ke statement terakhir
static std::shared_ptr<ParseNode> parseStatementList()
{
    auto node = std::make_shared<ParseNode>("<statement-list>");

    while (!isStatementStart(current().type) &&
           !check(TOKEN_ENDSY) &&
           !check(TOKEN_EOF))
    {
        syntaxError("statement");

        auto errNode = std::make_shared<ParseNode>(
            "ERROR(unexpected:" + tokenToString(current()) + ")");

        node->children.push_back(errNode);

        consume();

        if (check(TOKEN_SEMICOLON))
        {
            node->children.push_back(consume());
            break;
        }
    }

    node->children.push_back(parseStatement());
    while (check(TOKEN_SEMICOLON))
    {
        auto semNode = consume();
        if (isStatementStart(current().type))
        {
            node->children.push_back(semNode);
            node->children.push_back(parseStatement());
        }
        else
        {
            if (!node->children.empty())
                node->children.back()->children.push_back(semNode);
            break;
        }
    }
    return node;
}

// kalau tidak cocok apa-apa, anggap empty statement
static std::shared_ptr<ParseNode> parseStatement()
{
    if (check(TOKEN_UNKNOWN))
    {
        syntaxError("valid statement");

        auto err = std::make_shared<ParseNode>("ERROR(unexpected:" + tokenToString(current()) + ")");

        consume();

        return err;
    }
    else if (check(TOKEN_IDENT))
    {
        auto identNode = consume();
        auto variableNode = parseVariable(identNode);
        if (check(TOKEN_BECOMES))
        {
            return parseAssignmentStatement(variableNode);
        }
        return parseProcFuncCall(identNode);
    }
    else if (check(TOKEN_IFSY))
    {
        return parseIfStatement();
    }
    else if (check(TOKEN_CASESY))
    {
        return parseCaseStatement();
    }
    else if (check(TOKEN_WHILESY))
    {
        return parseWhileStatement();
    }
    else if (check(TOKEN_REPEATSY))
    {
        return parseRepeatStatement();
    }
    else if (check(TOKEN_FORSY))
    {
        return parseForStatement();
    }
    else if (check(TOKEN_BEGINSY))
    {
        return parseCompoundStatement();
    }
    return std::make_shared<ParseNode>("<statement>");
}

static std::shared_ptr<ParseNode> parseVariable(
    std::shared_ptr<ParseNode> identNode)
{
    auto node = std::make_shared<ParseNode>("<variable>");

    node->children.push_back(identNode);

    while (check(TOKEN_LBRACK) || check(TOKEN_PERIOD))
    {
        node->children.push_back(parseComponentVariable());
    }

    return node;
}

static std::shared_ptr<ParseNode> parseComponentVariable()
{
    auto node = std::make_shared<ParseNode>("<component-variable>");

    if (check(TOKEN_LBRACK))
    {
        node->children.push_back(consume());

        node->children.push_back(parseIndexList());

        node->children.push_back(expect(TOKEN_RBRACK));
    }
    else if (check(TOKEN_PERIOD))
    {
        node->children.push_back(consume());

        node->children.push_back(expect(TOKEN_IDENT));
    }

    return node;
}

static std::shared_ptr<ParseNode> parseIndexList()
{
    auto node = std::make_shared<ParseNode>("<index-list>");

    if (check(TOKEN_INTCON) ||
        check(TOKEN_CHARCON) ||
        check(TOKEN_IDENT))
    {
        node->children.push_back(consume());
    }
    else
    {
        syntaxError("index");
    }

    while (check(TOKEN_COMMA))
    {
        node->children.push_back(consume());

        if (check(TOKEN_INTCON) ||
            check(TOKEN_CHARCON) ||
            check(TOKEN_IDENT))
        {
            node->children.push_back(consume());
        }
        else
        {
            syntaxError("index");
        }
    }

    return node;
}

static std::shared_ptr<ParseNode> parseAssignmentStatement(std::shared_ptr<ParseNode> variableNode)
{
    auto node = std::make_shared<ParseNode>("<assignment-statement>");
    node->children.push_back(variableNode);
    node->children.push_back(expect(TOKEN_BECOMES));
    node->children.push_back(parseExpression());
    return node;
}

static std::shared_ptr<ParseNode> parseProcFuncCall(std::shared_ptr<ParseNode> identNode)
{
    auto node = std::make_shared<ParseNode>("<procedure/function-call>");
    node->children.push_back(identNode);
    if (check(TOKEN_LPARENT))
    {
        node->children.push_back(consume());
        node->children.push_back(parseParameterList());
        node->children.push_back(expect(TOKEN_RPARENT));
    }
    return node;
}

static std::shared_ptr<ParseNode> parseIfStatement()
{
    auto node = std::make_shared<ParseNode>("<if-statement>");
    node->children.push_back(expect(TOKEN_IFSY));
    node->children.push_back(parseExpression());
    node->children.push_back(expect(TOKEN_THENSY));
    node->children.push_back(parseStatement());
    if (check(TOKEN_ELSESY))
    {
        node->children.push_back(consume());
        node->children.push_back(parseStatement());
    }
    return node;
}

static std::shared_ptr<ParseNode> parseCaseStatement()
{
    auto node = std::make_shared<ParseNode>("<case-statement>");
    node->children.push_back(expect(TOKEN_CASESY));
    node->children.push_back(parseExpression());
    node->children.push_back(expect(TOKEN_OFSY));
    node->children.push_back(parseCaseBlock());
    node->children.push_back(expect(TOKEN_ENDSY));
    return node;
}

static std::shared_ptr<ParseNode> parseCaseBlock()
{
    auto node = std::make_shared<ParseNode>("<case-block>");
    node->children.push_back(parseConstant());
    while (check(TOKEN_COMMA))
    {
        node->children.push_back(consume());
        node->children.push_back(parseConstant());
    }
    node->children.push_back(expect(TOKEN_COLON));
    node->children.push_back(parseStatement());
    while (check(TOKEN_SEMICOLON))
    {
        node->children.push_back(consume());
        if (!check(TOKEN_ENDSY))
            node->children.push_back(parseCaseBlock());
    }
    return node;
}

static std::shared_ptr<ParseNode> parseWhileStatement()
{
    auto node = std::make_shared<ParseNode>("<while-statement>");
    node->children.push_back(expect(TOKEN_WHILESY));
    node->children.push_back(parseExpression());
    node->children.push_back(expect(TOKEN_DOSY));
    node->children.push_back(parseStatement());
    return node;
}

static std::shared_ptr<ParseNode> parseRepeatStatement()
{
    auto node = std::make_shared<ParseNode>("<repeat-statement>");
    node->children.push_back(expect(TOKEN_REPEATSY));
    node->children.push_back(parseStatementList());
    node->children.push_back(expect(TOKEN_UNTILSY));
    node->children.push_back(parseExpression());
    return node;
}

static std::shared_ptr<ParseNode> parseForStatement()
{
    auto node = std::make_shared<ParseNode>("<for-statement>");
    node->children.push_back(expect(TOKEN_FORSY));
    node->children.push_back(expect(TOKEN_IDENT));
    node->children.push_back(expect(TOKEN_BECOMES));
    node->children.push_back(parseExpression());
    if (check(TOKEN_TOSY))
        node->children.push_back(consume());
    else
        node->children.push_back(expect(TOKEN_DOWNTOSY));
    node->children.push_back(parseExpression());
    node->children.push_back(expect(TOKEN_DOSY));
    node->children.push_back(parseStatement());
    return node;
}

static std::shared_ptr<ParseNode> parseParameterList()
{
    auto node = std::make_shared<ParseNode>("<parameter-list>");
    node->children.push_back(parseExpression());
    while (check(TOKEN_COMMA))
    {
        node->children.push_back(consume());
        node->children.push_back(parseExpression());
    }
    return node;
}

static std::shared_ptr<ParseNode> parseExpression()
{
    auto node = std::make_shared<ParseNode>("<expression>");
    node->children.push_back(parseSimpleExpression());
    // relasi opsional: = <> > >= < <=
    if (check(TOKEN_EQL) || check(TOKEN_NEQ) || check(TOKEN_GTR) ||
        check(TOKEN_GEQ) || check(TOKEN_LSS) || check(TOKEN_LEQ))
    {
        node->children.push_back(consume());
        node->children.push_back(parseSimpleExpression());
    }
    return node;
}

static std::shared_ptr<ParseNode> parseSimpleExpression()
{
    auto node = std::make_shared<ParseNode>("<simple-expression>");
    if (check(TOKEN_PLUS) || check(TOKEN_MINUS))
        node->children.push_back(consume());
    node->children.push_back(parseTerm());
    while (check(TOKEN_PLUS) || check(TOKEN_MINUS) || check(TOKEN_ORSY))
    {
        node->children.push_back(consume());
        node->children.push_back(parseTerm());
    }
    return node;
}

static std::shared_ptr<ParseNode> parseTerm()
{
    auto node = std::make_shared<ParseNode>("<term>");
    node->children.push_back(parseFactor());
    while (check(TOKEN_TIMES) || check(TOKEN_RDIV) || check(TOKEN_IDIV) ||
           check(TOKEN_IMOD) || check(TOKEN_ANDSY))
    {
        node->children.push_back(consume());
        node->children.push_back(parseFactor());
    }
    return node;
}

static std::shared_ptr<ParseNode> parseFactor()
{
    auto node = std::make_shared<ParseNode>("<factor>");

    if (check(TOKEN_IDENT))
    {

        auto identNode = consume();

        if (check(TOKEN_LPARENT))
        {
            node->children.push_back(parseProcFuncCall(identNode));
        }
        else if (check(TOKEN_LBRACK) || check(TOKEN_PERIOD))
        {
            node->children.push_back(parseVariable(identNode));
        }
        else
        {
            node->children.push_back(identNode);
        }
    }
    else if (check(TOKEN_INTCON) || check(TOKEN_REALCON) ||
             check(TOKEN_CHARCON) || check(TOKEN_STRING))
    {
        node->children.push_back(consume());
    }
    else if (check(TOKEN_LPARENT))
    {
        node->children.push_back(consume());
        node->children.push_back(parseExpression());
        node->children.push_back(expect(TOKEN_RPARENT));
    }
    else if (check(TOKEN_NOTSY))
    {
        node->children.push_back(consume());
        node->children.push_back(parseFactor());
    }
    else
    {
        // token tidak valid untuk factor
        syntaxError("factor (ident|intcon|realcon|charcon|string|(expr)|not|func-call)");
        node->children.push_back(std::make_shared<ParseNode>("ERROR"));
    }
    return node;
}
