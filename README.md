# Compactador Paralelo com Processos e Threads (Grau B - SO)

Compactador/descompactador de texto em C (Linux), baseado no `mini_zip.c`
fornecido. O trabalho troca palavras frequentes por um símbolo de 1 byte
(dicionário) e divide o serviço entre **processos** e **threads**.

## Estado atual

| Parte | Situação | Responsável |
|---|---|---|
| Compactador (3 threads + filas) | pronto e testado | Fabíola |
| Memória compartilhada (`Stats`) | pronta | Fabíola |
| Monitor | **provisório** (só imprime contadores)
| Descompactador | **sequencial**, igual ao `mini_zip.c` original |
| Dicionário ampliado (desafio) | **não feito** | a combinar |

## Estrutura dos arquivos

| Arquivo | Conteúdo |
|---|---|
| `common.h` | `Block`, `Stats`, constantes (`BLK`, `FOLGA`, `QSIZE`, `SHM_NAME`). É o contrato entre os módulos. |
| `stats.c/.h` | Cria e remove o segmento de memória compartilhada com o `Stats`. |
| `dicionario.c/.h` | Dicionário palavra ↔ símbolo (`word_to_sym`, `sym_to_word`). Compactador e Descompactador usam o mesmo. |
| `fila.c/.h` | Fila circular limitada (produtor/consumidor) de blocos. |
| `compactador.c/.h` | Processo Compactador: leitura de bloco, compressão e as 3 threads. Função pública: `compactador_run()`. |
| `descompactador.c/.h` | Função `decompress()` (versão sequencial). |
| `monitor.c/.h` | Função `monitor_loop()` (versão provisória). |
| `main.c` | Lê os argumentos, abre os arquivos, faz o `fork()` e chama os módulos. |
| `Makefile` | Atalho para compilar (`make`). |

## Como compilar

`make` (e `make clean` para limpar)

## Como usar

```bash
./mz -c entrada.txt compactado.mzp
./mz -d compactado.mzp saida.txt
diff entrada.txt saida.txt && echo OK
```

Na compactação, o log das threads sai no `stderr` e o do Monitor no `stdout`:

```bash
./mz -c entrada.txt c1.mzp 2> log_threads.txt
```

## Como funciona

### Processos

```
main()
 ├─ stats_create()        cria a memória compartilhada (antes do fork)
 ├─ fork() ─► processo Monitor        lê Stats e imprime
 └─ processo Compactador (o pai)
      ├─ thread leitura       ─► fila q1 ─►
      ├─ thread compactação   ─► fila q2 ─►
      └─ thread gravação
```

- O **Monitor** é um processo filho criado com `fork()`.
- A comunicação entre os processos é feita por **memória compartilhada**
  (`shm_open` + `mmap`, segmento `SHM_NAME`), com a struct `Stats`.
- O mutex do `Stats` usa `PTHREAD_PROCESS_SHARED`, senão não funciona entre
  processos.
- Quando as threads terminam, o pai marca `terminou = 1`; o Monitor imprime o
  resumo e sai; o pai faz `waitpid` e remove o segmento (`shm_unlink`).

### Threads do Compactador

| Thread | O que faz |
|---|---|
| Leitura | Lê o arquivo em blocos (`BLK`), sem cortar palavra no meio, e coloca em `q1`. |
| Compactação | Tira de `q1`, troca palavras do dicionário por símbolos, coloca em `q2`. Não toca em arquivo. |
| Gravação | Tira de `q2` e grava no arquivo de saída. |

### Sincronização

- Cada fila é um **buffer circular de tamanho fixo** (`QSIZE`) com **1 mutex e
  2 variáveis de condição** (`not_full`, `not_empty`): é o padrão
  produtor/consumidor. Se a fila enche, o produtor espera; se esvazia, o
  consumidor espera.
- Cada fila tem um produtor e um consumidor, então os blocos saem na mesma
  ordem em que entraram e o arquivo compactado fica na ordem certa.
- **Fim do arquivo:** um bloco com `len = 0` (sentinela) é passado adiante pelas
  três threads.
- **Estatísticas:** cada bloco soma seus contadores locais ao `Stats`
  compartilhado **uma vez**, sob o mutex, para não travar a cada palavra.

### Corte de blocos

Se um bloco terminasse no meio de uma palavra (`progr|ama`), o dicionário não
reconheceria nenhum dos pedaços. Por isso a leitura lê `BLK - FOLGA` bytes e,
se o último for uma letra, continua lendo até o próximo separador. `FOLGA` é o
espaço reservado para completar a palavra (maior palavra esperada).

## Contrato entre o grupo

Os três módulos dependem de:

1. **`Stats` (em `common.h`)**: mesmos campos, na mesma ordem. Se alguém
   acrescentar um campo, avise os outros.
2. **`SHM_NAME`**: nome do segmento compartilhado (`/mz_stats`).
3. **Dicionário (`dicionario.c`)**: o Descompactador precisa usar exatamente o
   mesmo dicionário do Compactador.

**Monitor**: basta reescrever `monitor.c` (função
`monitor_loop(Stats *)`). Ele lê os contadores sob `st->lock` até
`st->terminou == 1`.

**Descompactador**: basta evoluir `descompactador.c`.
Atenção: no modo `-d` o `main.c` ainda não cria o Monitor nem usa processos
separados. O enunciado pede Descompactador + Monitor nesse modo.

## Limitações conhecidas

- **Acentos:** `isalpha` só reconhece letras ASCII. Letras como `ç`, `ã`, `é`
  contam como separador e quebram a palavra.
- **Símbolos do dicionário no texto:** se o texto original tiver um caractere
  que também é símbolo do dicionário (por exemplo `-`, `?`, `/`, dígitos), o
  descompactador o troca por uma palavra e o texto não volta igual. O
  Compactador avisa no `stderr` quando isso acontece.
- **Dicionário pequeno:** são 39 palavras. Muitas palavras dos textos de teste
  ficam de fora (nos testes, mais da metade).
- Os `fprintf` das threads (`[leitura]`, `[compacta]`, `[grava]`) servem para
  mostrar o paralelismo na apresentação; podem ser removidos.

## Pendências

- [ ] Dicionário ampliado cobrindo todas as palavras dos dois textos de teste
      (desafio do enunciado). Sugestão: símbolos em bytes `0x80`–`0xFF` ou
      código de 2 bytes, definido em `dicionario.c` e usado por todos.
- [ ] Monitor definitivo (palavras lidas, compactadas, não encontradas,
      tamanho original e compactado, percentual).
- [ ] Descompactador como processo próprio, com o Monitor no modo `-d`.
- [ ] Testar com os dois arquivos fornecidos e conferir com `diff`.
- [ ] Preparar a apresentação (até 20 minutos): papel de cada processo e
      thread, como trocam informação, dificuldades e demonstração.
