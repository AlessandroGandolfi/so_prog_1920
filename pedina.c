#include "header.h"

int waitObj();
int calcPercorso();
int muoviPedina(int);
void aggiornaStato();

/* globali */
ped *mc_ped_squadra;
char *mc_char_scac;
long pid_master;
int mc_id_squadra, msg_id_coda, mc_id_scac, sem_id_scac;
int ind_ped_sq, pos_token, token_gioc, ind_mossa;
coord *percorso;

/* 
parametri a pedine
0 - path relativo file pedina
1 - difficoltá gioco (per config)
2 - id token
3 - indice token squadra
4 - id sem scacchiera
5 - id mc squadra, array pedine
6 - indice identificativo pedina dell'array in mc squadra
7 - id coda msg
8 - pid master
9 - id mem cond scacchiera caratteri
*/
int main(int argc, char **argv) {
    token_gioc = atoi(argv[2]);
    pos_token = atoi(argv[3]);
    sem_id_scac = atoi(argv[4]);
    mc_id_squadra = atoi(argv[5]);
    ind_ped_sq = atoi(argv[6]);
    msg_id_coda = atoi(argv[7]);
    pid_master = atol(argv[8]);
    mc_id_scac = atoi(argv[9]);

    getConfig(argv[1]);

    signal(SIGUSR2, &signalHandler);
    TEST_ERROR;

    mc_ped_squadra = (ped *) shmat(mc_id_squadra, NULL, 0);
    mc_char_scac = (char *) shmat(mc_id_scac, NULL, 0);

    gestRound();

    return 0;
}

int waitObj() {
    msg_conf msg_obiettivo;

    errno = 0;

    /* ricezione messaggio con id di mc con bandiere */
    msgrcv(msg_id_coda, &msg_obiettivo, sizeof(msg_conf) - sizeof(long), (long) (getpid() + MSG_OBIETTIVO), 0);
    TEST_ERROR;

    #if DEBUG
    printf("ped %d: msg obiettivo %d ricevuto\n", (ind_ped_sq + 1), (mc_ped_squadra[ind_ped_sq].id_band));
    #endif
    
    /* solo quelle con obiettivo ricevono il messaggio */
    return calcPercorso();    
}

int calcPercorso() {
    int i, num_mosse, token_round;
    coord cont;
    msg_conf msg_fine_perc;

    errno = 0;

    /* 
    nel caso di bandierina o casella occupata nel corso del round
    elimino e rialloco array di mosse
    */
    if(percorso != NULL) free(percorso);
    num_mosse = calcDist(mc_ped_squadra[ind_ped_sq].pos_attuale, mc_ped_squadra[ind_ped_sq].obiettivo);
    percorso = (coord *) calloc(num_mosse, sizeof(coord));
    
    cont = mc_ped_squadra[ind_ped_sq].pos_attuale;

    /*calcolo il percorso della pedina*/
    for(i = 0; i < num_mosse; i++) {
        if((cont.x - mc_ped_squadra[ind_ped_sq].obiettivo.x) == 0) {
            /* mossa finale percorso */
            if((cont.y - mc_ped_squadra[ind_ped_sq].obiettivo.y) == 0)
                percorso[i] = mc_ped_squadra[ind_ped_sq].obiettivo;
            /* mossa in alto */
            else if((cont.y - mc_ped_squadra[ind_ped_sq].obiettivo.y) > 0) {
                percorso[i].x = cont.x;
                percorso[i].y = cont.y - 1;
            }
            /* mossa in basso */
            else {
                percorso[i].x = cont.x;
                percorso[i].y = cont.y + 1;
            }    
        }
        /* mossa a sinistra */
        else if((cont.x - mc_ped_squadra[ind_ped_sq].obiettivo.x) > 0) {
            percorso[i].x = cont.x - 1;
            percorso[i].y = cont.y;
        }
        /* mossa a destra */
        else {
            percorso[i].x = cont.x + 1;
            percorso[i].y = cont.y;
        }
        cont = percorso[i];
    }

    token_round = semctl(token_gioc, pos_token, GETVAL, 0);

    /* msg send fine calcolo percorso a giocatore prima di inizio round */
    if(token_round) {
        msg_fine_perc.mtype = (long) (getpid() + MSG_PERCORSO);
        msgsnd(msg_id_coda, &msg_fine_perc, sizeof(msg_conf) - sizeof(long), 0);
        TEST_ERROR;

        #if DEBUG
        /* printf("ped %d: msg fine calc percorso a gioc %d\n", (ind_ped_sq + 1), token_round); */
        #endif
    }

    return num_mosse;
}

int muoviPedina(int dim) {
    int band_presa;
    struct sembuf sops;
    struct timespec arg_sleep;

    band_presa = TRUE;

    /* mc_char_scac[INDEX(mc_ped_squadra[ind_ped_sq].pos_attuale)] = '0';*/

    for(ind_mossa = 0; ind_mossa < dim && band_presa; ind_mossa++) {
        /* prima di ogni mossa controlla dalla scacchiera che l'obiettivo non sia stato giá preso */
        if(mc_char_scac[INDEX(mc_ped_squadra[ind_ped_sq].obiettivo)] == 'B'
            #if DEBUG_BAND_EASY
            || mc_char_scac[INDEX(mc_ped_squadra[ind_ped_sq].obiettivo)] == (mc_ped_squadra[ind_ped_sq].id_band + 'A')
            #endif
        ) {
            sops.sem_num = INDEX(percorso[ind_mossa]);
            sops.sem_op = -1;
            sops.sem_flg = IPC_NOWAIT;

            /* prova ad eseguire mossa subito */
            if(semop(sem_id_scac, &sops, 1) == -1) {
                arg_sleep.tv_sec = 0;
                arg_sleep.tv_nsec = SO_MIN_HOLD_NSEC / 2;

                /* riprova dopo (SO_MIN_HOLD_NSEC / 2) se non riesce al primo tentativo */
                if(semtimedop(sem_id_scac, &sops, 1, &arg_sleep) == -1)
                    band_presa = FALSE; /* richiesta nuovo obiettivo */
                else aggiornaStato();

            } else aggiornaStato();
        } else band_presa = FALSE;  /* richiesta nuovo obiettivo se viene preso suo obiettivo */
    }

    /* mc_char_scac[INDEX(mc_ped_squadra[ind_ped_sq].pos_attuale)] = (pos_token + 1) + '0';*/

    return band_presa;
}

void aggiornaStato() {
    struct sembuf sops;
    struct timespec arg_sleep;

    errno = 0;

    mc_char_scac[INDEX(mc_ped_squadra[ind_ped_sq].pos_attuale)] = '0';

    /* libera risorsa precedente */
    sops.sem_num = INDEX(mc_ped_squadra[ind_ped_sq].pos_attuale);
    sops.sem_op = 1;
    sops.sem_flg = 0;
    semop(sem_id_scac, &sops, 1);
    TEST_ERROR;
    
    mc_ped_squadra[ind_ped_sq].pos_attuale = percorso[ind_mossa];
    mc_ped_squadra[ind_ped_sq].mosse_rim--;
    
    mc_char_scac[INDEX(mc_ped_squadra[ind_ped_sq].pos_attuale)] = (pos_token + 1) + '0';

    arg_sleep.tv_sec = 0;
    arg_sleep.tv_nsec = SO_MIN_HOLD_NSEC;
    nanosleep(&arg_sleep, NULL);
}

/* dist manhattan */
int calcDist(coord cas1, coord cas2) {
    return abs(cas1.x - cas2.x) + abs(cas1.y - cas2.y);
}

void getConfig(char *mode) {
    FILE *fs;
    char *config_file;

    config_file = (char *) malloc(sizeof(char)*10);
    strcpy(config_file, "./config/");
    strcat(config_file, mode);
    strcat(config_file, ".txt");
    
    #if DEBUG
    /* printf("path file conf: %s\n", config_file); */
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
        fscanf(fs, "%d%*[0]", &DIST_BAND);
    } else {
        printf("Errore apertura file di configurazione\n");
        exit(0);
    }

    fclose(fs);
}

void gestRound() {
    int num_mosse;
    struct sembuf sops;
    msg_band_presa msg_presa;

    do {
        num_mosse = waitObj();

        if(semctl(token_gioc, pos_token, GETVAL, 0))
            printf("ped %d gioc %d: ricevuto obiettivo %d\n", ind_ped_sq, (pos_token + 1), mc_ped_squadra[ind_ped_sq].id_band);

        /* bloccate fino a quando round non é in corso */
        sops.sem_num = pos_token;
        sops.sem_op = 0;
        sops.sem_flg = 0;
        semop(token_gioc, &sops, 1);

        if(muoviPedina(num_mosse)) {
            msg_presa.id_band = mc_ped_squadra[ind_ped_sq].id_band;
            msg_presa.pos_token = pos_token;
            msg_presa.mtype = pid_master + (long) MSG_BANDIERA;

            errno = 0;

            /* msg a master per bandiera presa */
            msgsnd(msg_id_coda, &msg_presa, sizeof(msg_band_presa) - sizeof(long), 0);
            TEST_ERROR;
        }
        
        /* richiesta di un nuovo obiettivo una volta che pedina é di nuovo ferma */
        kill(getppid(), SIGUSR1);
    } while(TRUE);
}

void signalHandler(int signal_number) {
    errno = 0;
    
    /*printf("ped gioc %d: %d %d\n", (pos_token + 1), mc_ped_squadra[ind_ped_sq].pos_attuale.y, mc_ped_squadra[ind_ped_sq].pos_attuale.x);*/

    signal(SIGUSR2, SIG_DFL);

    shmdt(mc_char_scac);
    TEST_ERROR;
    
    shmdt(mc_ped_squadra);
    TEST_ERROR;

    exit(EXIT_SUCCESS);
}