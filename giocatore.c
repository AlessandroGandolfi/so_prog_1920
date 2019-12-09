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

void initPedine(char *, int, char *, char *);

/* globali */
pid_t pids_pedine[SO_NUM_P];
ped *mc_ped_squadra;
int mc_id_squadra;

int main(int argc, char **argv) {
    int status;

    mc_ped_squadra = (ped *) shmat(mc_id_squadra, NULL, 0);

    /* creazione pedine, valorizzazione pids_pedine */
    initPedine(argv[0], atoi(argv[1]), argv[2], argv[3]);

    /* attesa terminazione di tutte le pedine */
    while(wait(&status) > 0);
    TEST_ERROR;

    shmdt(mc_ped_squadra);
    TEST_ERROR;
	shmctl(mc_id_squadra, IPC_RMID, NULL);
    TEST_ERROR;

    exit(EXIT_SUCCESS);
}

void initPedine(char *token_gioc, int pos_token, char *mc_id_scac, char *mc_id_squadra) {
    int i;
    char **param_pedine;
    struct sembuf sops;

    /* parametri a pedine */
    param_pedine = (char **) calloc(4, sizeof(char *));
    param_pedine[0] = mc_id_scac;
    sprintf(param_pedine[1], "%d", mc_id_squadra);
    param_pedine[3] = NULL;

    for(i = 0; i < SO_NUM_P; i++) {
        /* check posizioni */
        sops.sem_num = pos_token;
        sops.sem_op = -1;
        sops.sem_flg = 0;
        semop(token_gioc, &sops, 1);

        // occupa cella
        
        /* rilascia token successivo */
        sops.sem_num = (pos_token == (SO_NUM_G - 1)) ? 0 : pos_token + 1;
        sops.sem_op = 1;
        sops.sem_flg = 0;
        semop(token_gioc, &sops, 1);

        /* creazione proc pedine */
        pids_pedine[i] = fork();
        if(!pids_pedine[i]) {
            /* passo indice ciclo a pedina per accesso diretto a propria struttura in array */
            sprintf(param_pedine[2], "%d", i);

            execve("./pedina", param_pedine, NULL);
            TEST_ERROR;
        }
    }
}