#include "./config.h"

void initGiocatori(int);
void initSemafori();
void stampaSemafori();

/* globali */
pid_t pids_giocatori[SO_NUM_G];
int sem_celle[SO_ALTEZZA];

int main(int argc, char **argv) {
    int *sm_sem;
    int status, mc_id_scac;
    semun arg;
    
    initSemafori();

    if(DEBUG) stampaSemafori();

    /* creazione mem condivisa */
    mc_id_scac = shmget(IPC_PRIVATE, sizeof(sem_celle), S_IRUSR | S_IWUSR);
    TEST_ERROR;

    /* collegamento a mem cond */
    sm_sem = (int *) shmat(mc_id_scac, NULL, 0);

    /* valorizzazione intero in mem condivisa */
    sm_sem = sem_celle;
    TEST_ERROR;

    /* creazione giocatori, valorizzazione pids_giocatori */
    initGiocatori(mc_id_scac);

    /* attesa terminazione di tutti i giocatori */
    while(wait(&status) > 0);

    /* detach mc, rm mc, rm scac */

    return 0;
}

void initSemafori() {
    int i;
    semun sem_arg;
    unsigned short val_array[SO_BASE];

    bzero(val_array, sizeof(val_array));
    sem_arg.array = val_array;

    /* qualcosa all'interno del ciclo fa esplodere il programma */
    for(i = 0; i < SO_ALTEZZA; i++) {
        sem_celle[i] = semget(IPC_PRIVATE, SO_BASE, 0600);
        TEST_ERROR;

        semctl(sem_celle[i], 0, SETALL, sem_arg);
        TEST_ERROR;
    }

    if(DEBUG){ semctl(sem_celle[3], 5, SETVAL, 7);
    TEST_ERROR;}
}

void stampaSemafori() {
    int i, j;
    /* implementare semun (?) */
    unsigned short val_array[SO_BASE];
    
    for(i = 0; i < SO_ALTEZZA; i++) {
        semctl(sem_celle[i], 0, GETALL, val_array);
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
    id_param = (char *) malloc(sizeof(char)); */
    sprintf(id_param, "%d", mc_id_scac);

    /* parametri a giocatore */
    param_giocatori = (char **) calloc(3, sizeof(char *));
    param_giocatori[0] = "giocatore";
    param_giocatori[1] = id_param;
    param_giocatori[3] = NULL;

    for(i = 0; i < SO_NUM_G; i++) {
        pids_giocatori[i] = fork();
        if(!pids_giocatori[i]) {
            /* esecuzione codice giocatore */
            execve("./giocatore", param_giocatori, NULL);
            TEST_ERROR;
        }
    }
}