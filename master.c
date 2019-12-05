#include "./config.h"

void initGiocatori(int);
void initSemafori();

/* globali */
pid_t pids_giocatori[SO_NUM_G];
int sem_celle[SO_ALTEZZA];

int main(int argc, char **argv) {
    int *shared_data;
    int status, shid;
    semun arg;
    
    initSemafori();

    /* creazione mem condivisa */
    shid = shmget(IPC_PRIVATE, sizeof(int), S_IRUSR | S_IWUSR);
    if (shid == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* collegamento a mem cond */
    shared_data = (int *) shmat(shid, NULL, 0);

    /* valorizzazione intero in mem condivisa */
    *shared_data = 10;
    if(errno) {
        fprintf(stderr, "%s: %d. Errore in shmat #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* creazione giocatori, valorizzazione pids_giocatori 
    initGiocatori(shid);*/

    /* attesa terminazione di tutti i giocatori */
    while(wait(&status) > 0);

    return 0;
}

void initSemafori() {
    int i;
    semun sem_arg;

    sem_arg.array = (unsigned short *) malloc(sizeof(unsigned short));

    /* qualcosa all'interno del ciclo fa esplodere il programma */
    for(i = 0; i < SO_ALTEZZA; i++) {
        sem_arg.array[i] = i * 2;
        sem_celle[i] = semget(IPC_PRIVATE, SO_BASE, IPC_CREAT | IPC_EXCL);
        semctl(sem_celle[i], 0, SETALL, sem_arg);
    }

    printf("eooooooo\n");
    sem_arg.val = 5;
    semctl(sem_celle[2], 3, SETVAL, sem_arg);
    semctl(sem_celle[2], 0, GETALL, sem_arg);
    printf("%d\n", sem_arg.array[3]);
}

void initGiocatori(int mem_cond_id) {
    int i;
    char **param_giocatori;
    char *id_param;

    /* salvataggio mem_cond_id in id_param come stringa */
    id_param = (char *) malloc(sizeof(char));
    sprintf(id_param, "%d", mem_cond_id);

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
        }
    }
}