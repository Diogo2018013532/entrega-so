/* tarifador_exemplo.c - SÓ PARA TESTES, NÃO ENTREGAR.
   Imita o tarifador dos docentes: lê uma linha "origem destino peso" do stdin
   e responde com uma linha "distancia valor" no stdout. Termina quando o
   stdin fecha. O fflush é necessário para a resposta sair logo pelo pipe. */
#include <stdio.h>

int main(void) {
    char  origem[16], destino[16];
    float peso;

    while (scanf("%15s %15s %f", origem, destino, &peso) == 3) {
        float km    = 3.0f + peso;           /* distância inventada */
        float valor = 2.0f + km * 0.5f;      /* valor inventado */
        printf("%.1f %.2f\n", km, valor);
        fflush(stdout);
    }
    return 0;
}
