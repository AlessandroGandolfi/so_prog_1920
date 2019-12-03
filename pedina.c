#include "./config.h"

int main(int argc, char **argv) {
    /* id sm da parametri */
    int shid = atoi(argv[1]);
    /* collegamento a sm */
    int *shared_data = shmat(shid, NULL, 0);

    printf("%s: %d, val: \n", argv[0], getpid(), *shared_data);
    exit(EXIT_SUCCESS);
}