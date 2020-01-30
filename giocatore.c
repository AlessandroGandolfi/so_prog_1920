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
void initObiettivi(int, int);
int assegnaObiettivo(coord, int);
void sqOrNem(int *, int *, int, coord, int);

/* globali */
ped *mc_ped_squadra;
char *mc_char_scac;
band *mc_bandiere;
int mc_id_squadra, msg_id_coda, mc_id_scac, sem_id_scac;
pid_t *pids_pedine;

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
    initObiettivi(msg_id_coda, pos_token);

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
    char *param_pedine[9];
    char tmp_params[6][sizeof(char *)];
    struct sembuf sops;
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
    7 - id coda msg
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
    sprintf(tmp_params[5], "%d", msg_id_coda);
    param_pedine[7] = tmp_params[5];
    param_pedine[8] = NULL;
    
    /* pedine ancora non piazzate con coord -1, -1 */
    for(i = 0; i < SO_NUM_P; i++) {
        mc_ped_squadra[i].pos_attuale.x = -1; 
        mc_ped_squadra[i].pos_attuale.y = -1;
        mc_ped_squadra[i].obiettivo = -1;
    }

    for(i = 0; i < SO_NUM_P; i++) {
        /* token per piazzamento pedine */
        sops.sem_num = pos_token;
        sops.sem_op = -1;
        sops.sem_flg = 0;
        semop(token_gioc, &sops, 1);

        #if DEBUG
        /* printf("gioc %d: ped %d piazzata\n", pos_token, i); */
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
void initObiettivi(int msg_id_coda, int pos_token) {
    msg_band msg_new_band;
    msg_new_obj msg_obj;
    int i, num_band, riga, range_scan, ped_sq, ped_nem;
    coord check;

    // --------------------------SPOSTARE--------------------------

    /* ricezione messaggio con id di mc con bandiere */
    msgrcv(msg_id_coda, &msg_new_band, sizeof(msg_band) - sizeof(long), (long) getppid(), 0);
    TEST_ERROR;

    #if DEBUG
    printf("gioc %d: messaggio fine band ricevuto\n", (pos_token + 1));
    #endif

    mc_bandiere = (band *) shmat(msg_new_band.ind, NULL, 0);

    // ----------------------------------------------------

    for(i = 0; i < msg_new_band.num_band; i++) {
        ped_sq = FALSE;
        ped_nem = FALSE;
        range_scan = 0;
        do {
            range_scan++;
            /* scan riga da -range a +range */
            for(riga = (range_scan * -1); riga <= range_scan; riga++) {
                check.y = mc_bandiere[i].pos_band.y;
                check.y += riga;
                if(check.y >= 0 && check.y < SO_ALTEZZA) {
                    /* scan da sx a centro */
                    check.x = mc_bandiere[i].pos_band.x;
                    check.x -= (range_scan - abs(riga));
                    if(check.x >= 0)
                        sqOrNem(&ped_sq, &ped_nem, pos_token, check, i);

                    /* da dopo centro a dx */
                    check.x +=  2 * (range_scan - abs(riga));
                    if(check.x < SO_BASE && (range_scan - abs(riga)) > 0)
                        sqOrNem(&ped_sq, &ped_nem, pos_token, check, i);
                }
            }
        } while(!ped_sq && !ped_nem);
    }

    for(i = 0; i < SO_NUM_P; i++) {
        #if DEBUG
        if(mc_ped_squadra[i].obiettivo != -1) {
            printf("gioc %d: band %d %d\n"
                , (pos_token + 1)
                , mc_bandiere[mc_ped_squadra[i].obiettivo].pos_band.y
                , mc_bandiere[mc_ped_squadra[i].obiettivo].pos_band.x);
        }
        #endif
            
        msg_obj.mc_id_band = msg_new_band.ind;
        msg_obj.band_assegnata = (mc_ped_squadra[i].obiettivo != -1) ? TRUE : FALSE;
        msg_obj.mtype = (long) pids_pedine[i];
        msgsnd(msg_id_coda, &msg_obj, sizeof(msg_new_obj) - sizeof(long), 0);
        TEST_ERROR;
    }

    // --------------------------SPOSTARE--------------------------
    shmdt(mc_bandiere);
}

void sqOrNem(int *ped_sq, int *ped_nem, int pos_token, coord check, int index) {
    if(mc_char_scac[INDEX(check)] == ((pos_token + 1) + '0'))
        *ped_sq = assegnaObiettivo(check, index);
    else if(mc_char_scac[INDEX(check)] != 'B' 
        && mc_char_scac[INDEX(check)] != '0'
        #if DEBUG
        && mc_char_scac[INDEX(check)] != '*'
        #endif
        )
        *ped_nem = TRUE;

    #if DEBUG
    if(mc_char_scac[INDEX(check)] == '0') 
        mc_char_scac[INDEX(check)] = '*';
    #endif
}

int assegnaObiettivo(coord pos_ped_sq, int ind_band) {
    int i;

    i = 0;

    while(calcDist(mc_ped_squadra[i].pos_attuale, pos_ped_sq)) i++;

    if(mc_ped_squadra[i].obiettivo != -1
        && mc_bandiere[ind_band].punti <= mc_bandiere[mc_ped_squadra[i].obiettivo].punti) 
        return FALSE;

    mc_ped_squadra[i].obiettivo = ind_band;

    return TRUE;
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