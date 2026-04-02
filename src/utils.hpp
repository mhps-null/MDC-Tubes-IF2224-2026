#pragma once

#include "token.hpp"
#include <string>

// konversi string ke lowercase
// dipakai buat keyword matching yang case-insensitive
std::string toLower(const std::string &str);

// cek apakah identifier ini keyword Arion atau bukan
// kalau iya → return TokenType keyword-nya
// kalau bukan → return TOKEN_IDENT
// case-insensitive: "BEGIN", "Begin", "begin" semua → TOKEN_BEGINSY
TokenType lookupKeyword(const std::string &identifier);

// baca seluruh isi file ke string
// return true kalau berhasil, false kalau gagal
bool readFile(const std::string &path, std::string &content);

// tulis string ke file
// return true kalau berhasil, false kalau gagal
bool writeFile(const std::string &path, const std::string &content);
