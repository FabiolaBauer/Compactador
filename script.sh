#!/bin/sh
# Gera a lista de palavras dos textos, da mais frequente para a menos frequente,
# no formato de array C. Uso: sh script.sh entrada.txt entrada2.txt
cat "$@" | tr -cs 'A-Za-z' '\n' | grep . | sort | uniq -c \
  | sort -k1,1nr -k2,2 \
  | awk '{printf "  \"%s\",%s", $2, (NR%8==0 ? "\n" : "")} END{print ""}'

