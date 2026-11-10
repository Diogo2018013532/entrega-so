
#ifndef MENSAGENS_H
#define MENSAGENS_H

#include <sys/types.h>
#include "config.h"

/* Identificadores dos tipos de mensagem */
#define TIPO_PEDIDO "pedido"
#define TIPO_REGISTO "RegistoEst"
#define TIPO_ACEITA "Aceita"
#define TIPO_ENTREGUE "Entregue"
#define TIPO_ESTADO "Estado"
#define TIPO_GANHOS "Ganhos"
#define TIPO_SAI "Sai"
#define TIPO_ANUNCIO "Anuncio"
#define TIPO_RESPOSTA "Resposta"
#define TIPO_ENCERRA "Encerra"

/* Mensagem enviada pelo gerador à central */
typedef struct {
    char tipo[TAM_TIPO];
    char corpo[TAM_CORPO];
} MsgPedido;

/* Registo inicial de um estafeta */
typedef struct {
    char tipo[TAM_TIPO];
    char username[TAM_USERNAME];
    pid_t pid;
    char pipe_para_central[TAM_PIPE];
    char pipe_para_estafeta[TAM_PIPE];
} MsgRegistoEst;

/* Tentativa de aceitar um pedido */
typedef struct {
    char tipo[TAM_TIPO];
    int id_pedido;
} MsgAceita;

/* Comando entregue */
typedef struct {
    char tipo[TAM_TIPO];
} MsgEntregue;

/* Consulta do estado */
typedef struct {
    char tipo[TAM_TIPO];
} MsgEstado;

/* Consulta dos ganhos */
typedef struct {
    char tipo[TAM_TIPO];
} MsgGanhos;

/* Saída do estafeta */
typedef struct {
    char tipo[TAM_TIPO];
} MsgSai;

/* Anúncio enviado pela central ao estafeta */
typedef struct {
    char tipo[TAM_TIPO];
    int id;
    char origem[TAM_LOCAL];
    char destino[TAM_LOCAL];
    float peso;
    float valor;
    int segundos_restantes;
} MsgAnuncio;

/* Resposta enviada pela central */
typedef struct {
    char tipo[TAM_TIPO];
    int sucesso;
    char mensagem[100];
} MsgResposta;

/* Encerramento enviado pela central */
typedef struct {
    char tipo[TAM_TIPO];
} MsgEncerra;

#endif
