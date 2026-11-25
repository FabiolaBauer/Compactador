#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include "descompactador.h"
#include "dicionario.h"
#include "fila.h"
#include "formato.h"

/* Mesmo atraso de demonstracao do Compactador (variavel MZ_ATRASO, em
 * microssegundos). Sem a variavel (ou com valor invalido/negativo), nao ha atraso. */
static int atraso_us(void){
  const char *s = getenv("MZ_ATRASO");
  int v = s ? atoi(s) : 0;
  return v > 0 ? v : 0;
}

/* threads compartilham. */
typedef struct {
  FILE *in, *out;
  Queue q1; /* leitura -> descompactacao */
  Queue q2; /* descompactacao -> gravacao */
  Stats *st;
  int erro_leitura; /* escrito so pela thread de leitura */
  int erro_formato; /* escrito so pela thread de descompactacao */
  int erro_grava;   /* escrito so pela thread de gravacao */
  int atraso;       /* MZ_ATRASO; so lido pelas threads */
} Ctx;

static void espera(const Ctx *c){
  if (c->atraso) usleep(c->atraso);
}

/* 1 se o byte abre um codigo de 2 bytes (escape ou palavra com 2 bytes). */
static int dois_bytes(unsigned char c){
  return c == ESCAPE || c >= COD2_INI;
}

/* 1 se o bloco termina no primeiro byte de um codigo de 2 bytes, ou seja,
 * o segundo byte ficou para o proximo bloco. Percorre desde o inicio porque
 * o byte 0xF0 tambem pode ser o segundo byte de outro codigo. */
static int falta_byte(const char *buf, int n){
  int i = 0;
  while (i < n) i += dois_bytes((unsigned char)buf[i]) ? 2 : 1;
  return i > n;
}

/* Le um bloco do arquivo compactado sem cortar um codigo de 2 bytes no meio. */
static int read_block(FILE *f, char *buf){
  int n = fread(buf, 1, BLK - 1, f); /* deixa 1 byte para completar o codigo */
  if (n == 0) return ferror(f) ? -1 : 0;
  if (falta_byte(buf, n)){
    int c = fgetc(f);
    if (c != EOF) buf[n++] = c; /* EOF aqui: arquivo cortado, a descompactacao avisa */
  }

  if(ferror(f)) return -1;
  return n;
}

/* Copia 'len' bytes para o bloco de saida. Uma palavra decodificada ocupa
 * mais que o codigo, entao o texto pode nao caber em um bloco so: quando
 * enche, o bloco vai para a gravacao e comeca outro. */
static void emite(Ctx *c, Block *out, const char *p, int len){
  if (out->len + len > BLK_SAIDA){
    q_push(&c->q2, out);
    out->len = 0;
  }
  memcpy(out->data + out->len, p, len);
  out->len += len;
}

/* Descompacta um bloco e soma as contagens em st. 'em_palavra' guarda se o
 * bloco anterior terminou no meio de uma palavra fora do dicionario, para
 * ela nao ser contada duas vezes. */
static void decompress_block(Ctx *c, const Block *in, Block *out, int *em_palavra){
  long lidas = 0, comp = 0, nao = 0; /* contadores locais do bloco */
  int i = 0;

  while (i < in->len){
    unsigned char b = in->data[i];

    if (b < COD1_INI){ /* texto ASCII */
      if (isalpha(b)){
        if (!*em_palavra){ lidas++; nao++; } /* inicio de palavra fora do dicionario */
        *em_palavra = 1;
      } else {
        *em_palavra = 0;
      }
      emite(c, out, (const char *)&b, 1);
      i++;
      continue;
    }

    *em_palavra = 0;
    if (dois_bytes(b) && i + 1 >= in->len){ /* falta o segundo byte */
      fprintf(stderr, "descompacta: arquivo cortado no meio de um codigo\n");
      c->erro_formato = 1;
      break;
    }

    if (b == ESCAPE){ /* proximo byte e texto */
      emite(c, out, in->data + i + 1, 1);
      i += 2;
      continue;
    }

    /* codigo de palavra (1 ou 2 bytes) */
    int usados;
    const char *w = dic_decodifica((const unsigned char *)in->data + i, &usados);
    if (w){
      emite(c, out, w, strlen(w));
      lidas++;
      comp++;
    } else {
      fprintf(stderr, "aviso: codigo invalido 0x%02X\n", b);
      c->erro_formato = 1;
    }
    i += usados;
  }

  pthread_mutex_lock(&c->st->lock);
  c->st->palavras_lidas += lidas;
  c->st->palavras_compactadas += comp;
  c->st->palavras_nao_encontradas += nao;
  pthread_mutex_unlock(&c->st->lock);
}

static void *thr_leitura(void *arg){
  Ctx *c = arg;
  Block b;
  int n = 0;
  while ((b.len = read_block(c->in, b.data)) > 0){
    pthread_mutex_lock(&c->st->lock);
    c->st->bytes_compactado += b.len;
    pthread_mutex_unlock(&c->st->lock);
    fprintf(stderr, "[leitura]     bloco %d\n", n++);
    espera(c);
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

static void *thr_descompacta(void *arg){
  Ctx *c = arg;
  Block in, out;
  int n = 0;
  int em_palavra = 0; /* passa de um bloco para o outro */
  out.len = 0;
  while (1){
    q_pop(&c->q1, &in);
    if (in.len == 0){ out.len = 0; q_push(&c->q2, &out); break; }
    decompress_block(c, &in, &out, &em_palavra);
    fprintf(stderr, "[descompacta] bloco %d\n", n++);
    espera(c);
    if (out.len > 0){ /* manda o que sobrou; len 0 seria lido como fim */
      q_push(&c->q2, &out);
      out.len = 0;
    }
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
    c->st->bytes_original += var;
    pthread_mutex_unlock(&c->st->lock);
    fprintf(stderr, "[grava]       bloco %d\n", n++);
    espera(c);
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

bool descompactador_run(FILE *in, FILE *out, Stats *st){
  Ctx ctx = {0};
  ctx.in = in; ctx.out = out; ctx.st = st;
  ctx.atraso = atraso_us(); /* lido uma vez, antes de criar as threads */
  q_init(&ctx.q1);
  q_init(&ctx.q2);

  pthread_t t1, t2, t3;
  Block fim; fim.len = 0;
  int r;

  /* criadas de tras para frente, como no Compactador */
  if ((r = pthread_create(&t3, NULL, thr_grava, &ctx)) != 0){
    fprintf(stderr, "pthread_create(grava): %s\n", strerror(r));
    marca_fim(st);

    q_destroy(&ctx.q1);
    q_destroy(&ctx.q2);

    return false;
  }

  if ((r = pthread_create(&t2, NULL, thr_descompacta, &ctx)) != 0){
    fprintf(stderr, "pthread_create(descompacta): %s\n", strerror(r));
    q_push(&ctx.q2, &fim);
    pthread_join(t3, NULL);

    q_destroy(&ctx.q1);
    q_destroy(&ctx.q2);

    marca_fim(st);
    return false;
  }

  if ((r = pthread_create(&t1, NULL, thr_leitura, &ctx)) != 0){
    fprintf(stderr, "pthread_create(leitura): %s\n", strerror(r));
    q_push(&ctx.q1, &fim);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    q_destroy(&ctx.q1);
    q_destroy(&ctx.q2);

    marca_fim(st);
    return false;
  }

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  pthread_join(t3, NULL);

  q_destroy(&ctx.q1);
  q_destroy(&ctx.q2);

  marca_fim(st);
  return !ctx.erro_leitura && !ctx.erro_formato && !ctx.erro_grava;
}
