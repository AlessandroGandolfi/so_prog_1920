/*
la scacchiera é una puttanata
*/

#include "./config.h"

int main(int argc, char **argv) {
    pid_t pids_giocatori[SO_NUM_G];
    char *id_param;
    int *shared_data;
    int status, i, shid;

    /* creazione mem condivisa */
    shid = shmget(IPC_PRIVATE, sizeof(int), S_IRUSR | S_IWUSR);
    if (shid == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* 
    collegamento a mem cond 
    no malloc perché in kernel
    */
    shared_data = (int *) shmat(shid, NULL, 0);

    /* valorizzazione intero in mem condivisa */
    *shared_data = 10;
    if(errno) {
        fprintf(stderr, "%s: %d. Errore in shmat #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* salvataggio shid in id_param come stringa */
    id_param = (char *) malloc(sizeof(char));
    sprintf(id_param, "%d", shid);

    /* parametri a giocatore */
    char *param_giocatori[] = { 
        "giocatore",
        id_param,
        NULL 
    };

    for(i = 0; i < SO_NUM_G; i++) {
        pids_giocatori[i] = fork();
        if(!pids_giocatori[i]) {
            execve("./giocatore", param_giocatori, NULL);
        }
    }

    /* attesa terminazione tutti i giocatori */
    while(wait(&status) > 0);

    return 0;
}