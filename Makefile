CC     = gcc
CFLAGS = -Wall -Wextra -pthread
LDLIBS = -lrt
OBJS   = main.o stats.o dicionario.o fila.o compactador.o descompactador.o monitor.o

mz: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

# dependencias dos headers
main.o:          common.h stats.h compactador.h descompactador.h monitor.h
stats.o:         common.h stats.h
dicionario.o:    dicionario.h
fila.o:          common.h fila.h
compactador.o:   common.h compactador.h dicionario.h fila.h
descompactador.o: descompactador.h dicionario.h
monitor.o:       common.h monitor.h

clean:
	rm -f $(OBJS) mz

.PHONY: clean
