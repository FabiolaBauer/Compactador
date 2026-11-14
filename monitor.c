#include <stdio.h>
#include <unistd.h>
#include "monitor.h"

void monitor_loop(Stats *s){
  int fim = 0;
  while (!fim){
    usleep(100000);
    pthread_mutex_lock(&s->lock);
    printf("[monitor] lidas=%ld comp=%ld nao=%ld orig=%ld comprimido=%ld\n",
           s->palavras_lidas, s->palavras_compactadas, s->palavras_nao_encontradas,
           s->bytes_original, s->bytes_compactado);
    fim = s->terminou;
    pthread_mutex_unlock(&s->lock);
  }
  if (s->bytes_original > 0)
    printf("[monitor] compressao: %.1f%%\n",
           100.0 * (1.0 - (double)s->bytes_compactado / s->bytes_original));
}
