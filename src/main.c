/*
 * mz - compactador/descompactador paralelo.
 *
 *   make
 *   ./mz -c entrada.txt compactado.mzp
 *   ./mz -d compactado.mzp saida.txt
 *
 * Regra: um simbolo do dicionario NAO pode aparecer no texto original,
 * senao o descompactador nao sabe se ele e simbolo ou pontuacao do texto.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "common.h"
#include "stats.h"
#include "compactador.h"
#include "descompactador.h"
#include "monitor.h"

int main(int argc, char **argv){
  if (argc != 4) { printf("uso: %s -c|-d entrada.txt saida.txt\n", argv[0]); return 1; }

  FILE *in  = fopen(argv[2], "rb");
  if (!in)  { perror(argv[2]); return 1; }
  FILE *out = fopen(argv[3], "wb");
  if (!out) { perror(argv[3]); return 1; }

  if (argv[1][1] == 'c') {
    Stats *st = stats_create(); /* memoria compartilhada ANTES do fork */

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) { /* FILHO: Monitor */
      monitor_loop(st);
      exit(0);
    }

    compactador_run(in, out, st); /* PAI: Compactador (3 threads) */

    waitpid(pid, NULL, 0); /* espera o Monitor imprimir o resumo */
    stats_destroy(st);
  } else {
    decompress(in, out);
  }

  fclose(in);
  fclose(out);
  return 0;
}
