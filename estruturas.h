
#ifndef ESTRUTURAS_H
#define ESTRUTURAS_H

#include "config.h"

/* Configuração da central */
typedef struct {
    int max_estafetas;
    int prazo_aceitacao;
    char central_pipe[TAM_PIPE];
    char bin_dir[TAM_CAMINHO];
} ConfigCentral;

/* Configuração do estafeta */
typedef struct {
    char username[TAM_USERNAME];
    char central_pipe[TAM_PIPE];
} ConfigEstafeta;

/* Estatísticas gerais da central */
typedef struct {
    int pedidos_recebidos;
    int pedidos_atribuidos;
    int pedidos_concluidos;
    int pedidos_expirados;
    int pedidos_perdidos;

    float valor_total_entregas;
} Estatisticas;

#endif
