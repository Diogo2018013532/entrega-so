#ifndef CONFIG_H
#define CONFIG_H

/* ---- Limites fixos do enunciado ---- */
#define LIMITE_ESTAFETAS     20
#define LIMITE_PEDIDOS       30
#define VALOR_MAXIMO         100000   /* maior valor aceite em -p, -m, prazo */

/* ---- Valores por omissão: central ---- */
#define MAXESTAFETAS_OMISSAO 10
#define PRAZOACEITA_OMISSAO  30
#define BINDIR_OMISSAO       "."

/* ---- Valores por omissão: gerador ---- */
#define INTMIN_OMISSAO       2
#define INTMAX_OMISSAO       10

/* ---- Nomes das variáveis de ambiente ---- */
#define ENV_MAXESTAFETAS     "MAXESTAFETAS"
#define ENV_PRAZOACEITA      "PRAZOACEITA"
#define ENV_BINDIR           "BINDIR"
#define ENV_CENTRAL_PIPE     "CENTRAL_PIPE"
#define ENV_INTMIN           "INTMIN"
#define ENV_INTMAX           "INTMAX"

/* ---- Tamanhos (incluem o '\0') ---- */
#define TAM_TIPO             12   /* campo inicial de todas as mensagens */
#define TAM_CORPO            50   /* corpo da mensagem do gerador */
#define TAM_LOCAL            16   /* origem/destino: ate 15 caracteres */
#define TAM_USERNAME         32
#define TAM_PIPE             128  /* nome de um named pipe */
#define TAM_CAMINHO          256

/* ---- Outros ---- */
#define NOME_TARIFADOR       "tarifador"
#define FICHEIRO_PID         "central.pid"   /* instância única */

#endif
