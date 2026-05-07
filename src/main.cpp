#include "lexer.hpp"
#include "utils.hpp"
#include "parser.hpp"
#include "parsetree.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <input.txt> [output.txt]" << std::endl;
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

    fs::path baseDir = current / "test";

    // =========================
    // INPUT PATH
    // =========================
    fs::path inputPath = argv[1];

    if (!inputPath.is_absolute())
    {
        inputPath = baseDir / inputPath;
    }

    // =========================
    // OUTPUT PATH
    // =========================
    fs::path outputPath;

    if (argc >= 3)
    {
        outputPath = argv[2];

        if (!outputPath.is_absolute())
        {
            outputPath = baseDir / outputPath;
        }
    }

    // =========================
    // READ SOURCE FILE
    // =========================
    std::string source;

    if (!readFile(inputPath.string(), source))
    {
        std::cerr << "Error: tidak dapat membuka file '"
                  << inputPath << "'" << std::endl;
        return 1;
    }

    // =========================
    // LEXER
    // =========================
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan();

    // =========================
    // PRINT TOKENS
    // =========================
    std::stringstream tokenOutput;

    for (const Token &tok : tokens)
    {
        if (tok.type == TOKEN_UNKNOWN)
        {
            tokenOutput << "UNKNOWN (" << tok.value << ")\n";
        }
        else
        {
            tokenOutput << tokenToString(tok) << '\n';
        }
    }

    std::cout << "=== TOKENS ===\n";
    std::cout << tokenOutput.str() << '\n';

    // =========================
    // PARSER
    // =========================
    initParser(tokens);

    auto root = parse();

    // =========================
    // PRINT PARSE TREE
    // =========================
    std::cout << "=== PARSE TREE ===\n";

    printTree(root, std::cout);

    // =========================
    // WRITE OUTPUT FILE
    // =========================
    if (!outputPath.empty())
    {
        fs::create_directories(outputPath.parent_path());

        std::ofstream outFile(outputPath.string());

        if (!outFile.is_open())
        {
            std::cerr << "Error: tidak dapat menulis ke file '"
                      << outputPath << "'" << std::endl;
            return 1;
        }

        outFile << "=== TOKENS ===\n";
        outFile << tokenOutput.str() << '\n';

        outFile << "=== PARSE TREE ===\n";
        printTree(root, outFile);

        outFile.close();

        std::cout << "\nOutput berhasil ditulis ke "
                  << outputPath << std::endl;
    }

    // return hadError() ? 1 : 0;

    return 0;
}