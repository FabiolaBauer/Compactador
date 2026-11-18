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
  int erro_leitura;
  int erro_grava;
} Ctx;

static int read_block(FILE *f, char *buf){
  int n = fread(buf, 1, BLK - FOLGA, f);
  if (n == 0) return ferror(f) ? -1 : 0;
  if (isalpha((unsigned char)buf[n-1])){
    int c;
    while (n < BLK && (c = fgetc(f)) != EOF){
      buf[n++] = c;
      if (!isalpha(c)) break;
    }
  }

  if(ferror(f)) return -1;
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

  if (b.len < 0) {
    perror("leitura");
    c->erro_leitura = 1;
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
    if(c->erro_grava) continue;
    
    size_t var = fwrite(b.data, 1, b.len, c->out);
    if( var != (size_t)b.len) {
      perror("fwrite");
      c->erro_grava = 1;
    }

    pthread_mutex_lock(&c->st->lock);
    c->st->bytes_compactado += var;
    pthread_mutex_unlock(&c->st->lock);
    fprintf(stderr, "[grava]      bloco %d\n", n++);
  }
  if (!c->erro_grava && fflush(c->out) != 0){ 
    perror("fflush"); 
    c->erro_grava = 1; 
  }

  return NULL;
}

//Marca o fim para o monitor, mesmo que tenha ocorrido erro de leitura ou gravação.
static void marca_fim(Stats *st){
  pthread_mutex_lock(&st->lock);
  st->terminou = 1;
  pthread_mutex_unlock(&st->lock);
}

bool compactador_run(FILE *in, FILE *out, Stats *st){
  Ctx ctx = {0};
  ctx.in = in; ctx.out = out; ctx.st = st;
  q_init(&ctx.q1);
  q_init(&ctx.q2);

  pthread_t t1, t2, t3;
  Block fim; fim.len = 0;
  int r;

  if ((r = pthread_create(&t3, NULL, thr_grava, &ctx)) != 0){
    fprintf(stderr, "pthread_create(grava): %s\n", strerror(r));
    marca_fim(st); 
    return false;
  }

  if ((r = pthread_create(&t2, NULL, thr_compacta, &ctx)) != 0){
    fprintf(stderr, "pthread_create(compacta): %s\n", strerror(r));
    q_push(&ctx.q2, &fim);
    pthread_join(t3, NULL);
    marca_fim(st); 
    return false;
  }

  if ((r = pthread_create(&t1, NULL, thr_leitura, &ctx)) != 0){
    fprintf(stderr, "pthread_create(leitura): %s\n", strerror(r));
    q_push(&ctx.q1, &fim); 
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    marca_fim(st); 
    return false;
  }

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  pthread_join(t3, NULL);

  marca_fim(st);
  return !ctx.erro_leitura && !ctx.erro_grava;
}
