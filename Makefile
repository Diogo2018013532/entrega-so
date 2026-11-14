
all: central

central: central.c config.h estruturas.h mensagens.h
	gcc central.c -o central
	
clean:
	rm -f central central.pid
	
