#include "header.h"

void initPedine(char *);
void piazzaPedina(int);
int checkPosPedine(coord);
void initObiettivi();
void scanBandiera(int);
int assegnaObiettivo(coord, int, int);

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
    sigset_t block_mask;

    getConfig(argv[1]);

    srand(time(NULL) + getpid());

    new_signal_handler.sa_handler = &signalHandler;
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

    config_file = (char *) malloc(sizeof(char) * 10);
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
        /* TODO CONTROLLARE */
        fscanf(fs, "%d%*[0]", &DIST_BAND);
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

/* dist manhattan */
int calcDist(coord cas1, coord cas2) {
    return abs(cas1.x - cas2.x) + abs(cas1.y - cas2.y);
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
            errno = 0;
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

/* utilizzata solo per assegnazione degli obiettivi prima dell'inizio del round */
void initObiettivi() {
    msg_conf msg_perc, msg_obiettivo;
    int i;

    mc_bandiere = (band *) shmat(mc_id_bandiere, NULL, 0);
    TEST_ERROR;

    #if DEBUG
    printf("gioc %d: collegamento a ind bandiere %d riuscito\n", (pos_token + 1), mc_id_bandiere);
    #endif

    for(i = 0; i < num_band_round; i++)
        scanBandiera(i);

    shmdt(mc_bandiere);
    TEST_ERROR;

    for(i = 0; i < SO_NUM_P; i++) {
        if(mc_ped_squadra[i].id_band != -1) {
            msg_obiettivo.mtype = (long) (pids_pedine[i] + MSG_OBIETTIVO);

            msgsnd(msg_id_coda, &msg_obiettivo, sizeof(msg_conf) - sizeof(long), 0);
            TEST_ERROR;

            /* 
            msg per assicurarsi che prima dell'inizio di 
            un round tutte le pedine con un obiettivo
            calcolino il proprio percorso
            */
            msgrcv(msg_id_coda, &msg_perc, sizeof(msg_conf) - sizeof(long), (long) (pids_pedine[i] + MSG_PERCORSO), 0);
            TEST_ERROR;
        }
    }

    /* msg a master fine calcolo percorsi per inizio round */
    msg_perc.mtype = (long) getppid() + MSG_PERCORSO;
    msgsnd(msg_id_coda, &msg_perc, sizeof(msg_conf) - sizeof(long), 0);

    #if DEBUG
    printf("gioc %d: invio msg a master inizio round\n", (pos_token + 1));
    #endif
}

void scanBandiera(int id_band) {
    int riga, range_scan, ped_sq, ped_cont;
    coord check;

    ped_sq = FALSE;
    ped_cont = SO_NUM_P;
    range_scan = 0;

    do {
        range_scan++;
        /* scan riga da -range a +range */
        for(riga = -range_scan; riga <= range_scan; riga++) {
            check.y = mc_bandiere[id_band].pos_band.y;
            check.y += riga;
            if(check.y >= 0 && check.y < SO_ALTEZZA) {
                /* scan da sx a centro */
                check.x = mc_bandiere[id_band].pos_band.x;
                check.x -= (range_scan - abs(riga));
                if(check.x >= 0 && mc_char_scac[INDEX(check)] == ((pos_token + 1) + '0')) {
                    ped_cont--;
                    ped_sq = assegnaObiettivo(check
                                            , id_band
                                            , range_scan);
                }

                /* da dopo centro a dx */
                check.x += 2 * (range_scan - abs(riga));
                if(check.x < SO_BASE 
                    && (range_scan - abs(riga)) > 0 
                    && mc_char_scac[INDEX(check)] == ((pos_token + 1) + '0')) {
                    ped_cont--;
                    ped_sq = assegnaObiettivo(check
                                            , id_band
                                            , range_scan);
                }
            }
        }

    /* 
    esco dal ciclo se una pedina é stata assegnata alla 
    bandiera oppure se nessuna delle pedine puó arrivarci
    */
    } while(!ped_sq && ped_cont);
}

int assegnaObiettivo(coord check, int id_band, int mosse_richieste) {
    int ped_index;

    ped_index = 0;

    while(calcDist(mc_ped_squadra[ped_index].pos_attuale, check)) ped_index++;
    
    if(mc_ped_squadra[ped_index].mosse_rim < mosse_richieste) /* se la pedina non ha abbastanza mosse */
        return FALSE;

    if(mc_ped_squadra[ped_index].id_band != -1) { /* se la pedina ha giá un obiettivo assegnato */
        if(mc_bandiere[id_band].punti <= mc_bandiere[mc_ped_squadra[ped_index].id_band].punti) /* se la bandiera giá assegnata vale di piú di quella nuova */
            return FALSE; /* non assegno pedina */

        /* 
        nel caso una pedina venisse riassegnata ne 
        cerco un'altra che possa prendere quella bandiera 
        (se disponibile)
        */
        scanBandiera(mc_ped_squadra[ped_index].id_band);
    }

    mc_ped_squadra[ped_index].obiettivo = mc_bandiere[id_band].pos_band;
    mc_ped_squadra[ped_index].id_band = id_band;

    #if DEBUG
    printf("gioc %d: band %d assegnata a ped\n", pos_token, id_band);
    #endif

    return TRUE;
}

void signalHandler(int signal_number) {
    int status;
    
    errno = 0;

    signal(SIGUSR2, SIG_DFL);
    
    /* attesa terminazione di tutte le pedine */
    while(wait(&status) > 0);

    shmdt(mc_char_scac);

    shmdt(mc_ped_squadra);

    exit(EXIT_SUCCESS);
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