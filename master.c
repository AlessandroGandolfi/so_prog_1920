#include "./config.h"

int main(int argc, char **argv) {
    pid_t pids_giocatori[SO_NUM_G];
    char id_param[10];
    /* creazione mem condivisa */
    int status, shid = shmget(IPC_PRIVATE, sizeof(int), S_IRUSR | S_IWUSR);
    /* collegamento a mem cond */
    int *shared_data = (int *) shmat(shid, NULL, 0);

    /* valorizzazione intero in mem condivisa */
    *shared_data = 10;
    sprintf(id_param, "%d", shid);

    /* parametri a giocatore */
    char *param_giocatori[] = { 
        "giocatore",
        id_param,
        NULL 
    };

    for(int i = 0; i < SO_NUM_G; i++) {
        pids_giocatori[i] = fork();
        if(!pids_giocatori[i]) {
            execve("./giocatore", param_giocatori, NULL);
        }
    }

    /* attesa terminazione tutti i giocatori */
    while(wait(&status) > 0);

    return 0;
}