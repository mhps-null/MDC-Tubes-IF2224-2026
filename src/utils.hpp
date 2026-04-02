#pragma once

#include "token.hpp"
#include <string>

/**
 * Mengubah semua karakter dalam string menjadi huruf kecil.
 * Digunakan untuk perbandingan keyword yang case-insensitive.
 */
std::string toLower(const std::string &str);

/**
 * Mengecek apakah identifier adalah keyword.
 * Jika cocok, mengembalikan TokenType keyword, jika tidak → TOKEN_IDENT.
 */
TokenType lookupKeyword(const std::string &identifier);

/**
 * Membaca seluruh isi file ke dalam string.
 * Mengembalikan true jika berhasil, false jika gagal membuka file.
 */
bool readFile(const std::string &path, std::string &content);

/**
 * Menulis string ke file.
 * Mengembalikan true jika berhasil, false jika gagal membuka file.
 */
bool writeFile(const std::string &path, const std::string &content);