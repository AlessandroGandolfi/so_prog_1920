#include "header.h"

void initPedine(char *);
void piazzaPedina(int);
int checkPosPedine(coord);
void initObiettivi();
int assegnaObiettivo(coord, int, int);
void sqOrNem(int *, int *, coord, int, int, int);

/* globali */
ped *mc_ped_squadra;
char *mc_char_scac;
band *mc_bandiere;
int mc_id_squadra /* id mc array pedine della squadra */
    , msg_id_coda /* id della coda di messaggi */
    , mc_id_scac /* id mc dell'array di caratteri della scacchiera */
    , sem_id_scac /* id set di semafori per l'array della scacchiera */
    , mc_id_bandiere /* id mc delle bandiere del round */
    , num_band_round /* numero delle bandiere di un singolo round */
    , pos_token /* numero del giocatore */
    , token_gioc; /* id del semaforo usato per piazzare le pedine e per gestione del round */
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
    struct sigaction new_signal_handler;

    getConfig(argv[1]);

    srand(time(NULL) + getpid());

    new_signal_handler.sa_handler = &signalHandler;
    sigaction(SIGUSR1, &new_signal_handler, NULL);
    sigaction(SIGUSR2, &new_signal_handler, NULL);

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
    initPedine(argv[1]);

    gestRound();

    return 0;
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
void initPedine(char *mode) {
    int i;
    char *param_pedine[11];
    char tmp_params[8][sizeof(char *)];
    struct sembuf sops;
    msg_conf avviso_master;

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
    8 - pid master
    9 - id mem cond scacchiera caratteri
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
    sprintf(tmp_params[6], "%ld", (long) getppid());
    param_pedine[8] = tmp_params[6];
    sprintf(tmp_params[7], "%d", mc_id_scac);
    param_pedine[9] = tmp_params[7];
    param_pedine[10] = NULL;
    
    /* pedine ancora non piazzate con coord -1, -1 */
    for(i = 0; i < SO_NUM_P; i++) {
        mc_ped_squadra[i].pos_attuale.x = -1; 
        mc_ped_squadra[i].pos_attuale.y = -1;
        mc_ped_squadra[i].obiettivo.x = -1;
        mc_ped_squadra[i].obiettivo.y = -1;
        mc_ped_squadra[i].id_band = -1;
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

        piazzaPedina(i);
        
        /* 
        ultimo giocatore che piazza una pedina manda un messaggio al master
        che piazzerá a sua volta le bandierine, non rilascia primo token
        */
        if(i == (SO_NUM_P - 1) && pos_token == (SO_NUM_G - 1)) { 
        	avviso_master.mtype = (long) getpid() + MSG_PIAZZAMENTO;
            msgsnd(msg_id_coda, &avviso_master, sizeof(msg_conf) - sizeof(long), 0);
            TEST_ERROR;

            #if DEBUG
            printf("gioc %d (%ld): ult ped %d, msg fine piazzam su coda %d\n", (pos_token + 1), (long) getpid(), i, msg_id_coda);
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
void piazzaPedina(int ind_pedine) {
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

void gestRound() {
    msg_band msg_new_band;
    int i;

    do {
        #if DEBUG
        printf("gioc %d: ricezione msg band\n", (pos_token + 1));
        #endif
        
        /* ricezione messaggio con id di mc con bandiere */
        do {
            TEST_ERROR;
            msgrcv(msg_id_coda, &msg_new_band, sizeof(msg_band) - sizeof(long), (long) (getpid() + MSG_BANDIERA), 0);
        } while(errno == EINTR);

        mc_id_bandiere = msg_new_band.ind;
        num_band_round = msg_new_band.id_band;

        for(i = 0; i < SO_NUM_P; i++) {
            mc_ped_squadra[i].obiettivo.x = -1;
            mc_ped_squadra[i].obiettivo.y = -1;
            mc_ped_squadra[i].id_band = -1;
        }

        #if DEBUG
        printf("gioc %d: messaggio fine band ricevuto %d\n", (pos_token + 1), mc_id_bandiere);
        #endif

        /* assegnazione obiettivi */
        initObiettivi();

    } while(TRUE);
}

void initObiettivi() {
    msg_conf msg_perc, msg_obiettivo;
    int i, j, riga, range_scan, ped_sq, ped_nem, token_round;
    coord check;

    mc_bandiere = (band *) shmat(mc_id_bandiere, NULL, 0);
    TEST_ERROR;

    #if DEBUG
    printf("gioc %d: collegamento a ind bandiere %d riuscito\n", (pos_token + 1), mc_id_bandiere);
    #endif

    /*
    controlla se round é in corso
    se é in corso non tiene conto di pedine nemiche
    e non aspetta risposta da pedine della squadra
    */
    token_round = semctl(token_gioc, pos_token, GETVAL, 0);

    for(i = 0; i < num_band_round; i++) {
        if(!mc_bandiere[i].presa) {
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
                            sqOrNem(&ped_sq
                                    , &ped_nem
                                    , check
                                    , token_round
                                    , i
                                    , range_scan);

                        /* da dopo centro a dx */
                        check.x +=  2 * (range_scan - abs(riga));
                        if(check.x < SO_BASE && (range_scan - abs(riga)) > 0)
                            sqOrNem(&ped_sq
                                    , &ped_nem
                                    , check
                                    , token_round
                                    , i
                                    , range_scan);
                    }
                }
            } while(!ped_sq && !ped_nem);

        } else {
            for(j = 0; j < SO_NUM_P; j++) {
                if(mc_ped_squadra[j].id_band == i) {
                    mc_ped_squadra[j].obiettivo.x = -1;
                    mc_ped_squadra[j].obiettivo.y = -1;
                    mc_ped_squadra[j].id_band = -1;
                }
            }
        }
    }

    shmdt(mc_bandiere);
    TEST_ERROR;

    for(i = 0; i < SO_NUM_P; i++) {
        if(mc_ped_squadra[i].id_band != -1) {
            msg_obiettivo.mtype = (long) (pids_pedine[i] + MSG_OBIETTIVO);
            do {
                TEST_ERROR;
                msgsnd(msg_id_coda, &msg_obiettivo, sizeof(msg_conf) - sizeof(long), 0);
            } while(errno == EINTR);

            /* 
            msg per assicurarsi che prima dell'inizio di 
            un round tutte le pedine con un obiettivo
            calcolino il proprio percorso
            */
            if(token_round) {
                msgrcv(msg_id_coda, &msg_perc, sizeof(msg_conf) - sizeof(long), (long) (pids_pedine[i] + MSG_PERCORSO), 0);
                TEST_ERROR;
            }
        }
    }

    /* msg a master fine calcolo percorsi per inizio round */
    if(token_round) {
        msg_perc.mtype = (long) getppid() + MSG_PERCORSO;
        msgsnd(msg_id_coda, &msg_perc, sizeof(msg_conf) - sizeof(long), 0);

        #if DEBUG
        printf("gioc %d: invio msg a master inizio round\n", (pos_token + 1));
        #endif
    }
}

void sqOrNem(int *ped_sq, int *ped_nem, coord check, int token_round, int index, int mosse_richieste) {
    if(mc_char_scac[INDEX(check)] == ((pos_token + 1) + '0'))
        *ped_sq = assegnaObiettivo(check, index, mosse_richieste);
    else if(mc_char_scac[INDEX(check)] != 'B' 
        && mc_char_scac[INDEX(check)] != '0'
        && token_round /* giocatore non tiene conto delle pedine nemiche se é in corso un round */
        #if PRINT_SCAN
        && mc_char_scac[INDEX(check)] != '*'
        #endif
        )
        *ped_nem = TRUE;

    #if PRINT_SCAN
    if(mc_char_scac[INDEX(check)] == '0') 
        mc_char_scac[INDEX(check)] = '*';
    #endif
}

int assegnaObiettivo(coord pos_ped_sq, int id_band, int mosse_richieste) {
    int i;

    i = 0;

    while(calcDist(mc_ped_squadra[i].pos_attuale, pos_ped_sq)) i++;

    if((mc_ped_squadra[i].id_band != -1
        && mc_bandiere[id_band].punti <= mc_bandiere[mc_ped_squadra[i].id_band].punti)
        || mc_ped_squadra[i].mosse_rim < mosse_richieste) 
        return FALSE;

    mc_ped_squadra[i].obiettivo = mc_bandiere[id_band].pos_band;
    mc_ped_squadra[i].id_band = id_band;

    return TRUE;
}

void signalHandler(int signal_number) {
    int status;
    struct sigaction old_signal_handler;

    switch(signal_number) {
        case SIGUSR1:
            /* le pedine in movimento non sono presenti sulla scacchiera di caratteri */
            initObiettivi();
            break;
        case SIGUSR2:
            old_signal_handler.sa_handler = SIG_DFL;
            sigaction(SIGUSR1, &old_signal_handler, NULL);
            sigaction(SIGUSR2, &old_signal_handler, NULL);
            
            /* attesa terminazione di tutte le pedine */
            while(wait(&status) > 0);

            shmdt(mc_char_scac);
            TEST_ERROR;

            shmdt(mc_ped_squadra);
            TEST_ERROR;

            exit(EXIT_SUCCESS);
    }
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