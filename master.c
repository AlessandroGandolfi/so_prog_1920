/*
passaggi inizio primo round:
gioc
    master
        pedine

        - pedina bloccata su msgrcv da giocatore 
- ultimo giocatore a piazzare invia msg a master X
- giocatori si mettono in attesa di messaggio con id di bandiere in mc X
    - master riceve via libera da giocatori per piazzare bandiere X
    - tutti token a 1 X
    - allocazione array bandiere e calcolo punteggio singola bandiera X
    - invio SO_NUM_G msg con id mc bandiere a giocatori X
- giocatori ricezione msg da master X
- msg a ogni pedina dopo assegnazione percorso a pedine (ciclo su pids_pedine)
        - pedina riceve messaggio (ha obiettivo)
        - calcolo percorso
        - pedine wait for 0 su token squadra ad avvio
- msg fine assegnazione obiettivi a master
    - a messaggio di ultimo gioc per obiettivi setto tutti token a 0
        - avvio movimenti
-------------------------------------------------------------------------
posso settare percorso a pedina una per volta
giocatore:
    for pedina
        assegna obiettivo
        manda messaggio a pedina singola per obiettivo
        attesa messaggio da pedina per calcolo percorso
    manda messaggio a master fine calcolo percorso
pedina: 
    attesa ricezione messaggio obiettivo
    calcolo percorso
    invio messaggio giocatore per fine percorso
    attesa 0
*/

#include "config.h"

void checkMode(int, char *);
void initRisorse();
int initGiocatori();
void initSemScacchiera();
void somebodyTookMaShmget(int);
void stampaScacchiera();
void initBandiere(int);
int checkPosBandiere(int, int, int);

/* globali */
gioc *giocatori;
char *mc_char_scac;
int *mc_sem_scac;
band *mc_bandiere;
int mc_id_sem, mc_id_scac, mc_id_band, msg_id_coda;

int main(int argc, char **argv) {
    int status, i, token_gioc;
    msg_fine_piaz msg;

    checkMode(argc, argv[1]);
    
    #if DEBUG
    testConfig();
    #endif

    srand(time(NULL) + getpid());

    initRisorse();

    if(DEBUG) printf("master: fine init risorse\n");

    initSemScacchiera();

    if(DEBUG) printf("master: fine init scacchiera\n");

    /* creazione giocatori, valorizzazione pids_giocatori */
    token_gioc = initGiocatori();

    /* master aspetta che l'ultimo giocatore abbia piazzato l'ultima pedina */
    if(DEBUG) printf("master: attesa msg ultimo piazzam da %ld, id coda %d\n", (long) giocatori[SO_NUM_G - 1].pid, msg_id_coda);
    /* msgrcv(msg_id_coda, &msg, sizeof(msg_fine_piaz) - sizeof(long), (long) giocatori[SO_NUM_G - 1].pid, 0); */
    TEST_ERROR;

    if(DEBUG) printf("master: val msg ricevuto %d\n", msg.fine_piaz);

    /* creazione bandiere, valorizzazione array bandiere */
    initBandiere(token_gioc);

    if(DEBUG) {
        sleep(1);
        stampaScacchiera();
    }

    /* attesa terminazione di tutti i giocatori */
    while(wait(&status) > 0);
    TEST_ERROR;

    /* detach e rm mc, sem, msg */
    somebodyTookMaShmget(token_gioc);

    return 0;
}

void checkMode(int argc, char *mode) {
    if(argc < 2 || (strcmp(mode, "easy") != 0 && strcmp(mode, "hard") != 0
    #if DEBUG
                    && strcmp(mode, "debug") != 0
    #endif
    )) {
        printf("Specificare \"easy\" o \"hard\" per avviare il progetto\n");
        exit(0);
    } else {
        getConfig(mode);
    }
}

void getConfig(char *mode) {
    FILE *fs;
    char *config_file;
    char config_value[sizeof(int)];

    // config_file = (char *) malloc(sizeof(char));
    config_file = "./config/";

    strcat(config_file, mode);
    strcat(config_file, ".txt");
    
    if(DEBUG) printf("path file conf: %s\n", config_file);

    fs = fopen(config_file, "r");

    if(fs) {
        fscanf(fs, "%s%*[^\n]", config_value);
        SO_NUM_G = atoi(config_value);
        strcpy(config_value, "");

        fscanf(fs, "%s%*[^\n]", config_value);
        SO_NUM_P = atoi(config_value);
        strcpy(config_value, "");

        fscanf(fs, "%s%*[^\n]", config_value);
        SO_MAX_TIME = atoi(config_value);
        strcpy(config_value, "");

        fscanf(fs, "%s%*[^\n]", config_value);
        SO_BASE = atoi(config_value);
        strcpy(config_value, "");

        fscanf(fs, "%s%*[^\n]", config_value);
        SO_ALTEZZA = atoi(config_value);
        strcpy(config_value, "");

        fscanf(fs, "%s%*[^\n]", config_value);
        SO_FLAG_MIN = atoi(config_value);
        strcpy(config_value, "");

        fscanf(fs, "%s%*[^\n]", config_value);
        SO_FLAG_MAX = atoi(config_value);
        strcpy(config_value, "");

        fscanf(fs, "%s%*[^\n]", config_value);
        SO_ROUND_SCORE = atoi(config_value);
        strcpy(config_value, "");

        fscanf(fs, "%s%*[^\n]", config_value);
        SO_N_MOVES = atoi(config_value);
        strcpy(config_value, "");

        fscanf(fs, "%s%*[^\n]", config_value);
        SO_MIN_HOLD_NSEC = atoi(config_value);
        strcpy(config_value, "");

        fscanf(fs, "%s%*[^\n]", config_value);
        DIST_PED_GIOC = atoi(config_value);
    } else {
        printf("Errore apertura file di configurazione\n");
        exit(0);
    }

    fclose(fs);
}

void initSemScacchiera() {
    int i;
    semun sem_arg;

    sem_arg.array = (unsigned short *) calloc(SO_BASE, sizeof(unsigned short));
    
    /* memset cambia 1 a 127 per cast da unsigned short a int */
    for(i = 0; i < SO_BASE; i++) sem_arg.array[i] = 1; 

    for(i = 0; i < SO_ALTEZZA; i++) {
        mc_sem_scac[i] = semget(IPC_PRIVATE, SO_BASE, 0600);
        TEST_ERROR;

        semctl(mc_sem_scac[i], 0, SETALL, sem_arg);
        TEST_ERROR;
    }
}

void somebodyTookMaShmget(int token_gioc) {
    int i;

    for(i = 0; i < SO_ALTEZZA; i++) {
        semctl(mc_sem_scac[i], 0, IPC_RMID);
        TEST_ERROR;
    }

    for(i = 0; i < SO_NUM_G; i++) {
        shmctl(giocatori[i].mc_id_squadra, IPC_RMID, NULL);
        TEST_ERROR;
    }
    
    shmdt(mc_bandiere);
    TEST_ERROR;
	shmctl(mc_id_band, IPC_RMID, NULL);
    TEST_ERROR;

    shmdt(mc_sem_scac);
    TEST_ERROR;
	shmctl(mc_id_sem, IPC_RMID, NULL);
    TEST_ERROR;

    shmdt(mc_char_scac);
    TEST_ERROR;
	shmctl(mc_id_scac, IPC_RMID, NULL);
    TEST_ERROR;

    semctl(token_gioc, 0, IPC_RMID);
    TEST_ERROR;

    msgctl(msg_id_coda, IPC_RMID, NULL);
    TEST_ERROR;
}

void stampaScacchiera() {
    int i, j;
    ped *mc_ped_squadra;

    if(DEBUG) printf("master: inizio calcolo mosse rimanenti\n");

    /* mosse rimanenti */
    for(i = 0; i < SO_NUM_G; i++) {
        giocatori[i].tot_mosse_rim = 0;
        
        mc_ped_squadra = (ped *) shmat(giocatori[i].mc_id_squadra, NULL, 0);
        TEST_ERROR;

        for(j = 0; j < SO_NUM_P; j++) {
            giocatori[i].tot_mosse_rim += mc_ped_squadra[j].mosse_rim;
        }

        shmdt(mc_ped_squadra);
        TEST_ERROR;
    }

    if(DEBUG) printf("master: fine calcolo mosse rimanenti\n");

    /* stampa matrice caratteri */
    for(i = 0; i < (SO_ALTEZZA * SO_BASE); i++) {
        if(ENABLE_COLORS) {
            switch(mc_char_scac[i]) {
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
        printf("%c", mc_char_scac[i]);
        if(!((i + 1) % SO_BASE)) printf("\n");
    }

    if(DEBUG) printf("master: fine stampa scacchiera\n");
    
    if(ENABLE_COLORS) printf("\033[0m");

    for(i = 0; i < SO_NUM_G; i++) 
        printf("Punteggio giocatore %d: %d, %d mosse totali rimanenti\n", (i + 1), giocatori[i].punteggio, giocatori[i].tot_mosse_rim);
}

int initGiocatori() {
    int i, token_gioc;
    char *param_giocatori[6];
    char tmp_params[5][sizeof(char *)];
    semun sem_arg;

    sem_arg.array = (unsigned short *) calloc(SO_BASE, sizeof(unsigned short));

    /* init token posizionamento pedine */
    for(i = 0; i < SO_NUM_G; i++) sem_arg.array[i] = i ? 0 : 1;
    
    token_gioc = semget(IPC_PRIVATE, SO_NUM_G, 0600);
    TEST_ERROR;
    semctl(token_gioc, 0, SETALL, sem_arg);
    TEST_ERROR;

    /* 
    parametri a giocatore 
    - id token
    - indice token squadra
    - id mc sem scacchiera, array id set semafori
    - id mc squadra, array pedine
    - id mc char scacchiera
    - id coda msg
    */

    // aggiungere mode
    sprintf(tmp_params[0], "%d", token_gioc);
    param_giocatori[0] = tmp_params[0];
    sprintf(tmp_params[2], "%d", mc_id_sem);
    param_giocatori[2] = tmp_params[2];
    sprintf(tmp_params[4], "%d", msg_id_coda);
    param_giocatori[4] = tmp_params[4];
    sprintf(tmp_params[5], "%d", mc_id_scac);
    param_giocatori[5] = tmp_params[5];
    param_giocatori[6] = NULL;

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
                execv("./giocatore", param_giocatori);
                TEST_ERROR;
                exit(EXIT_FAILURE);
        }
    }

    if(DEBUG) printf("master: fine creazione giocatori\n");

    return token_gioc;
}

void initBandiere(int token_gioc) {
    int i, riga, colonna, sem_val, tot_punti_rim, num_band;
    msg_band msg_new_band;
    semun sem_arg;

    tot_punti_rim = SO_ROUND_SCORE;
    num_band = (rand() % (SO_FLAG_MAX - SO_FLAG_MIN + 1)) + SO_FLAG_MIN;

    if(DEBUG) num_band = (SO_NUM_G == 2) ? 5 : 40;

    printf("%d bandiere piazzate\n", num_band);

    mc_id_band = shmget(IPC_PRIVATE, num_band * sizeof(band), S_IRUSR | S_IWUSR);
    TEST_ERROR;

    /* creazione array bandiere in mc */
    mc_bandiere = (band *) shmat(mc_id_band, NULL, 0);
    TEST_ERROR;

    /* bandiere ancora non piazzate con coord -1, -1 */
    for(i = 0; i < num_band; i++) {
        mc_bandiere[i].pos_band.x = -1;
        mc_bandiere[i].pos_band.y = -1;
    }

    /* 
    per ogni bandiera randomizzo i valori fino a quando non trovo 
    una cella libera senza altre bandiere "troppo vicine" 
    */
    for(i = 0; i < num_band; i++) {
        do {
            do {
                riga = rand() % SO_ALTEZZA;
                colonna = rand() % SO_BASE;
            } while(checkPosBandiere(riga, colonna, num_band));

            sem_val = semctl(mc_sem_scac[riga], colonna, GETVAL, NULL);
        } while(!sem_val);

        /* valorizzazione mc bandiere */
        mc_bandiere[i].pos_band.y = riga;
        mc_bandiere[i].pos_band.x = colonna;
        mc_bandiere[i].presa = 0;
        if(i != (num_band - 1))
            mc_bandiere[i].punti = rand() % ((int) tot_punti_rim / 2);
        else
            mc_bandiere[i].punti = tot_punti_rim;
        tot_punti_rim -= mc_bandiere[i].punti;

        /* piazzamento bandiere su scacchiera */
        mc_char_scac[(riga * SO_BASE) + colonna] = 'B';
    }

    #if DEBUG
    testSemToken(token_gioc);
    #endif

    /* 
    setto token a 1 per pedine in wait for 0 successivamente
    da decrementare a inizio round per inizio movimenti
    */
    sem_arg.array = (unsigned short *) calloc(SO_NUM_G, sizeof(unsigned short));
    for(i = 0; i < SO_NUM_G; i++) sem_arg.array[i] = 1;
    semctl(token_gioc, 0, SETALL, sem_arg);
    free(sem_arg.array);

    #if DEBUG
    testSemToken(token_gioc);
    #endif

    msg_new_band.ind = mc_id_band;
    msg_new_band.mtype = (long) getpid();
    /* manda un messaggio per giocatore */
    for(i = 0; i < SO_NUM_G; i++) {
        msgsnd(msg_id_coda, &msg_new_band, sizeof(msg_band) - sizeof(long), 0);
        TEST_ERROR;
    }
}

int checkPosBandiere(int riga, int colonna, int num_band) {
    int i;

    /* 
    ciclo su array fino a quando non trovo band con coord -1, -1 (da lí in poi non ancora piazzate) 
    o se ne trovo una giá piazzata troppo vicina 
    */
    for(i = 0; i < num_band, mc_bandiere[i].pos_band.x != -1 && mc_bandiere[i].pos_band.y != -1; i++)
        if(calcDist(colonna, mc_bandiere[i].pos_band.x, riga, mc_bandiere[i].pos_band.y) < DIST_BAND) return TRUE;

    return FALSE;
}

int calcDist(int x1, int x2, int y1, int y2) {
    int distanza, dif_riga, dif_col, dif_min, dif_max;

    dif_riga = abs(y1 - y2);
    dif_col = abs(x1 - x2);
    
    dif_min = (dif_col <= dif_riga) ? dif_col : dif_riga;
    dif_max = (dif_col > dif_riga) ? dif_col : dif_riga;
    distanza = ((int) sqrt(2)) * dif_min + (dif_max - dif_min);

    return distanza;
}

void initRisorse() {
    /* creazione coda msg */
    msg_id_coda = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600);
    TEST_ERROR;

    /* creazione e collegamento a mc scacchiera */ 
    mc_id_scac = shmget(IPC_PRIVATE, sizeof(char) * SO_ALTEZZA * SO_BASE, S_IRUSR | S_IWUSR);
    TEST_ERROR;
    mc_char_scac = shmat(mc_id_scac, NULL, 0);
    TEST_ERROR;
    /* scacchiera tutta a 0, caratteri sovrascritti da semafori e pedine in futuro */
    memset(mc_char_scac, '0', sizeof(char) * SO_ALTEZZA * SO_BASE);

    /* creazione e collegamento a mc sem scacchiera */
    mc_id_sem = shmget(IPC_PRIVATE, sizeof(int) * SO_ALTEZZA, S_IRUSR | S_IWUSR);
    TEST_ERROR;
    mc_sem_scac = (int *) shmat(mc_id_sem, NULL, 0);
    TEST_ERROR;

    giocatori = (gioc *) calloc(SO_NUM_G, sizeof(gioc));
}

#if DEBUG
void testSemToken(int token_gioc) {
    unsigned short *val_array;
    int i;

    val_array = (unsigned short *) calloc(SO_NUM_G, sizeof(unsigned short));

    semctl(token_gioc, 0, GETALL, val_array);

    for(i = 0; i < SO_NUM_G; i++) {
        printf("master: token gioc %d settato a %hu\n", (i + 1), val_array[i]);
    }
}

void testConfig() {
    printf("val di SO_NUM_G: %d\n", SO_NUM_G);
}
#endif