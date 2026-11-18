/*
 * tipos e constantes compartilhados por todos os modulos.
 */
#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>

#define BLK   256 /* tamanho maximo de um bloco, em bytes */
#define BLK_SAIDA (BLK * 2) /* tamanho maximo de um bloco compactado, em bytes */
#define FOLGA 64 /* maior palavra esperada: sobra p/ completar bloco */
#define QSIZE 4 /* capacidade de cada fila entre as threads */

#define SHM_NAME "/mz_stats" /* nome do segmento de memoria compartilhada */

/* Estatisticas compartilhadas entre processos (vivem em shared memory). */
typedef struct {
  pthread_mutex_t lock; /* PTHREAD_PROCESS_SHARED: vale entre processos */
  long palavras_lidas;
  long palavras_compactadas;
  long palavras_nao_encontradas;
  long bytes_original;
  long bytes_compactado;
  int  terminou; /* o processo de trabalho avisa o Monitor */
} Stats;

/* len == 0 marca o fim. */
typedef struct {
  char data[BLK_SAIDA];
  int  len;
} Block;

#endif
