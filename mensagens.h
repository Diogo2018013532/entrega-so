#ifndef MENSAGENS_H
#define MENSAGENS_H

#include "config.h"

/* Todas as mensagens começam por char tipo[TAM_TIPO]. Quem lê o pipe lê
   primeiro esse campo e, conforme o valor, sabe o que vem a seguir.
   Todas as estruturas são muito menores que PIPE_BUF (4096 bytes), por
   isso cada write() num pipe é atómico. */

/* Valores do campo tipo */
#define TIPO_PEDIDO     "pedido"      /* gerador  -> central */
#define TIPO_REGISTO    "RegistoEst"  /* estafeta -> central */
#define TIPO_ACEITA     "Aceita"      /* estafeta -> central */
#define TIPO_ENTREGUE   "Entregue"    /* estafeta -> central */
#define TIPO_ESTADO     "Estado"      /* estafeta -> central */
#define TIPO_GANHOS     "Ganhos"      /* estafeta -> central */
#define TIPO_SAI        "Sai"         /* estafeta -> central */
#define TIPO_ANUNCIO    "Anuncio"     /* central  -> estafeta */
#define TIPO_ENCERRA    "Encerra"     /* central  -> estafeta */

/* gerador -> central */
typedef struct {
    char tipo[TAM_TIPO];
    char corpo[TAM_CORPO];            /* "<id> <origem> <destino> <peso>" */
} MsgPedido;

/* estafeta -> central (pelo CENTRAL_PIPE) */
typedef struct {
    char tipo[TAM_TIPO];
    char username[TAM_USERNAME];
    char pipe_para_central[TAM_PIPE];   /* estafeta -> central */
    char pipe_para_estafeta[TAM_PIPE];  /* central -> estafeta */
} MsgRegistoEst;

/* estafeta -> central */
typedef struct {
    char tipo[TAM_TIPO];
    int  id_pedido;
} MsgAceita;

/* estafeta -> central: Entregue, Estado, Ganhos e Sai (só o tipo) */
typedef struct {
    char tipo[TAM_TIPO];
} MsgComando;

/* central -> estafeta */
typedef struct {
    char  tipo[TAM_TIPO];
    int   id;
    char  origem[TAM_LOCAL];
    char  destino[TAM_LOCAL];
    float peso;
    float valor;
    int   segundos_restantes;
} MsgAnuncio;

/* central -> estafeta */
typedef struct {
    char tipo[TAM_TIPO];
} MsgEncerra;

#endif
