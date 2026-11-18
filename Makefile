CC     = gcc
CFLAGS = -Wall -Wextra -pthread -Iinclude
LDLIBS = -lrt
OBJS   = main.o stats.o dicionario.o fila.o compactador.o descompactador.o monitor.o

# fontes em src/, headers em include/
vpath %.c src
vpath %.h include

mz: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

# dependencias dos headers
main.o:          common.h stats.h compactador.h descompactador.h monitor.h
stats.o:         common.h stats.h
dicionario.o:    dicionario.h formato.h
fila.o:          common.h fila.h
compactador.o:   common.h compactador.h dicionario.h fila.h formato.h
descompactador.o: descompactador.h dicionario.h formato.h
monitor.o:       common.h monitor.h

clean:
	rm -f $(OBJS) mz

.PHONY: clean
