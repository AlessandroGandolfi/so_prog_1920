/*
il giocatore cerca di conquistare le pedine che
    valgono di piú
    sono piú vicine
pedine distrinuitr con spazio tra di esse
le indicazioni del giocatore consistono solo nel dare l'obiettivo da raggiungere alle pedine
nel caso venga segnalato che una bandierina é stata presa il giocatore
    controlla che abbia assegnato delle pedine a quella bandiera e chi l'ha presa
        se nemico puó far continuare avanzamento delle pedine
        se propria pedina assegna nuovo obiettivo
*/
#include "./config.h"

void initPedine(char *);

/* globali */
pid_t pids_pedine[SO_NUM_P];

int main(int argc, char **argv) {
    int status;

    /* creazione pedine, valorizzazione pids_pedine */
    initPedine(argv[1]);

    /* attesa terminazione di tutte le pedine */
    while(wait(&status) > 0);
    TEST_ERROR;

    exit(EXIT_SUCCESS);
}

void initPedine(char *mc_id_scac) {
    int i;
    char **param_pedine;

    /* parametri a pedine */
    param_pedine = (char **) calloc(3, sizeof(char *));
    param_pedine[0] = "pedina";
    param_pedine[1] = mc_id_scac;
    param_pedine[2] = NULL;

    for(i = 0; i < SO_NUM_P; i++) {
        pids_pedine[i] = fork();
        if(!pids_pedine[i]) {
            execve("./pedina", param_pedine, NULL);
            TEST_ERROR;
        }
    }
}