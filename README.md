# Compactador Paralelo com Processos e Threads (Grau B - SO)

Compactador/descompactador de texto em C (Linux), baseado no `mini_zip.c`
fornecido. O programa troca palavras por códigos de 1 ou 2 bytes (dicionário)
e divide o serviço entre **processos** e **threads**.

## Estado atual

| Parte | Situação |
|---|---|
| Compactador (3 threads + filas) | pronto e testado |
| Memória compartilhada (`Stats`) | pronta |
| Tratamento de erros (compactador e `main`) | pronto |
| Dicionário ampliado (184 palavras) + escape | pronto e testado |
| Monitor | **provisório** (só imprime contadores) |
| Descompactador | **versão mínima sequencial**, sem processo próprio |

## Estrutura dos arquivos

| Arquivo | Conteúdo |
|---|---|
| `include/common.h` | `Block`, `Stats`, constantes (`BLK`, `BLK_SAIDA`, `FOLGA`, `QSIZE`, `SHM_NAME`). Compartilhado por todos os módulos. |
| `include/formato.h` | Formato do arquivo compactado: o que cada faixa de byte significa. Compactador e Descompactador precisam seguir o mesmo. |
| `include/dicionario.h`, `src/dicionario.c` | Lista de palavras, `dic_init`, `dic_codifica` (busca binária) e `dic_decodifica`. |
| `include/stats.h`, `src/stats.c` | Cria e remove o segmento de memória compartilhada com o `Stats`. |
| `include/fila.h`, `src/fila.c` | Fila circular limitada (produtor/consumidor) de blocos. |
| `include/compactador.h`, `src/compactador.c` | Processo Compactador: leitura de bloco, compressão e as 3 threads. Função pública: `compactador_run()`. |
| `include/descompactador.h`, `src/descompactador.c` | Função `decompress()` (versão sequencial). |
| `include/monitor.h`, `src/monitor.c` | Função `monitor_loop()` (versão provisória). |
| `src/main.c` | Lê os argumentos, abre os arquivos, faz o `fork()` e chama os módulos. |
| `script.sh` | Gera a lista de palavras do dicionário a partir dos textos. |
| `Makefile` | Compilação (`make`). |

## Compilar

No Linux, na pasta do projeto:

```bash
make            # gera o executavel ./mz
make clean      # apaga os .o e o ./mz
```

Para recompilar do zero: `make clean && make`.

## Executar

```bash
./mz -c <texto> <compactado>      # compactar   (Compactador + Monitor)
./mz -d <compactado> <texto>      # descompactar
```

Exemplo:

```bash
./mz -c entrada.txt c1.mzp
./mz -d c1.mzp saida1.txt
```

- O Monitor escreve no `stdout`. O log das threads (`[leitura]`,
  `[compacta]`, `[grava]`) sai no `stderr`.
- Para ver só o Monitor: `./mz -c entrada.txt c1.mzp 2>/dev/null`
- Para guardar o log das threads: `./mz -c entrada.txt c1.mzp 2> log_threads.txt`
- Código de saída: `0` se deu tudo certo, `1` em caso de erro
  (`echo $?` mostra).

### Demonstrar o paralelismo (`MZ_ATRASO`)

Os textos são pequenos e a compactação termina em menos de 1 ms. Para ver as
threads trabalhando ao mesmo tempo e o Monitor acompanhando o andamento, defina
um atraso por bloco, em microssegundos:

```bash
MZ_ATRASO=200000 ./mz -c entrada.txt c1.mzp      # 200 ms por bloco
```

As linhas `[leitura]`, `[compacta]` e `[grava]` aparecem intercaladas: enquanto
um bloco é gravado, o seguinte já está sendo compactado e outro já está sendo
lido. Com 100 ms de atraso, o `entrada2.txt` (5 blocos × 3 etapas) levaria
1,5 s se as etapas fossem em sequência, e leva cerca de 0,75 s.

Sem a variável, não há atraso.

## Testar

### Os dois cenários do enunciado

```bash
make clean && make

./mz -c entrada.txt c1.mzp 2>/dev/null
./mz -d c1.mzp saida1.txt
diff entrada.txt saida1.txt && echo "entrada.txt: OK"

./mz -c entrada2.txt c2.mzp 2>/dev/null
./mz -d c2.mzp saida2.txt
diff entrada2.txt saida2.txt && echo "entrada2.txt: OK"
```

Está certo se aparecerem os dois `OK` sem nenhuma diferença listada pelo
`diff`, e se o Monitor mostrar `nao=0` nos dois textos.

### Mostrar os três conteúdos (pedido no enunciado)

```bash
cat entrada.txt                  # original
xxd c1.mzp | head -20            # compactado (binario; ou: hexdump -C c1.mzp)
cat saida1.txt                   # descompactado
ls -l entrada.txt c1.mzp         # tamanhos
```

### Resultados esperados

| Texto | Palavras lidas | Não encontradas | Original | Compactado | Compressão |
|---|---|---|---|---|---|
| `entrada.txt` | 315 | 0 | 1938 B | 755 B | 61,0% |
| `entrada2.txt` | 139 | 0 | 886 B | 303 B | 65,8% |

Com o dicionário original de 39 palavras, os mesmos textos davam:

| Texto | Não encontradas | Compressão |
|---|---|---|
| `entrada.txt` | 170 | 21,8% |
| `entrada2.txt` | 43 | 49,5% |

### Testes de erro

```bash
./mz -c entrada.txt /dev/full; echo "saida=$?"   # disco cheio: mostra o erro, saida=1
./mz -x entrada.txt lixo.txt;  echo "saida=$?"   # modo invalido: saida=1, nao cria lixo.txt
./mz -c naoexiste.txt c.mzp;   echo "saida=$?"   # arquivo inexistente: saida=1
```

### Texto com acentos e símbolos

Qualquer texto deve voltar idêntico, mesmo com palavras fora do dicionário:

```bash
printf 'O programa não é ruim: custa R$ 10,00 - será? Sim! @#%%\n' > especial.txt
./mz -c especial.txt ce.mzp 2>/dev/null && ./mz -d ce.mzp saida_e.txt
cmp especial.txt saida_e.txt && echo "especial: OK"
```

## Como funciona

### Processos

```
main()
 ├─ dic_init()            monta o indice do dicionario (antes de tudo)
 ├─ stats_create()        cria a memoria compartilhada (antes do fork)
 ├─ fork() ─► processo Monitor        le Stats e imprime
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
- Quando as threads terminam, o pai marca `terminou = 1`, o Monitor imprime o
  resumo e sai, e o pai faz `waitpid` e remove o segmento (`shm_unlink`).

### Threads do Compactador

| Thread | O que faz |
|---|---|
| Leitura | Lê o arquivo em blocos (`BLK`), sem cortar palavra no meio, e coloca em `q1`. |
| Compactação | Tira de `q1`, troca palavras do dicionário por códigos, coloca em `q2`. Não mexe em arquivo. |
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
- **Dicionário:** `dic_init()` monta o índice de busca antes do `fork` e das
  threads. Depois disso ele só é lido, então não precisa de mutex.

### Corte de blocos

Se um bloco terminasse no meio de uma palavra (`progr|ama`), o dicionário não
reconheceria nenhum dos pedaços. Por isso a leitura lê `BLK - FOLGA` bytes e,
se o último for uma letra, continua lendo até o próximo separador. `FOLGA` é o
espaço reservado para completar a palavra (maior palavra esperada).

### Tratamento de erros

- Erro de leitura (`ferror`) ou de gravação (`fwrite`, `fflush`) marca uma flag
  da thread. A thread **continua passando a sentinela** (ou, na gravação,
  continua esvaziando a fila), para as outras não travarem.
- As threads são criadas de trás para frente (gravação, compactação, leitura).
  Se uma criação falhar, as que já estão rodando recebem uma sentinela e
  terminam.
- `compactador_run` sempre marca `terminou = 1`, mesmo com erro, para o Monitor
  não ficar esperando. Ela retorna `false` se houve erro, e o `main` sai com
  código 1 depois de limpar a memória compartilhada.

## Dicionário ampliado

O dicionário tem as **184 palavras distintas** dos dois textos de teste
(incluindo `O`, `Este` e `Assim`, que são entradas separadas das minúsculas).
A lista é gerada pelo `script.sh` e colada em `src/dicionario.c`:

```bash
sh script.sh entrada.txt entrada2.txt
```

As palavras ficam **ordenadas por frequência**: só 111 cabem em código de
1 byte, e as que mais aparecem ficam com esses códigos. Palavras de 1 letra
precisam estar entre as 111 primeiras, senão o código ficaria maior que a
palavra.

A busca usa um índice ordenado alfabeticamente (`qsort` no `dic_init`) e
`bsearch`. A lista original continua na ordem de frequência, porque a posição
de cada palavra é o seu código.

### Formato do arquivo compactado (`formato.h`)

| Byte | Significado |
|---|---|
| `0x00`–`0x7F` | texto ASCII copiado como está |
| `0x80`–`0xEE` | palavra com código de 1 byte: índice `byte - 0x80` (111 palavras) |
| `0xEF` X | **escape**: X é um byte do texto original (≥ `0x80`) |
| `0xF0`–`0xFF` Y | palavra com código de 2 bytes: índice `111 + (byte - 0xF0) * 256 + Y` (4096 palavras) |

- Os códigos só usam bytes ≥ `0x80`, então nunca se confundem com espaço,
  pontuação, dígitos ou palavras fora do dicionário, que ficam em ASCII.
- O **escape** cobre o caso contrário: um byte do texto ≥ `0x80` (por exemplo,
  um acento em UTF-8) é gravado com `0xEF` na frente, para o descompactador não
  o confundir com um código. Por isso o bloco de saída tem `BLK_SAIDA = 2 * BLK`.
- Capacidade total: 4207 palavras. Um `_Static_assert` em `dicionario.c` impede
  a compilação se o dicionário passar disso.

## Regras do código

Os três módulos dependem de:

1. **`Stats` (em `common.h`)**: mesmos campos, na mesma ordem. Se alguém
   acrescentar um campo, avise os outros.
2. **`SHM_NAME`**: nome do segmento compartilhado (`/mz_stats`).
3. **`Block` (em `common.h`)**: `data` tem `BLK_SAIDA` bytes.
4. **`formato.h`**: as faixas de byte. Não mudar sem avisar.
5. **Dicionário (`dicionario.c`)**: o Descompactador usa exatamente o mesmo
   dicionário do Compactador. `dic_init()` já é chamado no `main`.
6. **`MZ_ATRASO`**: mesma variável de ambiente para o atraso de demonstração,
   se o Descompactador também usar threads.

**Monitor**: basta reescrever `monitor.c` (função `monitor_loop(Stats *)`). Ele
lê os contadores sob `st->lock` até `st->terminou == 1`.

**Descompactador**: basta evoluir `descompactador.c`. Atenção:

- no modo `-d` o `main.c` ainda não cria o Monitor nem usa processos separados;
  o enunciado pede Descompactador + Monitor nesse modo;
- é preciso combinar com o Monitor o que cada campo do `Stats` significa no
  modo `-d`;
- se ler em blocos, o bloco não pode terminar logo depois de um `0xEF` ou de um
  byte `0xF0`–`0xFF`, porque falta o byte seguinte do código.

## Limitações conhecidas

- **Acentos:** `isalpha` só reconhece letras ASCII. Uma palavra como `não` é
  quebrada em `n` + `ã` + `o`. Ela **volta igual** (o `ã` passa pelo escape), mas
  não é compactada.
- **Maiúsculas:** o dicionário diferencia maiúsculas de minúsculas. `Programa`
  ou `PROGRAMA` não são encontradas (voltam iguais, mas sem compactar).
- O dicionário é fixo: palavras de outros textos que não estão na lista não são
  compactadas.

## Pendências

- [ ] Monitor definitivo (palavras lidas, compactadas, não encontradas,
      tamanho original e compactado, percentual).
- [ ] Descompactador como processo próprio, com o Monitor no modo `-d`.
- [ ] Preparar a apresentação (até 20 minutos): papel de cada processo e
      thread, como trocam informação, dificuldades e demonstração (incluindo a
      comparação do dicionário antigo com o ampliado).
