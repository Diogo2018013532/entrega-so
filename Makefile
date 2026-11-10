
all: central

central: central.c config.h estruturas.h mensagens.h tarifador.c
	gcc central.c -o central
	gcc tarifador.c -o tarifador
clean:
	rm -f central central.pid
	rm -f tarifador tarifador.pid
