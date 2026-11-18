#include "descompactador.h"
#include "dicionario.h"
#include "formato.h"

void decompress(FILE *in, FILE *out){
  int c;
  while ((c = fgetc(in)) != EOF){
    if (c < COD1_INI){ /* texto ASCII */
      fputc(c, out);
    } else if (c == ESCAPE){ /* proximo byte e texto */
      int x = fgetc(in);
      if (x == EOF) break; /* arquivo cortado */
      fputc(x, out);
    } else { /* codigo de palavra */
      unsigned char p[2] = { (unsigned char)c, 0 };
      if (c >= COD2_INI){
        int y = fgetc(in);
        if (y == EOF) break;
        p[1] = y;
      }
      int usados;
      const char *w = dic_decodifica(p, &usados);
      if (w) fputs(w, out);
      else fprintf(stderr, "aviso: codigo invalido 0x%02X\n", c);
    }
  }
}
