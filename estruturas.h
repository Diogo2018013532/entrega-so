#ifndef ESTRUTURAS_H
#define ESTRUTURAS_H

#include "config.h"

/* Definições de funcionamento da central */
typedef struct {
    int  max_estafetas;             /* MAXESTAFETAS ou -m */
    int  prazo_aceitacao;           /* PRAZOACEITA  ou -p (segundos) */
    char bin_dir[TAM_CAMINHO];      /* BINDIR: onde está o tarifador */
    char central_pipe[TAM_PIPE];    /* CENTRAL_PIPE (só usado na fase final) */
} ConfigCentral;

/* Definições de funcionamento do estafeta */
typedef struct {
    char username[TAM_USERNAME];    /* argumento da linha de comandos */
    char central_pipe[TAM_PIPE];    /* CENTRAL_PIPE */
} ConfigEstafeta;

/* Estatísticas desde o arranque da central (comando stats) */
typedef struct {
    int   recebidos;
    int   atribuidos;
    int   concluidos;
    int   expirados;
    int   perdidos;
    float valor_total;              /* soma das entregas concluídas */
} Estatisticas;

#endif
