#include "./config.h"

void initGiocatori(int);
void initScacchiera();
void rmScacchiera();
void stampaScacchiera();

/* globali */
pid_t pids_giocatori[SO_NUM_G];
int *mc_sem_scac;

int main(int argc, char **argv) {
    int status, mc_id_scac;
    semun arg;

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

    /* bzero(val_array, sizeof(val_array)); */
    
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
    
    /* da gestire caso in cui semaforo a 0 (cella occupata) */
    for(i = 0; i < SO_ALTEZZA; i++) {
        semctl(mc_sem_scac[i], 0, GETALL, val_array);
        TEST_ERROR;
        for(j = 0; j < SO_BASE; j++) {  
            printf("%u", val_array[j]);
        }
        printf("\n");
    }
}

void initGiocatori(int mc_id_scac) {
    int i;
    char **param_giocatori;
    char id_param[3 * sizeof(mc_id_scac) + 1];

    /* salvataggio mem_cond_id in id_param come stringa

    uso nel caso debba salvare anche altri dati di dimensione diversa
    id_param = (char *) malloc(sizeof(char)); */
    sprintf(id_param, "%d", mc_id_scac);

    /* parametri a giocatore */
    param_giocatori = (char **) calloc(3, sizeof(char *));
    param_giocatori[0] = "giocatore";
    param_giocatori[1] = id_param;
    param_giocatori[3] = NULL;

    for(i = 0; i < SO_NUM_G; i++) {
        pids_giocatori[i] = fork();
        TEST_ERROR;
        if(!pids_giocatori[i]) {
            /* esecuzione codice giocatore */
            execve("./giocatore", param_giocatori, NULL);
            TEST_ERROR;
        }
    }
}