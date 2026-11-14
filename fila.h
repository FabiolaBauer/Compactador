/*
 * Sincronizacao: 1 mutex + 2 variaveis de condicao (not_full / not_empty).
 */
#ifndef FILA_H
#define FILA_H

#include <pthread.h>
#include "common.h"

typedef struct {
  Block buf[QSIZE];
  int head, tail, count;
  pthread_mutex_t m;
  pthread_cond_t not_full, not_empty;
} Queue;

void q_init(Queue *q);
void q_push(Queue *q, const Block *b); /* bloqueia se a fila esta cheia */
void q_pop(Queue *q, Block *b); /* bloqueia se a fila esta vazia */

#endif
