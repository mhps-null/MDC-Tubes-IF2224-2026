#include "lexer.hpp"
#include "utils.hpp"
#include <iostream>

Lexer::Lexer(const std::string &source)
    : source(source), pos(0), line(1), col(1) {}

std::vector<Token> Lexer::scan()
{
    std::vector<Token> tokens;
    State state = STATE_START;
    std::string lexeme;

    State lastAccept = STATE_DEAD;
    size_t lastAcceptPos = 0;
    int lastAcceptLine = 1;
    int lastAcceptCol = 1;
    std::string lastAcceptLexeme;

    int tokenStartLine = line;
    int tokenStartCol = col;

    while (pos <= source.size())
    {
        char c = (pos < source.size()) ? source[pos] : '\0';

        State next;
        if (c == '\0' && state == STATE_START)
        {
            break;
        }
        else if (c == '\0')
        {
            next = STATE_FINAL;
        }
        else
        {
            next = DFA::getNextState(state, c);
        }

        if (next == STATE_START)
        {
            // whitespace dari START skip
            if (c == '\n')
            {
                line++;
                col = 1;
            }
            else
            {
                col++;
            }
            pos++;
        }
        else if (next == STATE_DEAD)
        {
            std::cerr << "Lexer error (line " << line << ", col " << col
                      << "): karakter tidak dikenali '" << c << "'" << std::endl;
            tokens.push_back({TOKEN_ERROR, std::string(1, c), line, col});
            pos++;
            if (c == '\n')
            {
                line++;
                col = 1;
            }
            else
            {
                col++;
            }
        }
        else if (next != STATE_FINAL)
        {
            if (state == STATE_START)
            {
                tokenStartLine = line;
                tokenStartCol = col;
            }

            state = next;
            lexeme += c;
            if (c == '\n')
            {
                line++;
                col = 1;
            }
            else
            {
                col++;
            }
            pos++;

            if (DFA::isAccepting(state))
            {
                lastAccept = state;
                lastAcceptPos = pos;
                lastAcceptLine = line;
                lastAcceptCol = col;
                lastAcceptLexeme = lexeme;
            }

            if (state == STATE_COMMENT_STAR || state == STATE_COMMENT_CURLY)
            {
                lastAccept = STATE_FINAL;
            }
        }
        else
        {
            if (DFA::isAccepting(state))
            {
                emitToken(tokens, state, lexeme, tokenStartLine, tokenStartCol);
                state = STATE_START;
                lexeme.clear();
                lastAccept = STATE_FINAL;
            }
            else if (lastAccept != STATE_FINAL)
            {
                emitToken(tokens, lastAccept, lastAcceptLexeme,
                          tokenStartLine, tokenStartCol);
                pos = lastAcceptPos;
                line = lastAcceptLine;
                col = lastAcceptCol;
                state = STATE_START;
                lexeme.clear();
                lastAccept = STATE_FINAL;
            }
            else
            {
                std::string errLexeme = lexeme.empty() ? std::string(1, c) : lexeme;
                std::cerr << "Lexer error (line " << tokenStartLine
                          << ", col " << tokenStartCol
                          << "): token tidak valid '" << errLexeme << "'" << std::endl;
                tokens.push_back({TOKEN_ERROR, errLexeme, tokenStartLine, tokenStartCol});

                if (lexeme.empty())
                {
                    pos++;
                    if (c == '\n')
                    {
                        line++;
                        col = 1;
                    }
                    else
                    {
                        col++;
                    }
                }
                state = STATE_START;
                lexeme.clear();
                lastAccept = STATE_FINAL;
            }
        }
    }

    if (state != STATE_START)
    {
        if (DFA::isAccepting(state))
        {
            emitToken(tokens, state, lexeme, tokenStartLine, tokenStartCol);
        }
        else if (lastAccept != STATE_FINAL)
        {
            emitToken(tokens, lastAccept, lastAcceptLexeme,
                      tokenStartLine, tokenStartCol);
        }
        else if (!lexeme.empty())
        {
            std::cerr << "Lexer error (line " << tokenStartLine
                      << ", col " << tokenStartCol
                      << "): unexpected end of file, token '" << lexeme << "'" << std::endl;
            tokens.push_back({TOKEN_ERROR, lexeme, tokenStartLine, tokenStartCol});
        }
    }

    return tokens;
}

void Lexer::emitToken(std::vector<Token> &tokens, State state, const std::string &lexeme, int tokenLine, int tokenCol)
{

    TokenType type = DFA::stateToTokenType(state);

    switch (state)
    {
    case STATE_IDENT:
    {
        TokenType kwType = lookupKeyword(lexeme);
        if (kwType != TOKEN_IDENT)
        {
            tokens.push_back({kwType, "", tokenLine, tokenCol});
        }
        else
        {
            tokens.push_back({TOKEN_IDENT, lexeme, tokenLine, tokenCol});
        }
        break;
    }

    case STATE_END_STRING:
    {
        std::string content = "";
        if (lexeme.size() >= 2)
        {
            content = lexeme.substr(1, lexeme.size() - 2);
        }
        std::string evaluatedContent = "";
        for (size_t i = 0; i < content.size(); ++i)
        {
            if (content[i] == '\'' && i + 1 < content.size() && content[i + 1] == '\'')
            {
                evaluatedContent += '\'';
                i++;
            }
            else
            {
                evaluatedContent += content[i];
            }
        }

        std::string finalLexeme = "'" + evaluatedContent + "'";

        if (evaluatedContent.size() == 1)
        {
            tokens.push_back({TOKEN_STRING, finalLexeme, tokenLine, tokenCol});
        }
        else
        {
            tokens.push_back({TOKEN_STRING, finalLexeme, tokenLine, tokenCol});
        }
        break;
    }

    case STATE_INT:
    case STATE_REAL:
    {
        tokens.push_back({type, lexeme, tokenLine, tokenCol});
        break;
    }

    case STATE_COMMENT_FINAL:
    {
        break;
    }

    default:
    {
        tokens.push_back({type, "", tokenLine, tokenCol});
        break;
    }
    }
}
