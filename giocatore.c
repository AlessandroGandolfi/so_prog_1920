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
#include "config.h"

void initPedine(int, int, int);
void piazzaPedina(int, int);
int checkPosPedine(int, int);
void initObiettivi(int);

/* globali */
ped *mc_ped_squadra;
int *mc_sem_scac;
int mc_id_squadra, msg_id_coda, mc_id_scac;
char *mc_char_scac;

/* 
parametri a giocatore 
- path relativo file giocatore (per id processo htop)
- difficoltá gioco (per config)
- id token
- indice token squadra
- id mc sem scacchiera, array id set semafori
- id mc squadra, array pedine
- id mc char scacchiera
- id coda msg
*/
int main(int argc, char **argv) {
    int status, i;
    int token_gioc, pos_token, mc_id_sem, mc_id_band, num_band;

    getConfig(argv[1]);
    
    #if DEBUG
    testConfig();
    #endif

    srand(time(NULL) + getpid());
    
    token_gioc = atoi(argv[0]);
    pos_token = atoi(argv[1]);
    mc_id_sem = atoi(argv[2]);
    mc_id_squadra = atoi(argv[3]);
    msg_id_coda = atoi(argv[4]);
    mc_id_scac = atoi(argv[5]);

    /* collegamento a mem cond */
    mc_ped_squadra = (ped *) shmat(mc_id_squadra, NULL, 0);
    TEST_ERROR;
    
    mc_sem_scac = (int *) shmat(mc_id_sem, NULL, 0);
    TEST_ERROR;

    mc_char_scac = (char *) shmat(mc_id_scac, NULL, 0);
    TEST_ERROR;

    /* creazione pedine, valorizzazione pids_pedine */
    initPedine(token_gioc, pos_token, mc_id_sem);

    /* assegnazione obiettivi */
    initObiettivi(msg_id_coda);

    if(DEBUG) printf("gioc %d: messaggio fine band ricevuto\n", pos_token);

    /* attesa terminazione di tutte le pedine */
    while(wait(&status) > 0);
    TEST_ERROR;

    shmdt(mc_char_scac);
    TEST_ERROR;

    shmdt(mc_sem_scac);
    TEST_ERROR;

    shmdt(mc_ped_squadra);
    TEST_ERROR;

    exit(EXIT_SUCCESS);
}

void getConfig(char *mode) {
    FILE *fs;
    char *config_file;

    config_file = (char *) malloc(sizeof(char));
    strcpy(config_file, "./config/");
    strcat(config_file, mode);
    strcat(config_file, ".txt");

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
        fscanf(fs, "%d%*[^\0]", &DIST_PED_GIOC);
    } else {
        printf("Errore apertura file di configurazione\n");
        exit(0);
    }

    fclose(fs);
}

void initPedine(int token_gioc, int pos_token, int mc_id_sem) {
    int i;
    char *param_pedine[6];
    char tmp_params[5][sizeof(char *)];
    struct sembuf sops;
    pid_t *pids_pedine;
    msg_fine_piaz avviso_master;

    pids_pedine = (pid_t *) calloc(SO_NUM_P, sizeof(pid_t));

    /* 
    parametri a pedine
    - id token
    - indice token squadra
    - id mc scacchiera, array id set semafori
    - id mc squadra, array pedine
    - indice identificativo pedina dell'array in mc squadra
    */
    sprintf(tmp_params[0], "%d", token_gioc);
    param_pedine[0] = tmp_params[0];
    sprintf(tmp_params[1], "%d", pos_token);
    param_pedine[1] = tmp_params[1];
    sprintf(tmp_params[2], "%d", mc_id_sem);
    param_pedine[2] = tmp_params[2];
    sprintf(tmp_params[3], "%d", mc_id_squadra);
    param_pedine[3] = tmp_params[3];
    param_pedine[5] = NULL;
    
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

        if(DEBUG) printf("gioc %d: ped: %d piazzata\n", pos_token, i);

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
            if(DEBUG) printf("gioc %d (%ld): ult ped %d, msg fine piazzam su coda %d\n", pos_token, (long) getpid(), i, msg_id_coda);
        } else {
            sops.sem_num = (pos_token == (SO_NUM_G - 1)) ? 0 : pos_token + 1;
            sops.sem_op = 1;
            sops.sem_flg = 0;
            semop(token_gioc, &sops, 1);
        }

        /* creazione proc pedine */
        pids_pedine[i] = fork();

        switch(pids_pedine[i]) {
            case -1:
                TEST_ERROR;
                exit(EXIT_FAILURE);
            case 0:
                /* passo indice ciclo a pedina per accesso diretto a propria struttura in array */
                sprintf(tmp_params[4], "%d", i);
                param_pedine[4] = tmp_params[4];
                execv("./pedina", param_pedine);
                TEST_ERROR;
                exit(EXIT_FAILURE);
        }
    }
}

void piazzaPedina(int ind_pedine, int pos_token) {
    int riga, colonna;
    struct sembuf sops;
    
    /* 
    randomizzo i valori fino a quando non trovo 
    una cella libera senza altre pedine "troppo vicine" 
    */
    do {
        do {
            riga = rand() % SO_ALTEZZA;
            colonna = rand() % SO_BASE;
        } while(checkPosPedine(riga, colonna));

        sops.sem_num = colonna;
        sops.sem_op = -1;
        sops.sem_flg = IPC_NOWAIT;
    } while(semop(mc_sem_scac[riga], &sops, 1) == -1);

    /* valorizzazione array ped in mc */
    mc_ped_squadra[ind_pedine].mosse_rim = SO_N_MOVES;
    mc_ped_squadra[ind_pedine].pos_attuale.x = colonna;
    mc_ped_squadra[ind_pedine].pos_attuale.y = riga;

    /* piazzamento pedine su char scacchiera */
    mc_char_scac[(riga * SO_BASE) + colonna] = (pos_token + 1) + '0';
}

int checkPosPedine(int riga, int colonna) {
    int i;

    /* 
    ciclo su array fino a quando non trovo band con coord -1, -1 (da lí in poi non ancora piazzate) 
    o se ne trovo una giá piazzata troppo vicina 
    */
    for(i = 0; i < SO_NUM_P && (mc_ped_squadra[i].pos_attuale.x != -1 && mc_ped_squadra[i].pos_attuale.y != -1); i++)
        if(calcDist(colonna, mc_ped_squadra[i].pos_attuale.x, riga, mc_ped_squadra[i].pos_attuale.y) < DIST_PED_GIOC) 
            return TRUE;

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

void initObiettivi(int msg_id_coda) {
    msg_band msg;

    /* ricezione messaggio con id di mc con bandiere */
    msgrcv(msg_id_coda, &msg, sizeof(msg_band) - sizeof(long), (long) getppid(), 0);
    TEST_ERROR;
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
    if(SO_NUM_G)
        printf("gioc: valori configurazione correttamente estratti\n");
    else {
        printf("gioc: errore valorizzazione configurazione\n");
        exit(EXIT_FAILURE);
    }
}
#endif