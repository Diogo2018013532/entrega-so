
#ifndef CONFIG_H
#define CONFIG_H

/* Limites definidos no enunciado */
#define MAX_ESTAFETAS 20
#define MAX_PEDIDOS 30

/* Valores por omissão da central */
#define MAX_ESTAFETAS_OMISSAO 10
#define PRAZO_ACEITA_OMISSAO 30
#define BINDIR_OMISSAO "."

/* Valores por omissão do gerador */
#define INT_MIN_PEDIDOS 2
#define INT_MAX_PEDIDOS 10

/* Nomes das variáveis de ambiente */
#define ENV_CENTRAL_PIPE "CENTRAL_PIPE"
#define ENV_MAX_ESTAFETAS "MAXESTAFETAS"
#define ENV_PRAZO_ACEITA "PRAZOACEITA"
#define ENV_BINDIR "BINDIR"
#define ENV_INTMIN "INTMIN"
#define ENV_INTMAX "INTMAX"

/* Dimensões das estruturas e mensagens */
#define TAM_TIPO 12
#define TAM_USERNAME 32
#define TAM_LOCAL 16
#define TAM_PIPE 128
#define TAM_CORPO 50
#define TAM_CAMINHO 256

/* Outros */
#define NOME_TARIFADOR "tarifador"
#define FICHEIRO_PID "central.pid"

#endif
