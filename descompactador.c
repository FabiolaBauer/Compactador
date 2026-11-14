#include "descompactador.h"
#include "dicionario.h"

void decompress(FILE *in, FILE *out){
  int c;
  while ((c = fgetc(in)) != EOF){ /* le byte do arquivo */
    const char *w = sym_to_word(c); /* tenta achar a palavra correspondente */
    if (w) fputs(w, out); else fputc(c, out);
  }
}
