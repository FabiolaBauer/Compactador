#ifndef STATS_H
#define STATS_H

#include "common.h"

/* Cria o segmento compartilhado, mapeia e inicializa o Stats (mutex incluso). */
Stats *stats_create(void);

/* Desfaz o mapeamento e remove o segmento do sistema. */
void stats_destroy(Stats *s);

#endif
