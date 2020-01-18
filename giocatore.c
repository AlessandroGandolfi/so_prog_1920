/*
il giocatore cerca di conquistare le pedine che
    valgono di piú
    sono piú vicine
pedine distribuite con spazio tra di esse
le indicazioni del giocatore consistono solo nel dare l'obiettivo da raggiungere alle pedine
nel caso venga segnalato che una bandierina é stata presa il giocatore
    controlla che abbia assegnato delle pedine a quella bandiera e chi l'ha presa
        se nemico puó far continuare avanzamento delle pedine
        se propria pedina assegna nuovo obiettivo
*/
#include "header.h"

void initPedine(int, int, char *);
void piazzaPedina(int, int);
int checkPosPedine(coord);
void initObiettivi(int);

/* globali */
ped *mc_ped_squadra;
char *mc_char_scac;
int mc_id_squadra, msg_id_coda, mc_id_scac, sem_id_scac;

/* 
parametri a giocatore
0 - path relativo file giocatore
1 - difficoltá gioco (per config)
2 - id token
3 - indice token squadra
4 - id sem scacchiera
5 - id mc squadra, array pedine
6 - id coda msg
7 - id mc char scacchiera
*/
int main(int argc, char **argv) {
    int status, i;
    int token_gioc, pos_token, mc_id_band, num_band;

    getConfig(argv[1]);

    srand(time(NULL) + getpid());

    token_gioc = atoi(argv[2]);
    pos_token = atoi(argv[3]);
    sem_id_scac = atoi(argv[4]);
    mc_id_squadra = atoi(argv[5]);
    msg_id_coda = atoi(argv[6]);
    mc_id_scac = atoi(argv[7]);

    /* collegamento a mem cond */
    mc_ped_squadra = (ped *) shmat(mc_id_squadra, NULL, 0);
    TEST_ERROR;

    mc_char_scac = (char *) shmat(mc_id_scac, NULL, 0);
    TEST_ERROR;

    /* creazione pedine, valorizzazione pids_pedine */
    initPedine(token_gioc, pos_token, argv[1]);

    /* assegnazione obiettivi */
    initObiettivi(msg_id_coda);

    #if DEBUG
    printf("gioc %d: messaggio fine band ricevuto\n", pos_token);
    #endif

    /* attesa terminazione di tutte le pedine */
    while(wait(&status) > 0);
    TEST_ERROR;

    shmdt(mc_char_scac);
    TEST_ERROR;

    shmdt(mc_ped_squadra);
    TEST_ERROR;

    exit(EXIT_SUCCESS);
}

/* 
valorizzazione globali da file secondo modalitá
param: 
- modalitá selezionata
*/
void getConfig(char *mode) {
    FILE *fs;
    char *config_file;

    config_file = (char *) malloc(sizeof(char));
    strcpy(config_file, "./config/");
    strcat(config_file, mode);
    strcat(config_file, ".txt");
    
    #if DEBUG
    printf("path file conf: %s\n", config_file);
    #endif

    fs = fopen(config_file, "r");

    if(fs) {
        fscanf(fs, "%d%*[^\n]", &SO_NUM_G);
        fscanf(fs, "%d%*[^\n]", &SO_NUM_P);
        fscanf(fs, "%d%*[^\n]", &SO_MAX_TIME);
        fscanf(fs, "%d%*[^\n]", &SO_BASE);
        fscanf(fs, "%d%*[^\n]", &SO_ALTEZZA);
        fscanf(fs, "%d%*[^\n]", &SO_FLAG_MIN);
        fscanf(fs, "%d%*[^\n]", &SO_FLAG_MAX);
        fscanf(fs, "%d%*[^\n]", &SO_ROUND_SCORE);
        fscanf(fs, "%d%*[^\n]", &SO_N_MOVES);
        fscanf(fs, "%d%*[^\n]", &SO_MIN_HOLD_NSEC);
        fscanf(fs, "%d%*[^\n]", &DIST_PED);
        fscanf(fs, "%d%*[^\0]", &DIST_BAND);
    } else {
        printf("Errore apertura file di configurazione\n");
        exit(0);
    }

    fclose(fs);
}

/*
creazione e posizionamento pedine, sincronizzazione con token
param:
- id token sem
- posizione token squadra
- modalitá selezionata
*/
void initPedine(int token_gioc, int pos_token, char *mode) {
    int i;
    char *param_pedine[8];
    char tmp_params[5][sizeof(char *)];
    struct sembuf sops;
    pid_t *pids_pedine;
    msg_fine_piaz avviso_master;

    pids_pedine = (pid_t *) calloc(SO_NUM_P, sizeof(pid_t));

    /* 
    parametri a pedine
    0 - path relativo file pedina
    1 - difficoltá gioco (per config)
    2 - id token
    3 - indice token squadra
    4 - id mc scacchiera, array id set semafori
    5 - id mc squadra, array pedine
    6 - indice identificativo pedina dell'array in mc squadra
    */
    param_pedine[0] = "./pedina";
    param_pedine[1] = mode;
    sprintf(tmp_params[0], "%d", token_gioc);
    param_pedine[2] = tmp_params[0];
    sprintf(tmp_params[1], "%d", pos_token);
    param_pedine[3] = tmp_params[1];
    sprintf(tmp_params[2], "%d", sem_id_scac);
    param_pedine[4] = tmp_params[2];
    sprintf(tmp_params[3], "%d", mc_id_squadra);
    param_pedine[5] = tmp_params[3];
    param_pedine[7] = NULL;
    
    /* pedine ancora non piazzate con coord -1, -1 */
    for(i = 0; i < SO_NUM_P; i++) {
        mc_ped_squadra[i].pos_attuale.x = -1; 
        mc_ped_squadra[i].pos_attuale.y = -1;
    }

    for(i = 0; i < SO_NUM_P; i++) {
        /* token per piazzamento pedine */
        sops.sem_num = pos_token;
        sops.sem_op = -1;
        sops.sem_flg = 0;
        semop(token_gioc, &sops, 1);

        #if DEBUG
        printf("gioc %d: ped %d piazzata\n", pos_token, i);
        #endif

        piazzaPedina(i, pos_token);
        
        /* 
        ultimo giocatore che piazza una pedina manda un messaggio al master
        che piazzerá a sua volta le bandierine, non rilascia primo token
        */
        if(i == (SO_NUM_P - 1) && pos_token == (SO_NUM_G - 1)) { 
        	avviso_master.mtype = (long) getpid();
            avviso_master.fine_piaz = 10001;
            msgsnd(msg_id_coda, &avviso_master, sizeof(msg_fine_piaz) - sizeof(long), 0);
            TEST_ERROR;

            #if DEBUG
            printf("gioc %d (%ld): ult ped %d, msg fine piazzam su coda %d\n", pos_token, (long) getpid(), i, msg_id_coda);
            #endif
        } else {
            sops.sem_num = (pos_token == (SO_NUM_G - 1)) ? 0 : pos_token + 1;
            sops.sem_op = 1;
            sops.sem_flg = 0;
            semop(token_gioc, &sops, 1);
            TEST_ERROR;
        }

        /* passo indice ciclo a pedina per accesso diretto a propria struttura in array */
        sprintf(tmp_params[4], "%d", i);
        param_pedine[6] = tmp_params[4];

        /* creazione proc pedine */
        pids_pedine[i] = fork();

        switch(pids_pedine[i]) {
            case -1:
                TEST_ERROR;
                exit(EXIT_FAILURE);
            case 0:
                execv("./pedina", param_pedine);
                TEST_ERROR;
                exit(EXIT_FAILURE);
        }
    }
}

/*
piazzamento nuove bandiere e distribuzione loro punteggio
param:
- numero identificativo pedina di array
- posizione id token giocatori/squadra
*/
void piazzaPedina(int ind_pedine, int pos_token) {
    coord casella;
    struct sembuf sops;
    
    /* 
    randomizzo i valori fino a quando non trovo 
    una cella libera senza altre pedine "troppo vicine" 
    */
    do {
        casella.y = rand() % SO_ALTEZZA;
        casella.x = rand() % SO_BASE;
    } while(!checkPosPedine(casella));

    sops.sem_num = INDEX(casella);
    sops.sem_op = -1;
    sops.sem_flg = 0;
    semop(sem_id_scac, &sops, 1);
    TEST_ERROR;

    /* valorizzazione array ped in mc */
    mc_ped_squadra[ind_pedine].mosse_rim = SO_N_MOVES;
    mc_ped_squadra[ind_pedine].pos_attuale = casella;

    /* piazzamento pedine su char scacchiera */
    mc_char_scac[INDEX(casella)] = (pos_token + 1) + '0';
}

/*
controlli per piazzamento nuova pedina su posizione delle 
pedine giá piazzate e su cella libera
TRUE se supera controlli, FALSE altrimenti
param:
- coordinate generate
*/
int checkPosPedine(coord casella) {
    int i;

    /* 
    ciclo su array fino a quando non trovo band con coord -1, -1 (da lí in poi non ancora piazzate) 
    o se ne trovo una giá piazzata troppo vicina 
    */
    for(i = 0; i < SO_NUM_P && (mc_ped_squadra[i].pos_attuale.x != -1 && mc_ped_squadra[i].pos_attuale.y != -1); i++)
        if(calcDist(casella, mc_ped_squadra[i].pos_attuale) < DIST_PED
            || mc_char_scac[INDEX(casella)] != '0') 
            return FALSE;

    return TRUE;
}

/*
calcolo della distanza tra 2 caselle della scacchiera
param:
- coord prima casella
- coord seconda casella
*/
int calcDist(coord cas1, coord cas2) {
    int distanza, dif_riga, dif_col, dif_min, dif_max;

    /* differenza riga e colonna caselle */
    dif_riga = abs(cas1.y - cas2.y);
    dif_col = abs(cas1.x - cas2.x);
    
    dif_min = (dif_col <= dif_riga) ? dif_col : dif_riga;
    dif_max = (dif_col > dif_riga) ? dif_col : dif_riga;

    /* 
    distanza finale = sqrt(2) * passi diagonali + passi "rettilinei"
    passi in diagonale = dif_min
    passi "rettilinei" = (dif_max - dif_min) 
    */
    distanza = ((int) sqrt(2)) * dif_min + (dif_max - dif_min);

    return distanza;
}

/* TODO */
void initObiettivi(int msg_id_coda) {
    msg_band msg;

    /* ricezione messaggio con id di mc con bandiere */
    msgrcv(msg_id_coda, &msg, sizeof(msg_band) - sizeof(long), (long) getppid(), 0);
    TEST_ERROR;
}

#if DEBUG
void testConfig() {
    if(SO_NUM_G)
        printf("gioc: valori configurazione correttamente estratti\n");
    else {
        printf("gioc: errore valorizzazione configurazione\n");
        exit(EXIT_FAILURE);
    }
}
#endif