#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include "compactador.h"
#include "dicionario.h"
#include "fila.h"

/* threads compartilham. */
typedef struct {
  FILE *in, *out;
  Queue q1; /* leitura -> compactacao */
  Queue q2; /* compactacao -> gravacao */
  Stats *st;
} Ctx;

static int read_block(FILE *f, char *buf){
  int n = fread(buf, 1, BLK - FOLGA, f);
  if (n == 0) return 0;
  if (isalpha((unsigned char)buf[n-1])){
    int c;
    while (n < BLK && (c = fgetc(f)) != EOF){
      buf[n++] = c;
      if (!isalpha(c)) break;
    }
  }
  return n;
}

static void flush_word(char *w, int *wl, Block *out, int *o, long *lidas, long *comp, long *nao){
  w[*wl] = 0;
  (*lidas)++;
  int sym = word_to_sym(w);
  if (sym >= 0) {
    out->data[(*o)++] = sym;
    (*comp)++;
  } else {
    memcpy(out->data + *o, w, *wl);
    *o += *wl;
    (*nao)++;
  }
  *wl = 0;
}

/* Compacta um bloco de memoria em outro e soma as contagens em st. */
static void compress_block(const Block *in, Block *out, Stats *st){
  int o = 0; /* posicao de escrita em out->data */
  int wl = 0; /* comprimento da palavra atual */
  char w[256]; /* buffer temporario da palavra */
  long lidas = 0, comp = 0, nao = 0; /* contadores locais do bloco */

  for (int i = 0; i < in->len; i++){
    unsigned char c = in->data[i];
    if (isalpha(c)){
      if (wl == 255){
        memcpy(out->data + o, w, wl);
        o += wl;
        wl = 0;
      }
      w[wl++] = c;
    } else {
      if (wl) flush_word(w, &wl, out, &o, &lidas, &comp, &nao);
      if (sym_to_word(c))
        fprintf(stderr, "aviso: o texto contem '%c', que e simbolo do dicionario\n", c);
      out->data[o++] = c;
    }
  }
  if (wl) flush_word(w, &wl, out, &o, &lidas, &comp, &nao);
  out->len = o;

  pthread_mutex_lock(&st->lock);
  st->palavras_lidas += lidas;
  st->palavras_compactadas += comp;
  st->palavras_nao_encontradas += nao;
  pthread_mutex_unlock(&st->lock);
}

static void *thr_leitura(void *arg){
  Ctx *c = arg;
  Block b;
  int n = 0;
  while ((b.len = read_block(c->in, b.data)) > 0){
    pthread_mutex_lock(&c->st->lock);
    c->st->bytes_original += b.len;
    pthread_mutex_unlock(&c->st->lock);
    fprintf(stderr, "[leitura]    bloco %d\n", n++);
    q_push(&c->q1, &b);
  }
  b.len = 0;
  q_push(&c->q1, &b);
  return NULL;
}

static void *thr_compacta(void *arg){
  Ctx *c = arg;
  Block in, out;
  int n = 0;
  while (1){
    q_pop(&c->q1, &in);
    if (in.len == 0){ out.len = 0; q_push(&c->q2, &out); break; }
    compress_block(&in, &out, c->st);
    fprintf(stderr, "[compacta]   bloco %d\n", n++);
    q_push(&c->q2, &out);
  }
  return NULL;
}

static void *thr_grava(void *arg){
  Ctx *c = arg;
  Block b;
  int n = 0;
  while (1){
    q_pop(&c->q2, &b);
    if (b.len == 0) break;
    fwrite(b.data, 1, b.len, c->out);
    pthread_mutex_lock(&c->st->lock);
    c->st->bytes_compactado += b.len;
    pthread_mutex_unlock(&c->st->lock);
    fprintf(stderr, "[grava]      bloco %d\n", n++);
  }
  return NULL;
}

void compactador_run(FILE *in, FILE *out, Stats *st){
  Ctx ctx;
  ctx.in = in; ctx.out = out; ctx.st = st;
  q_init(&ctx.q1);
  q_init(&ctx.q2);

  pthread_t t1, t2, t3;
  pthread_create(&t1, NULL, thr_leitura,  &ctx);
  pthread_create(&t2, NULL, thr_compacta, &ctx);
  pthread_create(&t3, NULL, thr_grava,    &ctx);
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  pthread_join(t3, NULL);

  pthread_mutex_lock(&st->lock);
  st->terminou = 1; /* avisa o Monitor */
  pthread_mutex_unlock(&st->lock);
}
