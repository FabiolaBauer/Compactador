#ifndef MONITOR_H
#define MONITOR_H

#include "common.h"

/* Acompanha as estatisticas compartilhadas e apresenta
 * o resumo final da compactacao ou descompactacao. */
void monitor_loop(Stats *st);

#endif
