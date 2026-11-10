#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <math.h>

#include "config.h"
#include "estruturas.h"
#include "mensagens.h"

typedef struct {
    pid_t pid;
    int pipe_entrada;
    int pipe_saida;
} Tarifador;

 volatile sig_atomic_t sinal_recebido = 0;

/* Handler do SIGINT: apenas assinala que a central deve terminar. */
 void tratar_sigint(int sinal) {
    (void)sinal;
    sinal_recebido = 1;
}

/* Handler do SIGPIPE: impede que a central termine se o tarifador fechar o pipe. */
 void tratar_sigpipe(int sinal) {
    (void)sinal;
}

/* Configurar os sinais utilizados pela central. */
 int configurar_sinais(void) {
    struct sigaction acao;

    memset(&acao, 0, sizeof(acao));
    sigemptyset(&acao.sa_mask);

    acao.sa_handler = tratar_sigint;
    if (sigaction(SIGINT, &acao, NULL) == -1) {
        perror("sigaction SIGINT");
        return -1;
    }

    acao.sa_handler = tratar_sigpipe;
    if (sigaction(SIGPIPE, &acao, NULL) == -1) {
        perror("sigaction SIGPIPE");
        return -1;
    }

    return 0;
}

/* Converte texto num inteiro positivo. */
 int converter_inteiro(const char *texto, int *resultado) {
    long valor;
    char lixo;

    if (sscanf(texto, "%ld %c", &valor, &lixo) != 1)
        return -1;

    if (valor < 1 || valor > INT_MAX)
        return -1;

    *resultado = (int)valor;
    return 0;
}

/* Carregar configuração: valores por omissão, ambiente e argumentos. */
 int carregar_config(ConfigCentral *config, int argc, char *argv[]) {
    char *valor;
    int i;

    memset(config, 0, sizeof(*config));

    config->max_estafetas = MAX_ESTAFETAS_OMISSAO;
    config->prazo_aceitacao = PRAZO_ACEITA_OMISSAO;
    snprintf(config->bin_dir, sizeof(config->bin_dir), ".");

    valor = getenv(ENV_MAX_ESTAFETAS);
    if (valor != NULL && converter_inteiro(valor, &config->max_estafetas) == -1) {
        printf("Erro: MAXESTAFETAS inválido (%s).\n", valor);
        return -1;
    }

    valor = getenv(ENV_PRAZO_ACEITA);
    if (valor != NULL && converter_inteiro(valor, &config->prazo_aceitacao) == -1) {
        printf("Erro: PRAZOACEITA inválido (%s).\n", valor);
        return -1;
    }

    valor = getenv(ENV_BINDIR);
    if (valor != NULL) {
        if (strlen(valor) >= sizeof(config->bin_dir)) {
            printf("Erro: BINDIR demasiado longo.\n");
            return -1;
        }
        strcpy(config->bin_dir, valor);
    }

    valor = getenv(ENV_CENTRAL_PIPE);
    if (valor != NULL) {
        if (strlen(valor) >= sizeof(config->central_pipe)) {
            printf("Erro: CENTRAL_PIPE demasiado longo.\n");
            return -1;
        }
        strcpy(config->central_pipe, valor);
    }

    /* Os argumentos -m e -p substituem os valores do ambiente. */
    for (i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            printf("Erro: falta o valor do argumento %s.\n", argv[i]);
            return -1;
        }

        if (strcmp(argv[i], "-m") == 0) {
            if (converter_inteiro(argv[i + 1], &config->max_estafetas) == -1) {
                printf("Erro: valor inválido para -m (%s).\n", argv[i + 1]);
                return -1;
            }
        } else if (strcmp(argv[i], "-p") == 0) {
            if (converter_inteiro(argv[i + 1], &config->prazo_aceitacao) == -1) {
                printf("Erro: valor inválido para -p (%s).\n", argv[i + 1]);
                return -1;
            }
        } else {
            printf("Erro: argumento desconhecido (%s).\n", argv[i]);
            return -1;
        }
    }

    if (config->max_estafetas > MAX_ESTAFETAS) {
        printf("Erro: máximo de estafetas é %d.\n", MAX_ESTAFETAS);
        return -1;
    }

    return 0;
}

/* Verificar se já existe outra central em execução. */
 int verificar_instancia_unica(void) {
    int fd;
    struct flock bloqueio;

    fd = open("/tmp/entrega_so_central.lock", O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("Erro ao abrir ficheiro de bloqueio");
        return -1;
    }

    memset(&bloqueio, 0, sizeof(bloqueio));
    bloqueio.l_type = F_WRLCK;
    bloqueio.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLK, &bloqueio) == -1) {
        if (errno == EACCES || errno == EAGAIN)
            printf("Erro: já existe uma central em execução.\n");
        else
            perror("Erro ao bloquear ficheiro");

        close(fd);
        return -1;
    }

    /* O descritor fica aberto até ao fim para manter o bloqueio. */
    return fd;
}

/* Iniciar o tarifador através de dois pipes. */
 int iniciar_tarifador(Tarifador *t, const char *bin_dir) {
    char caminho[256];
    int p_entrada[2];
    int p_saida[2];

    if (snprintf(caminho, sizeof(caminho), "%s/tarifador", bin_dir) >= (int)sizeof(caminho)) {
        printf("Erro: caminho do tarifador demasiado longo.\n");
        return -1;
    }

    if (access(caminho, X_OK) == -1) {
        perror(caminho);
        return -1;
    }

    if (pipe(p_entrada) == -1) {
        perror("pipe entrada");
        return -1;
    }

    if (pipe(p_saida) == -1) {
        perror("pipe saída");
        close(p_entrada[0]);
        close(p_entrada[1]);
        return -1;
    }

    fflush(stdout);
    t->pid = fork();

    if (t->pid == -1) {
        perror("fork");
        close(p_entrada[0]);
        close(p_entrada[1]);
        close(p_saida[0]);
        close(p_saida[1]);
        return -1;
    }

    if (t->pid == 0) {
        /* Processo filho: liga os pipes ao stdin e stdout do tarifador. */
        close(p_entrada[1]);
        close(p_saida[0]);

        if (dup2(p_entrada[0], STDIN_FILENO) == -1) {
            perror("dup2 stdin");
            exit(1);
        }

        if (dup2(p_saida[1], STDOUT_FILENO) == -1) {
            perror("dup2 stdout");
            exit(1);
        }

        close(p_entrada[0]);
        close(p_saida[1]);

        execl(caminho, "tarifador", (char *)NULL);
        perror("execl");
        exit(1);
    }

    /* Processo pai: fecha as pontas utilizadas pelo filho. */
    close(p_entrada[0]);
    close(p_saida[1]);

    t->pipe_entrada = p_entrada[1];
    t->pipe_saida = p_saida[0];

    printf("Tarifador iniciado (PID %d).\n", (int)t->pid);
    return 0;
}

/* Lê uma linha do pipe do tarifador. */
 int ler_linha(int fd, char *buffer, size_t tamanho) {
    size_t i = 0;
    char c;
    ssize_t n;

    while (i < tamanho - 1) {
        n = read(fd, &c, 1);

        if (n == -1 && errno == EINTR)
            continue;

        if (n != 1)
            return -1;

        if (c == '\n') {
            buffer[i] = '\0';
            return 0;
        }

        buffer[i++] = c;
    }

    buffer[i] = '\0';
    return -1;
}

/* Enviar um pedido ao tarifador e receber distância e valor. */
 int tarifar_pedido(const Tarifador *t, const char *origem,
                          const char *destino, const char *peso,
                          float *distancia, float *valor) {
    char pergunta[100];
    char resposta[100];
    int n;
    ssize_t escritos;

    n = snprintf(pergunta, sizeof(pergunta), "%s %s %s\n",origem, destino, peso);

    if (n < 0 || n >= (int)sizeof(pergunta)) {
        printf("Erro: pedido demasiado longo.\n");
        return -1;
    }

    escritos = write(t->pipe_entrada, pergunta, (size_t)n);
    if (escritos != n) {
        if (escritos == -1)
            perror("Erro ao escrever ao tarifador");
        else
            printf("Erro: escrita incompleta no tarifador.\n");
        return -1;
    }

    if (ler_linha(t->pipe_saida, resposta, sizeof(resposta)) == -1) {
        printf("Erro: o tarifador não respondeu.\n");
        return -1;
    }

    if (sscanf(resposta, "%f %f", distancia, valor) != 2) {
        printf("Erro: resposta inesperada do tarifador (%s).\n", resposta);
        return -1;
    }

    return 0;
}

/* Mostrar as estatísticas atuais. */
 void mostrar_stats(const Estatisticas *stats) {
    printf("\nStats\n");
    printf("Pedidos recebidos:  %d\n", stats->pedidos_recebidos);
    printf("Pedidos atribuidos: %d\n", stats->pedidos_atribuidos);
    printf("Pedidos concluidos: %d\n", stats->pedidos_concluidos);
    printf("Pedidos expirados:  %d\n", stats->pedidos_expirados);
    printf("Pedidos perdidos:   %d\n", stats->pedidos_perdidos);
    printf("Valor total das entregas concluídas: %.2f euros\n", stats->valor_total_entregas);
}

/* Terminar o tarifador: fechar os pipes e esperar pelo processo filho. */
 void terminar_tarifador(Tarifador *t) {
    close(t->pipe_entrada);
    close(t->pipe_saida);

    while (waitpid(t->pid, NULL, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            break;
        }
    }

    printf("Tarifador terminado.\n");
}

int main(int argc, char *argv[]) {
    ConfigCentral config;
    Tarifador tarifador;
    Estatisticas stats;
    char linha[200];
    char p[5][64];
    int n;
    int terminar = 0;
    int lock_fd;

    if (configurar_sinais() == -1)
        return 1;

    if (carregar_config(&config, argc, argv) == -1)
        return 1;

    lock_fd = verificar_instancia_unica();
    if (lock_fd == -1)
        return 1;

    memset(&stats, 0, sizeof(stats));

    printf("Configuração da central:\n");
    printf("Máximo de estafetas: %d\n", config.max_estafetas);
    printf("Prazo: %d segundos\n", config.prazo_aceitacao);
    printf("Diretório: %s\n\n", config.bin_dir);

    if (iniciar_tarifador(&tarifador, config.bin_dir) == -1) {
        close(lock_fd);
        return 1;
    }

    printf("Central em funcionamento.\n");
    printf("Comandos disponíveis:\n");
    printf("pedido <origem> <destino> <peso>\n");
    printf("stats\n");
    printf("encerra\n\n");

    while (!terminar && !sinal_recebido) {
        printf("> ");
        fflush(stdout);

        if (fgets(linha, sizeof(linha), stdin) == NULL) {
            if (sinal_recebido)
                clearerr(stdin);
            break;
        }

        /* Se não coube a linha toda, limpar o restante da entrada. */
        if (strchr(linha, '\n') == NULL) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF)
                ;
            printf("Erro: comando demasiado longo.\n");
            continue;
        }

        /* Separar até cinco palavras para detetar argumentos a mais. */
        n = sscanf(linha, "%63s %63s %63s %63s %63s",
                   p[0], p[1], p[2], p[3], p[4]);

        if (n < 1)
            continue;

        if (strcmp(p[0], "stats") == 0) {
            if (n != 1)
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
            float peso;
            float distancia;
            float valor;
            char lixo;

            if (n != 4) {
                printf("Uso: pedido <origem> <destino> <peso>\n");
            }
            else if (strlen(p[1]) >= TAM_LOCAL || strlen(p[2]) >= TAM_LOCAL) {
                printf("Erro: origem e destino têm no máximo %d caracteres.\n",
                       TAM_LOCAL - 1);
            }
            else {
                errno = 0;
                if (sscanf(p[3], "%f %c", &peso, &lixo) != 1 ||
                    errno == ERANGE || !(peso > 0) || !isfinite(peso)) {
                    printf("Erro: peso inválido (%s).\n", p[3]);
                    continue;
                }
		
		if (stats.pedidos_recebidos >= MAX_PEDIDOS) {
    		printf("Erro: limite máximo de pedidos atingido.\n");
    		continue;
		}
                stats.pedidos_recebidos++;

                if (tarifar_pedido(&tarifador, p[1], p[2], p[3],
                                   &distancia, &valor) == 0) {
                    printf("Pedido %s -> %s (%s kg): %.2f km, %.2f euros\n",
                           p[1], p[2], p[3], distancia, valor);
                }
            }
        }
        else {
            printf("Comando desconhecido: %s\n", p[0]);
        }
    }

    if (sinal_recebido)
        printf("\nSIGINT recebido.\n");

    printf("A encerrar a central...\n");
    terminar_tarifador(&tarifador);
    close(lock_fd);
    printf("Central encerrada.\n");

    return 0;
}
