/*
passaggi inizio primo round:
gioc
    master
        pedine

        - pedina bloccata su msgrcv da giocatore
- ultimo giocatore a piazzare invia msg a master
- giocatori si mettono in attesa di messaggio con id di bandiere in mc
    - master riceve via libera da giocatori per piazzare bandiere
    - tutti token a 1
    - allocazione array bandiere
    - invio SO_NUM_G msg con id mc bandiere a giocatori
- giocatori ricezione msg da master
- msg a ogni pedina assegnazione percorso a pedine
        - pedina riceve messaggio (ha obiettivo)
        - calcolo percorso
        - pedine wait for 0 su token squadra ad avvio
- msg fine assegnazione obiettivi a master
    - a messaggio di ultimo gioc per obiettivi setto tutti token a 0
        - avvio movimenti
*/

#include ".\config.h"

void initGiocatori();
void initScacchiera();
void somebodyTookMaShmget();
void stampaScacchiera();
void initBandiere();
int checkPosBandiere(int, int);

/* globali */
gioc giocatori[SO_NUM_G];
int *mc_sem_scac;
band *mc_bandiere;
int token_gioc, mc_id_scac, mc_id_band, msg_id_coda, num_band;

int main(int argc, char **argv) {
    int status, i;
    msg_fine_piaz msg;

    srand(time(NULL) + getpid());

    /* creazione coda msg */
    msg_id_coda = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600);
    TEST_ERROR;

    /* creazione mc scacchiera */
    mc_id_scac = shmget(IPC_PRIVATE, sizeof(int) * SO_ALTEZZA, S_IRUSR | S_IWUSR);
    TEST_ERROR;

    /* collegamento a mc scac */
    mc_sem_scac = (int *) shmat(mc_id_scac, NULL, 0);
    TEST_ERROR;

    initScacchiera();

    /* creazione giocatori, valorizzazione pids_giocatori */
    initGiocatori();

    /* master aspetta che i 4 giocatori abbiano piazzato le pedine */
    for(i = 0; i < SO_NUM_G; i++)
        msgrcv(msg_id_coda, &msg, sizeof(msg_fine_piaz) - sizeof(long), (long) giocatori[i].pid, 0);
    TEST_ERROR;

    /* creazione bandiere, valorizzazione array bandiere */
    initBandiere();

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
    union semun sem_arg;
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
    
    memset(scacchiera, '0', SO_ALTEZZA * SO_BASE * sizeof(char));

    for(i = 0; i < num_band; i++)
        scacchiera[mc_bandiere[i].pos_band.y][mc_bandiere[i].pos_band.x] = 'B';

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

    /* se array band ne ha di non prese, le metto in matrice */

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
                    case 'B':
                        printf("\033[0;33m");
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

void initGiocatori() {
    int i;
    char *param_giocatori[5];
    char tmp_params[5][sizeof(char *)];
    union semun sem_arg;
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
    - id coda msg
    */

    sprintf(tmp_params[0], "%d", token_gioc);
    param_giocatori[0] = tmp_params[0];
    sprintf(tmp_params[2], "%d", mc_id_scac);
    param_giocatori[2] = tmp_params[2];
    sprintf(tmp_params[4], "%d", msg_id_coda);
    param_giocatori[4] = tmp_params[4];
    param_giocatori[5] = NULL;


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
                execve(".\giocatore", param_giocatori, NULL);
                TEST_ERROR;
                exit(EXIT_FAILURE);
        }
    }
}

void initBandiere() {
    int i, riga, colonna, sem_val;
    msg_band msg_new_band;

#ifdef EASY
    num_band = SO_FLAG_MAX;
#else
    num_band = (rand() % (SO_FLAG_MAX - SO_FLAG_MIN)) + SO_FLAG_MIN;
#endif

    mc_id_band = shmget(IPC_PRIVATE, num_band * sizeof(band), S_IRUSR | S_IWUSR);
    TEST_ERROR;

    mc_bandiere = (band *) shmat(mc_id_band, NULL, 0);
    TEST_ERROR;

    for(i = 0; i < num_band; i++) {
        mc_bandiere[i].pos_band.x = -1;
        mc_bandiere[i].pos_band.y = -1;
    }

    for(i = 0; i < num_band; i++) {
        do {
            do {
                riga = rand() % SO_ALTEZZA;
                colonna = rand() % SO_BASE;
            } while(checkPosBandiere(riga, colonna));

            sem_val = semctl(mc_sem_scac[riga], colonna, GETVAL, NULL);
        } while(sem_val == 0);

        mc_bandiere[i].pos_band.y = riga;
        mc_bandiere[i].pos_band.x = colonna;
    }

    msg_new_band.ind = mc_id_band;
    msg_new_band.mtype = getpid();
    /* manda un messaggio per giocatore */
    for(i = 0; i < SO_NUM_G; i++) {
        msgsnd(msg_id_coda, &msg_new_band, sizeof(msg_band) - sizeof(long), 0);
        TEST_ERROR;
    }
}

int checkPosBandiere(int riga, int colonna) {
    int i, check;

    i = 0;
    check = 0;

    while(mc_bandiere[i].pos_band.x != -1 && mc_bandiere[i].pos_band.y != -1 && i < num_band && check == 0) {
        if(calcDist(colonna, mc_bandiere[i].pos_band.x, riga, mc_bandiere[i].pos_band.y) < DIST_BAND) check++;
        i++;
    }

    return check;
}