#include "./config.h"

void initGiocatori(int);
void initScacchiera();
void somebodyTookMaShmget();
void stampaScacchiera();

/* globali */
gioc giocatori[SO_NUM_G];
int *mc_sem_scac;
int token_gioc, mc_id_scac;

int main(int argc, char **argv) {
    int status;

    /* creazione mem condivisa */
    mc_id_scac = shmget(IPC_PRIVATE, sizeof(int) * SO_ALTEZZA, S_IRUSR | S_IWUSR);
    TEST_ERROR;

    /* collegamento a mem cond */
    mc_sem_scac = (int *) shmat(mc_id_scac, NULL, 0);
    TEST_ERROR;

    initScacchiera();

    /* creazione giocatori, valorizzazione pids_giocatori */
    initGiocatori(mc_id_scac);
    
    if(DEBUG) {
        sleep(3);
        stampaScacchiera();
    }

    /* attesa terminazione di tutti i giocatori */
    while(wait(&status) > 0);
    TEST_ERROR;

    /* detach mc, rm mc, rm sem scac */
    somebodyTookMaShmget();

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
}

void somebodyTookMaShmget() {
    int i;

    for(i = 0; i < SO_ALTEZZA; i++) {
        semctl(mc_sem_scac[i], 0, IPC_RMID);
        TEST_ERROR;
    }

    for(i = 0; i < SO_NUM_G; i++) {
        shmctl(giocatori[i].mc_id_squadra, IPC_RMID, NULL);
        TEST_ERROR;
    }
    
    shmdt(mc_sem_scac);
    TEST_ERROR;
	shmctl(mc_id_scac, IPC_RMID, NULL);
    TEST_ERROR;
}

void stampaScacchiera() {
    int i, j;
    char scacchiera[SO_ALTEZZA][SO_BASE];
    ped *mc_ped_squadra;
    
    for(i = 0; i < SO_ALTEZZA; i++)
        for(j = 0; j < SO_BASE; j++)
            scacchiera[i][j] = '0';
    

    for(i = 0; i < SO_NUM_G; i++) {
        giocatori[i].tot_mosse_rim = 0;
        mc_ped_squadra = (ped *) shmat(giocatori[i].mc_id_squadra, NULL, 0);
        TEST_ERROR;

        for(j = 0; j < SO_NUM_P; j++) {
            scacchiera[mc_ped_squadra[j].pos_attuale.y][mc_ped_squadra[j].pos_attuale.x] = (i + 1) + '0';
            giocatori[i].tot_mosse_rim += mc_ped_squadra[j].mosse_rim;
        }

        shmdt(mc_ped_squadra);
        TEST_ERROR;
    }

    for(i = 0; i < SO_ALTEZZA; i++) {
        for(j = 0; j < SO_BASE; j++) {
            if(ENABLE_COLORS) {
                switch(scacchiera[i][j]) {
                    case '1':
                        printf("\033[1;31m");
                        break;
                    case '2':
                        printf("\033[1;34m");
                        break;
                    case '3':
                        printf("\033[1;32m");
                        break;
                    case '4':
                        printf("\033[1;36m");
                        break;
                    default:
                        printf("\033[0m");
                        break;
                }
            }
            printf("%c", scacchiera[i][j]);
        }
        printf("\n");
    }
    
    if(ENABLE_COLORS) printf("\033[0m");

    for(i = 0; i < SO_NUM_G; i++) 
        printf("Punteggio giocatore %d: %d, %d mosse totali rimanenti\n", (i + 1), giocatori[i].punteggio, giocatori[i].tot_mosse_rim);
}

void initGiocatori(int mc_id_scac) {
    int i;
    char *param_giocatori[5];
    char tmp_params[4][sizeof(char *)];
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
        - id mc scacchiera, array id set semafori
        - id mc squadra, array pedine
    
    salvataggio mem_cond_id in id_param come stringa 
    */

    sprintf(tmp_params[0], "%d", token_gioc);
    param_giocatori[0] = tmp_params[0];
    sprintf(tmp_params[2], "%d", mc_id_scac);
    param_giocatori[2] = tmp_params[2];
    param_giocatori[4] = NULL;


    for(i = 0; i < SO_NUM_G; i++) {
        giocatori[i].punteggio = 0;
        /* mc di squadra, array di pedine, visibile solo alla squadra */
        giocatori[i].mc_id_squadra = shmget(IPC_PRIVATE, sizeof(ped) * SO_NUM_P, S_IRUSR | S_IWUSR);
        TEST_ERROR;

        /* posizione token giocatore */
        sprintf(tmp_params[1], "%d", i);
        param_giocatori[1] = tmp_params[1];
        
        /* passaggio id mc di squadra come parametro a giocatori */
        sprintf(tmp_params[3], "%d", giocatori[i].mc_id_squadra);
        param_giocatori[3] = tmp_params[3];

        giocatori[i].pid = fork();
        TEST_ERROR;

        switch(giocatori[i].pid) {
            case -1:
                TEST_ERROR;
                exit(EXIT_FAILURE);
            case 0:
                execve("./giocatore", param_giocatori, NULL);
                TEST_ERROR;
                exit(EXIT_FAILURE);
        }
    }
}