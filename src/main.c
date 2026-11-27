/*
 * mz - compactador/descompactador paralelo.
 *
 *   make
 *   ./mz -c entrada.txt compactado.mzp
 *   ./mz -d compactado.mzp saida.txt
 *
 * Nos dois modos: o processo pai faz o trabalho (Compactador ou
 * Descompactador, com 3 threads) e o filho e o Monitor.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "common.h"
#include "stats.h"
#include "compactador.h"
#include "descompactador.h"
#include "monitor.h"
#include "dicionario.h"

int main(int argc, char **argv){
  if (argc != 4) { 
    printf("uso: %s -c|-d entrada.txt saida.txt\n", argv[0]); return 1; 
  }

  if (strcmp(argv[1], "-c") != 0 && strcmp(argv[1], "-d") != 0) {
    fprintf(stderr, "modo invalido '%s': use -c ou -d\n", argv[1]);
    return 1;
  }
  char modo = argv[1][1]; /* 'c' ou 'd' */

  dic_init();

  FILE *in  = fopen(argv[2], "rb");
  if (!in)  { perror(argv[2]); return 1; }
  FILE *out = fopen(argv[3], "wb");
  if (!out) { perror(argv[3]); fclose(in); return 1; }

  int ok = 1;

  Stats *st = stats_create(); /* memoria compartilhada ANTES do fork */
  st->modo = modo;

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    stats_destroy(st);
    fclose(in);
    fclose(out);
    return 1;
  }
  if (pid == 0) { /* FILHO: Monitor (nos dois modos) */
    monitor_loop(st);
    fflush(stdout);
    _exit(0); /* _exit: nao descarrega os FILE* herdados do pai */
  }

  /* PAI: Compactador ou Descompactador (3 threads cada). Mesmo com erro, segue
   * para a limpeza: os dois sempre marcam st->terminou, entao o Monitor termina. */
  if (modo == 'c') {
    if (!compactador_run(in, out, st)) {
      fprintf(stderr, "compactacao falhou\n");
      ok = 0;
    }
  } else {
    if (!descompactador_run(in, out, st)) {
      fprintf(stderr, "descompactacao falhou\n");
      ok = 0;
    }
  }

  waitpid(pid, NULL, 0); /* espera o Monitor imprimir o resumo */
  stats_destroy(st);

  fclose(in);
  if (fclose(out) != 0) { /* fclose grava o que restou no buffer */
    perror(argv[3]);
    ok = 0;
  }
  return ok ? 0 : 1;
}
