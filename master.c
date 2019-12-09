#include "./config.h"

void initGiocatori(int);
void initScacchiera();
void rmScacchiera();
void stampaScacchiera();

/* globali */
gioc giocatori[SO_NUM_G];
int *mc_sem_scac;
int token_gioc;

int main(int argc, char **argv) {
    int status, mc_id_scac;

    /* creazione mem condivisa */
    mc_id_scac = shmget(IPC_PRIVATE, sizeof(int) * SO_ALTEZZA, S_IRUSR | S_IWUSR);
    TEST_ERROR;

    /* collegamento a mem cond */
    mc_sem_scac = (int *) shmat(mc_id_scac, NULL, 0);
    TEST_ERROR;

    initScacchiera();
    
    if(DEBUG) stampaScacchiera();

    /* creazione giocatori, valorizzazione pids_giocatori */
    initGiocatori(mc_id_scac);

    /* attesa terminazione di tutti i giocatori */
    while(wait(&status) > 0);
    TEST_ERROR;

    /* detach mc, rm mc, rm scac */
    rmScacchiera();

    shmdt(mc_sem_scac);
    TEST_ERROR;
	shmctl(mc_id_scac, IPC_RMID, NULL);
    TEST_ERROR;

    return 0;
}

void initScacchiera() {
    int i;
    semun sem_arg;
    unsigned short val_array[SO_BASE];
    
    for(i = 0; i < SO_BASE; i++) val_array[i] = 1;
    sem_arg.array = val_array;

    for(i = 0; i < SO_ALTEZZA; i++) {
        mc_sem_scac[i] = semget(IPC_PRIVATE, SO_BASE, 0600);
        TEST_ERROR;

        semctl(mc_sem_scac[i], 0, SETALL, sem_arg);
        TEST_ERROR;
    }

    if(DEBUG) { 
        semctl(mc_sem_scac[3], 5, SETVAL, 7);
        TEST_ERROR;
    }
}

void rmScacchiera() {
    int i;

    for(i = 0; i < SO_ALTEZZA; i++) {
        semctl(mc_sem_scac[i], 0, IPC_RMID);
        TEST_ERROR;
    }
}

void stampaScacchiera() {
    int i, j;
    /* implementare semun (?) */
    unsigned short val_array[SO_BASE];

    for(i = 0; i < SO_NUM_G; i++) giocatori[i].tot_mosse_rim = 0;
    
    /* da gestire caso in cui semaforo a 0 (cella occupata) */
    for(i = 0; i < SO_ALTEZZA; i++) {
        semctl(mc_sem_scac[i], 0, GETALL, val_array);
        TEST_ERROR;
        for(j = 0; j < SO_BASE; j++) {
            /* gestione stampa pedina giocatore, punteggio somma mosse rimanenti */ 
            printf("%u", val_array[j]);
        }
        printf("\n");
    }
    
    for(i = 0; i < SO_NUM_G; i++) 
        printf("Punteggio giocatore %d: %d, %d mosse totali rimanenti\n", (i + 1), giocatori[i].punteggio, giocatori[i].tot_mosse_rim);
}

void initGiocatori(int mc_id_scac) {
    int i;
    char **param_giocatori;
    semun sem_arg;
    unsigned short val_array[SO_BASE];
    
    /* init token posizionamento pedine */
    for(i = 0; i < SO_NUM_G; i++) val_array[i] = i ? 0 : 1;
    sem_arg.array = val_array;
    token_gioc = semget(IPC_PRIVATE, SO_NUM_G, 0600);
    TEST_ERROR;
    semctl(token_gioc, 0, SETALL, sem_arg);
    TEST_ERROR;

    /* 
    parametri a giocatore 
        - id token per posizionamento pedine
        - indice proprio token
        - id mc scacchiera
        - id mc squadra
    */
    /* salvataggio mem_cond_id in id_param come stringa

    uso nel caso debba salvare anche altri dati di dimensione diversa
    id_param = (char *) malloc(sizeof(char)); */
    param_giocatori = (char **) calloc(5, sizeof(char *));
    sprintf(param_giocatori[0], "%d", token_gioc);
    sprintf(param_giocatori[2], "%d", mc_id_scac);
    param_giocatori[4] = NULL;

    for(i = 0; i < SO_NUM_G; i++) {
        giocatori[i].punteggio = 0;
        /* mc di squadra, array di pedine, visibile solo alla squadra */
        giocatori[i].mc_id_squadra = shmget(IPC_PRIVATE, sizeof(ped) * SO_NUM_P, S_IRUSR | S_IWUSR);
        TEST_ERROR;

        /* posizione token giocatore */
        sprintf(param_giocatori[1], "%d", i);
        /* passaggio id mc di squadra come parametro a giocatori */
        sprintf(param_giocatori[3], "%d", giocatori[i].mc_id_squadra);

        giocatori[i].pid = fork();
        TEST_ERROR;

        switch(giocatori[i].pid) {
            case -1:
                TEST_ERROR;
                exit(EXIT_FAILURE);
            case 0:
                /* esecuzione codice giocatore */
                execve("./giocatore", param_giocatori, NULL);
                TEST_ERROR;
                exit(EXIT_FAILURE);
        }
    }
}