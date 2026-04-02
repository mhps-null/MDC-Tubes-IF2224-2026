#include "utils.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <map>

std::string toLower(const std::string &str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

static const std::map<std::string, TokenType> keywordTable = {
    {"not", TOKEN_NOTSY},
    {"div", TOKEN_IDIV},
    {"mod", TOKEN_IMOD},
    {"and", TOKEN_ANDSY},
    {"or", TOKEN_ORSY},

    {"const", TOKEN_CONSTSY},
    {"type", TOKEN_TYPESY},
    {"var", TOKEN_VARSY},
    {"function", TOKEN_FUNCTIONSY},
    {"procedure", TOKEN_PROCEDURESY},
    {"array", TOKEN_ARRAYSY},
    {"record", TOKEN_RECORDSY},
    {"program", TOKEN_PROGRAMSY},

    {"begin", TOKEN_BEGINSY},
    {"if", TOKEN_IFSY},
    {"case", TOKEN_CASESY},
    {"repeat", TOKEN_REPEATSY},
    {"while", TOKEN_WHILESY},
    {"for", TOKEN_FORSY},
    {"end", TOKEN_ENDSY},
    {"else", TOKEN_ELSESY},
    {"until", TOKEN_UNTILSY},
    {"of", TOKEN_OFSY},
    {"do", TOKEN_DOSY},
    {"to", TOKEN_TOSY},
    {"downto", TOKEN_DOWNTOSY},
    {"then", TOKEN_THENSY}};

TokenType lookupKeyword(const std::string &identifier)
{
    std::string lower = toLower(identifier);
    auto it = keywordTable.find(lower);
    if (it != keywordTable.end())
    {
        return it->second;
    }
    return TOKEN_IDENT;
}

bool readFile(const std::string &path, std::string &content)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    file.close();
    return true;
}

bool writeFile(const std::string &path, const std::string &content)
{
    std::ofstream file(path);
    if (!file.is_open())
    {
        return false;
    }
    file << content;
    file.close();
    return true;
}
