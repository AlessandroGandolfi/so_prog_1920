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
void piazzaPedina(int, int, int);
int checkPosPedine(int, int);

/* globali */
pid_t pids_pedine[SO_NUM_P];
ped *mc_ped_squadra;
int *mc_sem_scac;
int mc_id_squadra;

/* 
parametri a giocatore 
- id token per posizionamento pedine
- indice proprio token
- id mc scacchiera
- id mc squadra
*/
int main(int argc, char **argv) {
    int status, i;

    // fare srand se serve

    /* collegamento a mem cond */
    mc_ped_squadra = (ped *) shmat(atoi(argv[3]), NULL, 0);
    TEST_ERROR;

        printf("1\n");
    for(i = 0; i < SO_NUM_P; i++) {
        printf("giocatore num %d, pedina %d\n", (atoi(argv[1]), mc_ped_squadra[i].mosse_rim));
    }
        printf("2\n");
    // mc_sem_scac = (int *) shmat(atoi(argv[2]), NULL, 0);
    // TEST_ERROR;

    // /* creazione pedine, valorizzazione pids_pedine */
    // initPedine(argv[0], atoi(argv[1]), argv[2], argv[3]);

    // /* attesa terminazione di tutte le pedine */
    // while(wait(&status) > 0);
    // TEST_ERROR;

    // shmdt(mc_sem_scac);
    // TEST_ERROR;

    // shmdt(mc_ped_squadra);
    // TEST_ERROR;

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
        semop(atoi(token_gioc), &sops, 1);

        piazzaPedina(atoi(mc_id_scac), atoi(mc_id_squadra), i);
        
        /* 
            rilascia token successivo
            quarto giocatore a mettere ultima pedina non rilascia risorsa primo giocatore

            si potrebbe usare per una wait for zero?
        */
        if(i != (SO_NUM_P - 1) && pos_token != (SO_NUM_G - 1)) {
            sops.sem_num = (pos_token == (SO_NUM_G - 1)) ? 0 : pos_token + 1;
            sops.sem_op = 1;
            sops.sem_flg = 0;
            semop(atoi(token_gioc), &sops, 1);
        }

        /* creazione proc pedine */
        pids_pedine[i] = fork();

        switch(pids_pedine[i]) {
            case -1:
                TEST_ERROR;
                exit(EXIT_FAILURE);
            case 0:
                /* passo indice ciclo a pedina per accesso diretto a propria struttura in array */
                sprintf(param_pedine[2], "%d", i);
                execve("./pedina", param_pedine, NULL);
                TEST_ERROR;
        }
    }
}

void piazzaPedina(int mc_id_scac, int mc_id_squadra, int ind_pedine) {
    int riga, colonna;
    struct sembuf sops;
    ped nuovaPedina;

    do {
        riga = rand() % SO_ALTEZZA;
        colonna = rand() % SO_BASE;

        sops.sem_num = colonna;
        sops.sem_op = -1;
        sops.sem_flg = 0;
    } while(semop(mc_sem_scac[riga], &sops, 1) == -1 && checkPosPedine(riga, colonna));

    mc_ped_squadra[ind_pedine].mosse_rim = SO_N_MOVES;
    mc_ped_squadra[ind_pedine].pos_attuale.x = colonna;
    mc_ped_squadra[ind_pedine].pos_attuale.y = riga;
}

int checkPosPedine(int riga, int colonna) {
    return 1;
}