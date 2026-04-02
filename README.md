# MDC-Tubes-IF2224-2026

> Tugas Besar IF2224 Teori Bahasa Formal dan Automata

<p align="center"> <img src="doc/preview.png" width="60%"/></p>

## Identitas Kelompok

- Ariel Cornelius Sitorus - 13524085
- Muhammad Haris Putra Sulastianto - 13524085
- Vara Azzara Ramli Pulukadang - 13524091
- Nathan Adhika Santosa - 13524041

---

## Deskripsi Program

Program ini merupakan implementasi lexer berbasis DFA untuk bahasa pemrograman Arion. Lexer membaca berkas kode sumber, melakukan pemindaian karakter demi karakter, lalu menghasilkan daftar token sesuai spesifikasi bahasa Arion.

Token yang dikenali meliputi literal (integer, real, karakter, string), operator aritmatika dan relasional, keyword, identifier, delimiter, serta komentar. Secara keseluruhan, terdapat 52 jenis token. Komentar tetap dikenali, tetapi tidak ditampilkan pada keluaran.

Alur kerja program:

1. Membaca berkas masukan dari argumen command line.
2. Menjalankan proses analisis leksikal menggunakan kelas `Lexer`.
3. Menampilkan token ke terminal.
4. Jika argumen berkas keluaran diberikan, hasil juga ditulis ke berkas.

---

## Prasyarat

### Prasyarat Utama

- Compiler C++ dengan dukungan standar C++17 (contoh: GCC, Clang, atau MinGW-w64)
- GNU Make

### Lingkungan Eksekusi yang Direkomendasikan

- Linux (native)
- Windows Subsystem for Linux (WSL)
- Git Bash pada Windows

PowerShell tetap dapat digunakan. Namun, saat menjalankan executable secara manual, gunakan format perintah Windows (contoh: `.\lexer.exe ...`).

### Verifikasi Instalasi

Gunakan perintah berikut untuk memastikan dependency utama telah tersedia:

```bash
g++ --version
make --version
```

---

## Cara Instalasi dan Penggunaan Program

### Quick Start

Kompilasi program:

```bash
make
```

Jalankan pengujian bawaan:

```bash
make run

# akan membaca masukan dan menulis keluaran pada
# Input: `test/milestone-1/input-1.txt`
# Output: `test/milestone-1/output-1.txt`
```

Bersihkan hasil kompilasi:

```bash
make clean
```

### Menjalankan Program Manual

Format umum:

```bash
# relatif terhadap folder test
./lexer <input_file>.txt <output_file>.txt

# Contoh:
./lexer milestone-1/input-1.txt milestone-1/output-1.txt
```

`<output_file>` bersifat opsional.

### Windows (PowerShell / Git Bash / WSL)

Kompilasi program:

```bash
make
```

Jalankan lexer secara manual:

```bash
./lexer.exe test/milestone-1/input-1.txt [test/milestone-1/output-1.txt]
```

Untuk PowerShell, disarankan menggunakan format path Windows:

```powershell
.\lexer.exe test\milestone-1\input-1.txt test\milestone-1\output-1.txt
```

Jalankan pengujian bawaan:

```bash
make run
```

Bersihkan hasil kompilasi:

```bash
make clean
```

### Linux

Kompilasi program:

```bash
make
```

Jalankan lexer secara manual:

```bash
./lexer test/milestone-1/input-1.txt [test/milestone-1/output-1.txt]
```

Jalankan pengujian bawaan:

```bash
make run
```

Bersihkan hasil kompilasi:

```bash
make clean
```

---

## Pembagian Tugas

| Nama                             | Tugas |
| -------------------------------- | ----- |
| Ariel Cornelius Sitorus          | ...   |
| Muhammad Haris Putra Sulastianto | ...   |
| Vara Azzara Ramli Pulukadang     | ...   |
| Nathan Adhika Santosa            | ...   |
