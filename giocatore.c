#include "./config.h"

int main(int argc, char **argv) {
    char id_param[10];
    int status;
    pid_t pids_pedine[SO_NUM_P];
    /* id sm da parametri */
    int shid = atoi(argv[1]);
    /* collegamento a sm */
    int *shared_data = shmat(shid, NULL, 0);

    printf("%s: %d, val: \n", argv[0], getpid(), *shared_data);

    sprintf(id_param, "%d", shid);
    char *param_pedine[] = { 
        "pedina",
        id_param,
        NULL 
    };

    for(int i = 0; i < SO_NUM_P; i++) {
        pids_pedine[i] = fork();
        if(!pids_pedine[i]) {
            execve("./pedina", param_pedine, NULL);
        }
    }

    /* attesa terminazione di tutte le pedine */
    while(wait(&status) > 0);

    exit(EXIT_SUCCESS);
}