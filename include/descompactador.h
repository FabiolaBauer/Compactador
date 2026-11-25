/*
 * descompactador.h - processo Descompactador: 3 threads (leitura, descompactacao,
 * gravacao) ligadas por duas filas. Mesmo modelo do Compactador.
 */
#ifndef DESCOMPACTADOR_H
#define DESCOMPACTADOR_H

#include <stdio.h>
#include "common.h"
#include <stdbool.h>

/* Descompacta 'in' em 'out' usando 3 threads. Atualiza as estatisticas em 'st'
 * e marca st->terminou = 1 ao final. */
bool descompactador_run(FILE *in, FILE *out, Stats *st);

#endif
