#ifndef FORMATO_H
#define FORMATO_H

#define COD1_INI 0x80 // range para codigos de 1 byte
#define COD1_FIM 0xEE // código superior aos ASCII
#define ESCAPE 0xEF // caractere de escape para dizer que é um símbolo literal
#define COD2_INI 0xF0 // range para codigos de 2 bytes
#define COD2_FIM 0xFF

#define N_COD1 (COD1_FIM - COD1_INI + 1)
#define N_COD2 ((COD2_FIM - COD2_INI + 1) * 256)
#define MAX_PALAVRAS (N_COD1 + N_COD2)

#endif 