# Alvo por omissão: escrever só "make" compila a central
all: central

# executável: ficheiros de que depende (se algum mudar, recompila)
central: central.c config.h estruturas.h mensagens.h
	gcc -Wall -Wextra central.c -o central

# apaga o executável e o ficheiro de PID
clean:
	rm -f central central.pid

# ---------------------------------------------------------------
# PROVISÓRIO, SÓ PARA TESTES: APAGAR ESTAS LINHAS ANTES DE ENTREGAR
# Compila o tarifador de exemplo (feito por nós, não é o dos docentes).
# Uso: "make tarifador". Quando os docentes derem o tarifador oficial,
# este alvo deixa de ser preciso. Não está no "all" nem no "clean"
# de propósito, para nunca apagar o tarifador oficial.
tarifador: teste_nao_entregar/tarifador_exemplo.c
	gcc -Wall -Wextra teste_nao_entregar/tarifador_exemplo.c -o tarifador