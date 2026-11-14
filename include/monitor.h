#ifndef MONITOR_H
#define MONITOR_H

#include "common.h"

/* Le periodicamente o Stats compartilhado e imprime o andamento.
 * Termina (e imprime o resumo) quando st->terminou == 1.
 * Versao provisoria. */
void monitor_loop(Stats *st);

#endif
