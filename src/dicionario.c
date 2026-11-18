#include <string.h>
#include <stdlib.h>
#include "dicionario.h"
#include "formato.h"

static const char *WORD[] = {
  "e",  "de",  "o",  "compressao",  "programa",  "dicionario",  "do",  "texto",
  "a",  "para",  "com",  "arquivos",  "em",  "palavras",  "arquivo",  "quando",
  "que",  "um",  "exemplo",  "como",  "muitas",  "O",  "apenas",  "este",
  "foi",  "nao",  "palavra",  "threads",  "uma",  "vezes",  "ainda",  "aparecem",
  "assim",  "cada",  "curtas",  "da",  "diferentes",  "escolhemos",  "final",  "frases",
  "grava",  "mesmo",  "mostra",  "muito",  "no",  "ocorrencias",  "original",  "os",
  "ou",  "pequeno",  "pode",  "por",  "processo",  "processos",  "repeticoes",  "se",
  "simbolo",  "simples",  "util",  "varias",  "academicos",  "ajuda",  "aluno",  "ao",
  "aprendizado",  "aqui",  "as",  "Assim",  "ate",  "atividade",  "avaliar",  "bastante",
  "c",  "caso",  "combinacoes",  "compactacao",  "comprimir",  "comprova",  "comum",  "comuns",
  "concluir",  "contem",  "conter",  "contextos",  "continua",  "correspondente",  "criado",  "criados",
  "demonstra",  "descompactado",  "deve",  "diferenca",  "direto",  "duas",  "efeito",  "eficiente",
  "ele",  "encontra",  "encontre",  "entender",  "escrever",  "espacos",  "estatisticas",  "Este",
  "estimular",  "estruturas",  "exatamente",  "exibe",  "facil",  "falamos",  "fato",  "faz",
  "fica",  "filho",  "foco",  "foque",  "formato",  "frequentes",  "ganhar",  "ganhos",
  "gravar",  "ideia",  "igual",  "inclui",  "isso",  "juntos",  "leitura",  "ler",
  "linha",  "linhas",  "logica",  "longo",  "maiores",  "mais",  "mas",  "material",
  "maxima",  "menor",  "metodo",  "modo",  "mostrar",  "muitos",  "normal",  "objetivo",
  "observar",  "organizado",  "pai",  "paragrafos",  "pensamos",  "pontuacao",  "porque",  "portugues",
  "possivel",  "possui",  "preparado",  "preservar",  "reduz",  "relatorio",  "repete",  "repetem",
  "repetidas",  "saida",  "seja",  "sejam",  "semelhantes",  "ser",  "sobre",  "solucao",
  "tamanho",  "tambem",  "tantas",  "taxa",  "tecnicos",  "tem",  "termos",  "testes",
  "textos",  "trabalha",  "trabalham",  "trabalhar",  "uso",  "utiliza",  "varios",  "ver",
  
};

#define N ((int)(sizeof(WORD) / sizeof(WORD[0])))

typedef struct {
  const char *word;
  int index;
} Entrada;

static Entrada ordenado[sizeof(WORD) / sizeof(WORD[0])];

static int cmp_entrada(const void *a, const void *b) {
  const Entrada *entrada_a = a;
  const Entrada *entrada_b = b;
  return strcmp(entrada_a->word, entrada_b->word);
}

static int cmp_chave(const void *chave, const void *elem) {
  const char *w = chave;
  const Entrada *e = elem;
  return strcmp(w, e->word);
}

void dic_init(void) {
  for (int i = 0; i < N; i++) {
    ordenado[i].word = WORD[i];
    ordenado[i].index = i;
  }
  qsort(ordenado, N, sizeof(Entrada), cmp_entrada);
}

// compara antes da compilação, se o número for maior, nem compila, pois o dicionário não cabe no formato.
_Static_assert(sizeof(WORD) / sizeof(WORD[0]) <= MAX_PALAVRAS, "MAX_PALAVRAS insuficiente");

int dic_codifica(const char *palavra, unsigned char *out) {
  Entrada *e = bsearch(palavra, ordenado, N, sizeof(Entrada), cmp_chave);
  if(e == NULL) return 0; // palavra não encontrada

  int i = e->index;
  if (i < N_COD1) {
    out[0] = COD1_INI + i;
    return 1;
  } 

  int j = i - N_COD1;
  out[0] = COD2_INI + (j >> 8);
  out[1] = j & 0xFF;
  return 2;
}

const char *dic_decodifica(const unsigned char *p, int *usados){
  int idx;
  if (p[0] >= COD1_INI && p[0] <= COD1_FIM){
    idx = p[0] - COD1_INI;
    *usados = 1;
  } else if (p[0] >= COD2_INI){   /* COD2_FIM e 0xFF: nao precisa testar */
    idx = N_COD1 + ((p[0] - COD2_INI) << 8) + p[1];
    *usados = 2;
  } else {
    return NULL;                  /* ASCII ou ESCAPE: nao e palavra */
  }
  return idx < N ? WORD[idx] : NULL;
}
