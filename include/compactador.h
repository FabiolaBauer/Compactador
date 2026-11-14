/*
 * compactador.h - processo Compactador: 3 threads (leitura, compactacao, gravacao)
 * ligadas por duas filas.
 */
#ifndef COMPACTADOR_H
#define COMPACTADOR_H

#include <stdio.h>
#include "common.h"

/* Compacta 'in' em 'out' usando 3 threads. Atualiza as estatisticas em 'st'
 * e marca st->terminou = 1 ao final. */
void compactador_run(FILE *in, FILE *out, Stats *st);

#endif
