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

int main(int argc, char **argv) {
    int status, i, shid;
    int *shared_data;
    pid_t pids_pedine[SO_NUM_P];
    char *id_param;
    char **param_pedine;

    /* id sm da parametri */
    shid = atoi(argv[1]);
    /* collegamento a sm */
    shared_data =  (int *) shmat(shid, NULL, 0);

    id_param = (char *) malloc(sizeof(char));
    sprintf(id_param, "%d", shid);

    printf("%s: %d, val: %d\n", argv[0], getpid(), *shared_data);

    param_pedine = (char **) calloc(3, sizeof(char *));
    param_pedine[0] = "pedina";
    param_pedine[1] = id_param;
    param_pedine[2] = NULL;

    for(i = 0; i < SO_NUM_P; i++) {
        pids_pedine[i] = fork();
        if(!pids_pedine[i]) {
            execve("./pedina", param_pedine, NULL);
        }
        printf("%s: %d, pedina: %d\n", argv[0], getpid(), i);
    }

    /* attesa terminazione di tutte le pedine */
    while(wait(&status) > 0);

    exit(EXIT_SUCCESS);
}