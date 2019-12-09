/*
il giocatore cerca di conquistare le pedine che
    valgono di piú
    sono piú vicine
pedine distribuite con spazio tra di esse
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
ped *mc_ped_squadra;
int mc_id_squadra;

int main(int argc, char **argv) {
    int status;

    /* array di pedine con relative informazioni, visibile solo alla squadra */
    mc_id_squadra = shmget(IPC_PRIVATE, sizeof(ped) * SO_NUM_P, S_IRUSR | S_IWUSR);
    TEST_ERROR;

    mc_ped_squadra = (ped *) shmat(mc_id_squadra, NULL, 0);

    /* creazione pedine, valorizzazione pids_pedine */
    initPedine(argv[1]);

    /* attesa terminazione di tutte le pedine */
    while(wait(&status) > 0);
    TEST_ERROR;

    shmdt(mc_ped_squadra);
    TEST_ERROR;
	shmctl(mc_id_squadra, IPC_RMID, NULL);
    TEST_ERROR;

    exit(EXIT_SUCCESS);
}

void initPedine(char *mc_id_scac) {
    int i;
    char **param_pedine;

    /* parametri a pedine */
    param_pedine = (char **) calloc(4, sizeof(char *));
    param_pedine[0] = mc_id_scac;
    sprintf(param_pedine[1], "%d", mc_id_squadra);
    param_pedine[3] = NULL;

    for(i = 0; i < SO_NUM_P; i++) {
        pids_pedine[i] = fork();
        if(!pids_pedine[i]) {
            /* passo indice ciclo a pedina per accesso diretto a propria struttura in array */
            sprintf(param_pedine[2], "%d", i);

            execve("./pedina", param_pedine, NULL);
            TEST_ERROR;
        }
    }
}