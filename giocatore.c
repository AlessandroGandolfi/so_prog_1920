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
#include "config.h"

void initPedine(char *, int, char *);
void piazzaPedina(int);
int checkPosPedine(int, int);
void initObiettivi(int);

/* globali */
pid_t pids_pedine[SO_NUM_P];
ped *mc_ped_squadra;
int *mc_sem_scac;
int mc_id_squadra, msg_id_coda;

/* 
parametri a giocatore 
- id token per posizionamento pedine
- indice proprio token
- id mc scacchiera, array id set semafori
- id mc squadra, array pedine
- id coda msg
*/
int main(int argc, char **argv) {
    int status, i;

    srand(time(NULL) + getpid());

    /* collegamento a mem cond */
    mc_ped_squadra = (ped *) shmat(atoi(argv[3]), NULL, 0);
    TEST_ERROR;
    
    mc_sem_scac = (int *) shmat(atoi(argv[2]), NULL, 0);
    TEST_ERROR;
    
    mc_id_squadra = atoi(argv[3]);

    /* creazione pedine, valorizzazione pids_pedine */
    initPedine(argv[0], atoi(argv[1]), argv[2]);

    /* assegnazione obiettivi */
    initObiettivi(atoi(argv[4]));

    /* attesa terminazione di tutte le pedine */
    while(wait(&status) > 0);
    TEST_ERROR;

    shmdt(mc_sem_scac);
    TEST_ERROR;

    shmdt(mc_ped_squadra);
    TEST_ERROR;

    exit(EXIT_SUCCESS);
}

void initPedine(char *token_gioc, int pos_token, char *mc_id_scac) {
    int i;
    char *param_pedine[4];
    char tmp_params[2][sizeof(char *)];
    struct sembuf sops;
    msg_fine_piaz avviso_master;

    /* 
    parametri a pedine
    - id mc scacchiera, array id set semafori
    - id mc squadra, array pedine
    - indice identificativo pedina dell'array in mc squadra
    */
    param_pedine[0] = mc_id_scac;
    sprintf(tmp_params[0], "%d", mc_id_squadra);
    param_pedine[1] = tmp_params[0];
    param_pedine[3] = NULL;
    
    for(i = 0; i < SO_NUM_P; i++) {
        mc_ped_squadra[i].pos_attuale.x = -1; 
        mc_ped_squadra[i].pos_attuale.y = -1;
    }

    for(i = 0; i < SO_NUM_P; i++) {
        /* check posizioni */
        sops.sem_num = pos_token;
        sops.sem_op = -1;
        sops.sem_flg = 0;
        semop(atoi(token_gioc), &sops, 1);

        if(DEBUG) printf("gioc: %d\tped: %d\n", pos_token, i);

        piazzaPedina(i);

        sops.sem_num = (pos_token == (SO_NUM_G - 1)) ? 0 : pos_token + 1;
        sops.sem_op = 1;
        sops.sem_flg = 0;
        semop(atoi(token_gioc), &sops, 1);

        if(i == (SO_NUM_P - 1) && pos_token == (SO_NUM_G - 1)) { 
            if(DEBUG) printf("gioc %d msg fine piazzam\n", pos_token);
        	avviso_master.mtype = (long) getpid();
            avviso_master.fine_piaz = 1;
            printf("aaaaaaaaaa\n");
            msgsnd(msg_id_coda, &avviso_master, sizeof(msg_fine_piaz) - sizeof(long), 0);
            TEST_ERROR;
            printf("dfghjk\n");
        }

        /* creazione proc pedine */
        pids_pedine[i] = fork();

        switch(pids_pedine[i]) {
            case -1:
                TEST_ERROR;
                exit(EXIT_FAILURE);
            case 0:
                /* passo indice ciclo a pedina per accesso diretto a propria struttura in array */
                sprintf(tmp_params[1], "%d", i);
                param_pedine[2] = tmp_params[1];
                execve("./pedina", param_pedine, NULL);
                TEST_ERROR;
                exit(EXIT_FAILURE);
        }
    }
}

void piazzaPedina(int ind_pedine) {
    int riga, colonna;
    struct sembuf sops;
    
    do {
        do {
            riga = rand() % SO_ALTEZZA;
            colonna = rand() % SO_BASE;
        } while(checkPosPedine(riga, colonna));

        sops.sem_num = colonna;
        sops.sem_op = -1;
        sops.sem_flg = IPC_NOWAIT;
    } while(semop(mc_sem_scac[riga], &sops, 1) == -1);

    mc_ped_squadra[ind_pedine].mosse_rim = SO_N_MOVES;
    mc_ped_squadra[ind_pedine].pos_attuale.x = colonna;
    mc_ped_squadra[ind_pedine].pos_attuale.y = riga;
}

int checkPosPedine(int riga, int colonna) {
    int i, check;

    i = 0;
    check = 0;

    while(mc_ped_squadra[i].pos_attuale.x != -1 && mc_ped_squadra[i].pos_attuale.y != -1 && i < SO_NUM_P && check == 0) {
        if(calcDist(colonna, mc_ped_squadra[i].pos_attuale.x, riga, mc_ped_squadra[i].pos_attuale.y) < DIST_PED_GIOC) check++;
        i++;
    }

    return check;
}

void initObiettivi(int msg_id_coda) {
    msg_band msg;

    msgrcv(msg_id_coda, &msg, sizeof(msg_band) - sizeof(long), (long) getppid(), 0);
    TEST_ERROR;
}