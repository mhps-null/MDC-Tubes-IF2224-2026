#include "lexer.hpp"
#include "utils.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
namespace fs = std::filesystem;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <input.txt> [output.txt]" << std::endl;
        return 1;
    }

    fs::path current = fs::canonical(argv[0]).parent_path();

    while (!fs::exists(current / "test"))
    {
        if (current == current.parent_path())
        {
            std::cerr << "Error: folder 'test' tidak ditemukan\n";
            return 1;
        }
        current = current.parent_path();
    }

    fs::path baseDir = current;

    // input
    fs::path inputPath = argv[1];
    if (!inputPath.is_absolute())
    {
        inputPath = baseDir / inputPath;
    }

    // output
    fs::path outputPath;
    if (argc >= 3)
    {
        outputPath = argv[2];
        if (!outputPath.is_absolute())
        {
            outputPath = baseDir / outputPath;
        }
    }

    std::string source;
    if (!readFile(inputPath.string(), source))
    {
        std::cerr << "Error: tidak dapat membuka file '" << inputPath << "'" << std::endl;
        return 1;
    }

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan();

    std::stringstream output;
    for (const Token &tok : tokens)
    {
        if (tok.type == TOKEN_ERROR)
        {
            output << "ERROR (" << tok.value << ")" << std::endl;
        }
        else
        {
            output << tokenToString(tok) << std::endl;
        }
    }

    std::cout << output.str();

    if (!outputPath.empty())
    {
        if (!writeFile(outputPath.string(), output.str()))
        {
            std::cerr << "Error: tidak dapat menulis ke file '" << outputPath << "'" << std::endl;
            return 1;
        }
        std::cout << "\nOutput berhasil ditulis ke " << outputPath << std::endl;
    }

    return 0;
}
