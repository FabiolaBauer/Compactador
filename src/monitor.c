
#include <stdio.h>
#include <pthread.h>
#include "monitor.h"

void monitor_loop(Stats *s){
    long palavras_lidas;
    long palavras_compactadas;
    long palavras_nao_encontradas;
    long bytes_original;
    long bytes_compactado;
    char modo;

    /* Aguarda o processo de trabalho terminar. */
    while (1){
        pthread_mutex_lock(&s->lock);
        int terminou = s->terminou;
        pthread_mutex_unlock(&s->lock);

        if (terminou)
            break;
    }

    /* Copia as estatisticas finais com protecao do mutex. */
    pthread_mutex_lock(&s->lock);

    palavras_lidas = s->palavras_lidas;
    palavras_compactadas = s->palavras_compactadas;
    palavras_nao_encontradas = s->palavras_nao_encontradas;
    bytes_original = s->bytes_original;
    bytes_compactado = s->bytes_compactado;
    modo = s->modo;

    pthread_mutex_unlock(&s->lock);

    if (modo == 'c'){
        printf("[monitor - compactacao] lidas=%ld comp=%ld nao=%ld original=%ld compactado=%ld\n",
               palavras_lidas, palavras_compactadas,
               palavras_nao_encontradas, bytes_original,
               bytes_compactado);
    } else {
        printf("[monitor - descompactacao] lidas=%ld comp=%ld nao=%ld compactado_lido=%ld original_reconstruido=%ld\n",
               palavras_lidas, palavras_compactadas,
               palavras_nao_encontradas, bytes_compactado,
               bytes_original);
    }

    if (bytes_original > 0){
        double taxa = 100.0 *
            (1.0 - (double)bytes_compactado / bytes_original);

        printf("[monitor] taxa de compactacao: %.1f%%\n", taxa);
    }
}
