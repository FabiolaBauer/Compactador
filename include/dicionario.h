#ifndef DICIONARIO_H
#define DICIONARIO_H

int dic_codifica(const char *w, unsigned char *out);
const char *dic_decodifica(const unsigned char *p, int *usados);
void dic_init(void);

#endif
