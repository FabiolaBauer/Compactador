#include "fila.h"

void q_init(Queue *q){
  q->head = q->tail = q->count = 0;
  pthread_mutex_init(&q->m, NULL);
  pthread_cond_init(&q->not_full, NULL);
  pthread_cond_init(&q->not_empty, NULL);
}

void q_push(Queue *q, const Block *b){
  pthread_mutex_lock(&q->m);
  while (q->count == QSIZE) pthread_cond_wait(&q->not_full, &q->m);
  q->buf[q->tail] = *b;
  q->tail = (q->tail + 1) % QSIZE;
  q->count++;
  pthread_cond_signal(&q->not_empty);
  pthread_mutex_unlock(&q->m);
}

void q_pop(Queue *q, Block *b){
  pthread_mutex_lock(&q->m);
  while (q->count == 0) pthread_cond_wait(&q->not_empty, &q->m);
  *b = q->buf[q->head];
  q->head = (q->head + 1) % QSIZE;
  q->count--;
  pthread_cond_signal(&q->not_full);
  pthread_mutex_unlock(&q->m);
}
