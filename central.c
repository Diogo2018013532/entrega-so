/* central.c - Passo 1: consola com os comandos stats e encerra */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      /* pipe, fork, dup2, execl, close, access, unlink */
#include <fcntl.h>       /* open */
#include <signal.h>      /* sigaction */
#include <sys/types.h>   /* pid_t */
#include <sys/wait.h>    /* waitpid */
#include <errno.h>

#include "config.h"
#include "estruturas.h"

/* Dados para falar com o tarifador */
typedef struct {
    pid_t pid;        /* número do processo tarifador */
    int   fd_escrita; /* ponta por onde a central escreve (vai para o stdin dele) */
    int   fd_leitura; /* ponta por onde a central lê (vem do stdout dele) */
} Tarifador;

/* Flag que o handler do SIGINT põe a 1. É volatile sig_atomic_t porque
   é alterada dentro de um handler e lida no programa principal */
static volatile sig_atomic_t sinal_recebido = 0;

/* Handler do SIGINT (Ctrl+C): só levanta a flag. Nada de printf aqui,
   porque não é seguro dentro de um handler */
static void tratar_sigint(int sinal) {
    (void) sinal;
    sinal_recebido = 1;
}

/* Handler do SIGPIPE: não faz nada. Serve só para o sinal deixar de matar
   a central: o write devolve erro (EPIPE) e tratamo-lo no código. */
static void tratar_sigpipe(int sinal) {
    (void) sinal;
}

/* SIGINT -> handler. Sem SA_RESTART (flags = 0), por isso o sinal
   interrompe o fgets e ele devolve NULL.
   SIGPIPE -> handler vazio: se o tarifador morrer, o write devolve erro
   em vez de matar a central. */
static int configurar_sinais(void) {
    struct sigaction acao;

    memset(&acao, 0, sizeof(acao));
    sigemptyset(&acao.sa_mask);

    acao.sa_handler = tratar_sigint;
    if (sigaction(SIGINT, &acao, NULL) == -1) {
        perror("sigaction(SIGINT)");
        return -1;
    }

    acao.sa_handler = tratar_sigpipe;
    if (sigaction(SIGPIPE, &acao, NULL) == -1) {
        perror("sigaction(SIGPIPE)");
        return -1;
    }
    return 0;
}

/* Mostra as estatísticas (const: só lê, não altera) */
static void mostrar_stats(const Estatisticas *s) {
    printf("Pedidos recebidos:  %d\n", s->recebidos);
    printf("Pedidos atribuídos: %d\n", s->atribuidos);
    printf("Pedidos concluídos: %d\n", s->concluidos);
    printf("Pedidos expirados:  %d\n", s->expirados);
    printf("Pedidos perdidos:   %d\n", s->perdidos);
    printf("Valor total das entregas concluídas: %.2f euros\n", s->valor_total);
}

/* Converte texto num inteiro positivo. Devolve 0 se está bem, -1 se não */
static int ler_inteiro(const char *texto, int *resultado) {
    long valor;      /* long: cabe mais do que um int */
    char lixo;

    /* "%ld %c": lê um inteiro e tenta ler mais um caracter.
       Se leu 2 coisas, havia lixo a seguir ao número (ex: "12abc") */
    if (sscanf(texto, "%ld %c", &valor, &lixo) != 1)
        return -1;

    if (valor < 1 || valor > VALOR_MAXIMO)
        return -1;

    *resultado = (int) valor;
    return 0;
}

/* Preenche a configuração: omissão -> variável de ambiente -> argumentos.
   Devolve 0 se está bem, -1 se algum valor é inválido */
static int carregar_config(ConfigCentral *cfg, int argc, char *argv[]) {
    char *valor;
    int   i;

    /* 1.º valores por omissão */
    memset(cfg, 0, sizeof(*cfg));
    cfg->max_estafetas   = MAXESTAFETAS_OMISSAO;
    cfg->prazo_aceitacao = PRAZOACEITA_OMISSAO;
    snprintf(cfg->bin_dir, sizeof(cfg->bin_dir), "%s", BINDIR_OMISSAO);

    /* 2.º variáveis de ambiente (getenv devolve NULL se não existir) */
    valor = getenv(ENV_MAXESTAFETAS);
    if (valor != NULL && ler_inteiro(valor, &cfg->max_estafetas) == -1) {
        printf("Erro: %s inválido (%s)\n", ENV_MAXESTAFETAS, valor);
        return -1;
    }

    valor = getenv(ENV_PRAZOACEITA);
    if (valor != NULL && ler_inteiro(valor, &cfg->prazo_aceitacao) == -1) {
        printf("Erro: %s inválido (%s)\n", ENV_PRAZOACEITA, valor);
        return -1;
    }

    valor = getenv(ENV_BINDIR);
    if (valor != NULL) {
        if (strlen(valor) >= sizeof(cfg->bin_dir)) {
            printf("Erro: %s demasiado longo\n", ENV_BINDIR);
            return -1;
        }
        strcpy(cfg->bin_dir, valor);
    }

    valor = getenv(ENV_CENTRAL_PIPE);    /* ainda não é usado, fica guardado */
    if (valor != NULL && strlen(valor) < sizeof(cfg->central_pipe))
        strcpy(cfg->central_pipe, valor);

    /* 3.º argumentos: vêm aos pares (-p 10, -m 5) e substituem o ambiente */
    for (i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            printf("Erro: falta o valor de %s\n", argv[i]);
            return -1;
        }

        if (strcmp(argv[i], "-p") == 0) {
            if (ler_inteiro(argv[i + 1], &cfg->prazo_aceitacao) == -1) {
                printf("Erro: valor inválido para -p (%s)\n", argv[i + 1]);
                return -1;
            }
        }
        else if (strcmp(argv[i], "-m") == 0) {
            if (ler_inteiro(argv[i + 1], &cfg->max_estafetas) == -1) {
                printf("Erro: valor inválido para -m (%s)\n", argv[i + 1]);
                return -1;
            }
        }
        else {
            printf("Erro: argumento desconhecido (%s)\n", argv[i]);
            return -1;
        }
    }

    /* o enunciado garante no máximo 20 estafetas */
    if (cfg->max_estafetas > LIMITE_ESTAFETAS) {
        printf("Erro: máximo de estafetas é %d\n", LIMITE_ESTAFETAS);
        return -1;
    }

    return 0;
}

/* Lança o tarifador com dois pipes:
     p_ent: central escreve em p_ent[1]  ->  tarifador lê no stdin
     p_sai: tarifador escreve no stdout  ->  central lê em p_sai[0]
   Devolve 0 se está bem, -1 se falhou */
static int iniciar_tarifador(Tarifador *t, const char *bin_dir) {
    char caminho[TAM_CAMINHO];
    int  p_ent[2];
    int  p_sai[2];

    /* caminho completo: BINDIR/tarifador */
    if (snprintf(caminho, sizeof(caminho), "%s/%s", bin_dir, NOME_TARIFADOR)
            >= (int) sizeof(caminho)) {
        printf("Erro: caminho do tarifador demasiado longo\n");
        return -1;
    }

    /* o ficheiro existe e pode ser executado? */
    if (access(caminho, X_OK) == -1) {
        perror(caminho);
        return -1;
    }

    if (pipe(p_ent) == -1) {
        perror("pipe");
        return -1;
    }
    if (pipe(p_sai) == -1) {
        perror("pipe");
        close(p_ent[0]);
        close(p_ent[1]);
        return -1;
    }

    fflush(stdout);             /* evita texto repetido no filho */
    t->pid = fork();            /* a partir daqui há 2 processos */
    if (t->pid == -1) {
        perror("fork");
        close(p_ent[0]); close(p_ent[1]);
        close(p_sai[0]); close(p_sai[1]);
        return -1;
    }

    if (t->pid == 0) {
        /* ---- FILHO (vai passar a ser o tarifador) ---- */
        /* stdin passa a ser a ponta de leitura do 1.º pipe,
           stdout passa a ser a ponta de escrita do 2.º pipe */
        dup2(p_ent[0], STDIN_FILENO);
        dup2(p_sai[1], STDOUT_FILENO);

        /* já não precisamos das pontas originais */
        close(p_ent[0]); close(p_ent[1]);
        close(p_sai[0]); close(p_sai[1]);

        execl(caminho, NOME_TARIFADOR, (char *) NULL);

        /* se chegou aqui o execl falhou */
        perror("execl");
        exit(1);
    }

    /* ---- PAI (central) ---- */

    /* fecha as pontas que são do filho */
    close(p_ent[0]);
    close(p_sai[1]);

    t->fd_escrita = p_ent[1];
    t->fd_leitura = p_sai[0];
    return 0;
}

/* Termina o tarifador: fechar o stdin dele faz com que acabe sozinho,
   e o waitpid espera por ele (senão ficava zombie) */
static void terminar_tarifador(Tarifador *t) {
    close(t->fd_escrita);
    close(t->fd_leitura);

    while (waitpid(t->pid, NULL, 0) == -1) {
        if (errno != EINTR) {       /* EINTR: interrompido por um sinal, repete */
            perror("waitpid");
            break;
        }
    }
    printf("Tarifador terminado.\n");
}

/* Lê uma linha (até ao '\n') do pipe, um byte de cada vez, para não
   ler mais do que a linha. Devolve 0 se está bem, -1 se falhou */
static int ler_linha(int fd, char *buffer, size_t tamanho) {
    size_t  i = 0;
    ssize_t n;
    char    c;

    while (i < tamanho - 1) {
        n = read(fd, &c, 1);
        if (n == -1 && errno == EINTR)   /* interrompido por um sinal: repete */
            continue;
        if (n != 1)                      /* erro ou fim de ficheiro */
            return -1;
        if (c == '\n') {
            buffer[i] = '\0';
            return 0;
        }
        buffer[i++] = c;
    }
    return -1;                           /* linha demasiado grande */
}

/* Pergunta o preço ao tarifador: escreve "origem destino peso" e lê
   "distancia valor". Devolve 0 se está bem, -1 se falhou */
static int tarifar_pedido(const Tarifador *t, const char *origem,
                          const char *destino, const char *peso,
                          float *distancia, float *valor) {
    char pergunta[100];
    char resposta[100];
    int  n;

    n = snprintf(pergunta, sizeof(pergunta), "%s %s %s\n",
                 origem, destino, peso);

    if (write(t->fd_escrita, pergunta, n) != n) {
        perror("Erro ao escrever ao tarifador");
        return -1;
    }

    if (ler_linha(t->fd_leitura, resposta, sizeof(resposta)) == -1) {
        printf("Erro: o tarifador não respondeu\n");
        return -1;
    }

    if (sscanf(resposta, "%f %f", distancia, valor) != 2) {
        printf("Erro: resposta inesperada do tarifador (%s)\n", resposta);
        return -1;
    }
    return 0;
}

/* Instância única: a central cria o ficheiro FICHEIRO_PID com o seu PID.
   Se o ficheiro já existe, assume que já há uma central em execução.
   Devolve 0 se pode arrancar, -1 se já há outra central (ou erro) */
static int verificar_instancia_unica(void) {
    char    texto[32];
    int     fd;
    ssize_t n;

    /* O_EXCL: só cria se o ficheiro NÃO existir (falha se já existir) */
    fd = open(FICHEIRO_PID, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd == -1) {
        printf("Erro: já existe uma central em execução\n");
        return -1;
    }

    /* escreve o nosso PID no ficheiro */
    n = snprintf(texto, sizeof(texto), "%d\n", (int) getpid());
    if (write(fd, texto, n) != n) {
        perror(FICHEIRO_PID);
        close(fd);
        unlink(FICHEIRO_PID);
        return -1;
    }
    close(fd);
    return 0;
}

int main(int argc, char *argv[]) {

    /* 1) Variáveis */
    ConfigCentral config;
    Tarifador tarifador;
    Estatisticas stats;
    char linha[200];     /* linha escrita pelo administrador */
    char p[5][64];       /* as palavras da linha: p[0] é o comando */
    int  n;              /* quantas palavras foram lidas */
    int  terminar = 0;   /* 1 = sair do ciclo */

    /* 2) Sinais */
    if (configurar_sinais() == -1)
        return 1;

    /* 3) Instância única (primeiro de tudo). Se falhar, NÃO se apaga
          o ficheiro, porque pode ser da outra central */
    if (verificar_instancia_unica() == -1)
        return 1;

    /* 4) Parâmetros e contadores a zero */
    if (carregar_config(&config, argc, argv) == -1) {
        unlink(FICHEIRO_PID);        /* a partir daqui o ficheiro é nosso */
        return 1;
    }

    memset(&stats, 0, sizeof(stats));

    /* 5) Tarifador */
    if (iniciar_tarifador(&tarifador, config.bin_dir) == -1) {
        unlink(FICHEIRO_PID);
        return 1;
    }
    printf("Tarifador iniciado (PID %d)\n", (int) tarifador.pid);

    printf("Central iniciada: max. estafetas=%d, prazo=%d s, bindir=%s\n",
           config.max_estafetas, config.prazo_aceitacao, config.bin_dir);
    printf("Comandos: pedido <origem> <destino> <peso>, estafetas, pedidos, stats,\n"
           "          prazo <n>, encerra\n");

    /* 6) Ciclo da consola (acaba com o encerra, Ctrl+D ou Ctrl+C) */
    while (!terminar && !sinal_recebido) {
        printf("> ");
        fflush(stdout);              /* mostra o prompt já */

        /* lê uma linha inteira; NULL = fim de ficheiro (Ctrl+D)
           ou interrompido pelo SIGINT */
        if (fgets(linha, sizeof(linha), stdin) == NULL)
            break;

        /* separa a linha em palavras; n = quantas encontrou.
           Lemos 5 (o pedido leva 4) para detetar palavras a mais */
        n = sscanf(linha, "%63s %63s %63s %63s %63s",
                   p[0], p[1], p[2], p[3], p[4]);

        if (n < 1)                   /* linha vazia */
            continue;

        if (strcmp(p[0], "stats") == 0) {
            if (n != 1)              /* stats não leva parâmetros: n tem de ser 1 */
                printf("Uso: stats\n");
            else
                mostrar_stats(&stats);
        }
        else if (strcmp(p[0], "encerra") == 0) {
            if (n != 1)
                printf("Uso: encerra\n");
            else
                terminar = 1;
        }
        else if (strcmp(p[0], "pedido") == 0) {
            float peso, distancia, valor;
            char  lixo;

            if (n != 4) {            /* pedido <origem> <destino> <peso> */
                printf("Uso: pedido <origem> <destino> <peso>\n");
            }
            else if (strlen(p[1]) >= TAM_LOCAL || strlen(p[2]) >= TAM_LOCAL) {
                printf("Erro: origem e destino têm no máximo %d caracteres\n",
                       TAM_LOCAL - 1);
            }
            /* peso: número real entre 0 e VALOR_MAXIMO (isto também
               rejeita "nan" e "inf") */
            else if (sscanf(p[3], "%f %c", &peso, &lixo) != 1 ||
                     !(peso > 0 && peso < VALOR_MAXIMO)) {
                printf("Erro: peso inválido (%s)\n", p[3]);
            }
            else {
                stats.recebidos++;
                if (tarifar_pedido(&tarifador, p[1], p[2], p[3],
                                   &distancia, &valor) == 0)
                    printf("Pedido %s -> %s (%s kg): %.2f km, %.2f euros\n",
                           p[1], p[2], p[3], distancia, valor);
            }
        }
        else if (strcmp(p[0], "estafetas") == 0) {
            if (n != 1)
                printf("Uso: estafetas\n");
            else
                printf("Não há estafetas ligados.\n");   /* ainda não existem */
        }
        else if (strcmp(p[0], "pedidos") == 0) {
            if (n != 1)
                printf("Uso: pedidos\n");
            else
                printf("Não há pedidos por atribuir nem em curso.\n");
        }
        else if (strcmp(p[0], "prazo") == 0) {
            int novo_prazo;

            /* prazo <n>: duas palavras e n inteiro positivo (a mesma
               função que valida o -p) */
            if (n != 2 || ler_inteiro(p[1], &novo_prazo) == -1) {
                printf("Uso: prazo <segundos>\n");
            }
            else {
                config.prazo_aceitacao = novo_prazo;
                printf("Prazo de aceitação passou a %d segundos\n", novo_prazo);
            }
        }
        else {
            printf("Comando desconhecido: %s\n", p[0]);
        }
    }

    /* 7) Fim: é igual para o encerra, o Ctrl+D e o Ctrl+C */
    if (sinal_recebido)
        printf("\nSIGINT recebido.\n");
    printf("A encerrar a central...\n");
    terminar_tarifador(&tarifador);
    unlink(FICHEIRO_PID);            /* apaga o ficheiro de PID */
    printf("Central encerrada.\n");
    return 0;
}