# MDC-Tubes-IF2224-2026

> Tugas Besar IF2224 Teori Bahasa Formal dan Automata

<p align="center"> <img src="doc/preview.png" width="60%"/></p>

## Identitas Kelompok

- Nathan Adhika Santosa - 13524041
- Muhammad Haris Putra Sulastianto - 13524053
- Ariel Cornelius Sitorus - 13524085
- Vara Azzara Ramli Pulukadang - 13524091

---

## Deskripsi Program

Program ini merupakan implementasi lexer berbasis DFA untuk bahasa pemrograman Arion. Lexer membaca file kode sumber, melakukan pemindaian karakter demi karakter, lalu menghasilkan daftar token sesuai spesifikasi bahasa Arion.

Token yang dikenali meliputi literal (integer, real, karakter, string), operator aritmatika dan relasional, keyword, identifier, delimiter, serta komentar. Secara keseluruhan, terdapat 52 jenis token. Komentar tetap dikenali, tetapi tidak ditampilkan pada keluaran.

Alur kerja program:

1. Membaca file masukan dari argumen command line.
2. Menjalankan proses analisis leksikal menggunakan kelas `Lexer`.
3. Menampilkan token ke terminal.
4. Jika argumen file keluaran diberikan, hasil juga ditulis ke file.

---

## Requirements

- Compiler C++17 (contoh: GCC, Clang, atau MinGW-w64)
- GNU Make

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

# akan membaca file input dan menulis output pada
# Input: `milestone-1/input-1.txt`
# Output: `milestone-1/output-1.txt`
```

Bersihkan hasil kompilasi:

```bash
make clean
```

### Menjalankan Program Manual

Format umum (relatif terhadap folder `test/`):

```bash
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
./lexer.exe milestone-1/input-1.txt [milestone-1/output-1.txt]
```

Untuk PowerShell, disarankan menggunakan format path Windows:

```powershell
.\lexer.exe milestone-1\input-1.txt milestone-1\output-1.txt
```

Jalankan pengujian bawaan:

### Linux

Kompilasi program:

```bash
make
```

Jalankan lexer secara manual:

```bash
./lexer milestone-1/input-1.txt [milestone-1/output-1.txt]
```

---

## Pembagian Tugas

| NIM      | Nama                             | Tugas                         |
| -------- | -------------------------------- | ----------------------------- |
| 13524041 | Nathan Adhika Santosa            | Laporan, Source Code, Diagram |
| 13524053 | Muhammad Haris Putra Sulastianto | Laporan, Source Code, Diagram |
| 13524085 | Ariel Cornelius Sitorus          | Source Code                   |
| 13524091 | Vara Azzara Ramli Pulukadang     | Laporan                       |
