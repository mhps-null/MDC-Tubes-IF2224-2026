// main.cpp
// Entry point buat Arion Lexer.
//
// Cara pakai:
//   ./lexer <input.txt> [output.txt]
//
// Kalau output.txt ga dikasih, output cuma ke terminal.
// Kalau dikasih, output ke terminal DAN ke file.

#include "lexer.hpp"
#include "utils.hpp"
#include <iostream>
#include <string>
#include <sstream>

int main(int argc, char* argv[]) {
    // cek argumen dulu
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.txt> [output.txt]" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = (argc >= 3) ? argv[2] : "";

    // baca file input
    std::string source;
    if (!readFile(inputPath, source)) {
        std::cerr << "Error: tidak dapat membuka file '" << inputPath << "'" << std::endl;
        return 1;
    }

    // jalankan lexer
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan();

    // format output
    std::stringstream output;
    for (const Token& tok : tokens) {
        if (tok.type == TOKEN_ERROR) {
            output << "ERROR (" << tok.value << ")" << std::endl;
        } else {
            output << tokenToString(tok) << std::endl;
        }
    }

    // cetak ke terminal
    std::cout << output.str();

    // kalau ada path output, tulis ke file juga
    if (!outputPath.empty()) {
        if (!writeFile(outputPath, output.str())) {
            std::cerr << "Error: tidak dapat menulis ke file '" << outputPath << "'" << std::endl;
            return 1;
        }
        std::cout << "\nOutput berhasil ditulis ke " << outputPath << std::endl;
    }

    return 0;
}
