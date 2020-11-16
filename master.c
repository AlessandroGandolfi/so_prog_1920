#include "header.h"

void checkMode(int, char *);
void initRisorse();
void initGiocatori(char *);
void initSemScacchiera();
void somebodyTookMaShmget();
void stampaScacchiera();
int initBandiere();
int checkPosBandiere(coord, int);

/* globali */
gioc *giocatori;
char *mc_char_scac;
band *mc_bandiere;
int mc_id_scac, mc_id_band, msg_id_coda, sem_id_scac, token_gioc, num_round;

int main(int argc, char **argv) {
    msg_conf msg_ped;

    checkMode(argc, argv[1]);

    #if DEBUG
    // testConfig();
    #endif

    /* time torna int secondi da mezzanotte primo gennaio 1970 */
    srand(time(NULL) + getpid());

    signal(SIGINT, &signalHandler);
    TEST_ERROR;
    signal(SIGALRM, &signalHandler);
    TEST_ERROR;

    initRisorse();

    /* creazione giocatori, valorizzazione pids_giocatori */
    initGiocatori(argv[1]);

    /* master aspetta che l'ultimo giocatore abbia piazzato l'ultima pedina */
    #if DEBUG
    // printf("master: attesa msg ultimo piazzam da %ld, id coda %d\n", (long) giocatori[SO_NUM_G - 1].pid, msg_id_coda);
    #endif
    msgrcv(msg_id_coda, &msg_ped, sizeof(msg_conf) - sizeof(long), (long) (giocatori[SO_NUM_G - 1].pid + MSG_PIAZZAMENTO), 0);
    TEST_ERROR;

    gestRound();

    return 0;
}

/* 
controllo su numero parametri esatti e corretta modalitá selezionata
param: 
- numero parametri passati a lancio esecuzione
- modalitá selezionata (def a "" in makefile)
*/
void checkMode(int argc, char *mode) {
    if(argc < 2 
        || (strcmp(mode, "easy") != 0 && strcmp(mode, "hard") != 0
            #if DEBUG
            && strcmp(mode, "debug") != 0
            #endif
        )
    ) {
        printf("Specificare \"easy\" o \"hard\" per avviare il progetto\n");
        exit(0);
    } else {
        getConfig(mode);
    }
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
init risorse globali (memorie condivise, coda msg, semafori, ...)
*/
void initRisorse() {
    /* creazione coda msg */
    msg_id_coda = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600);
    TEST_ERROR;

    /* creazione e collegamento a mc scacchiera */ 
    mc_id_scac = shmget(IPC_PRIVATE, sizeof(char) * SO_ALTEZZA * SO_BASE, S_IRUSR | S_IWUSR);
    TEST_ERROR;
    mc_char_scac = (char *) shmat(mc_id_scac, NULL, 0);
    TEST_ERROR;
    /* scacchiera tutta a 0, caratteri sovrascritti da semafori e pedine in futuro */
    memset(mc_char_scac, '0', sizeof(char) * SO_ALTEZZA * SO_BASE);

    /* semafori scacchiera */
    initSemScacchiera();

    giocatori = (gioc *) calloc(SO_NUM_G, sizeof(gioc));
}

/*
init array semafori scacchiera, tutti a 1 
*/
void initSemScacchiera() {
    int i;
    semun sem_arg;

    sem_arg.array = (unsigned short *) calloc(SO_BASE * SO_ALTEZZA, sizeof(unsigned short));
    
    /* memset cambia 1 a 127 per cast parametro da unsigned short a int */
    for(i = 0; i < (SO_BASE * SO_ALTEZZA); i++) sem_arg.array[i] = 1; 

    sem_id_scac = semget(IPC_PRIVATE, SO_BASE * SO_ALTEZZA, 0600);
    TEST_ERROR;

    semctl(sem_id_scac, 0, SETALL, sem_arg);
    TEST_ERROR;
}

/* 
eliminazione risorse (memorie condivise, coda msg, semafori, ...)
*/
void somebodyTookMaShmget() {
    int i;

    errno = 0;

    semctl(sem_id_scac, 0, IPC_RMID);
    TEST_ERROR;

    for(i = 0; i < SO_NUM_G; i++) {
        shmctl(giocatori[i].mc_id_squadra, IPC_RMID, NULL);
        TEST_ERROR;
    }

    free(giocatori);
    
    shmdt(mc_bandiere);
    TEST_ERROR;
	shmctl(mc_id_band, IPC_RMID, NULL);
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

/*
stampa scacchiera e calcolo mosse rimanenti per squadra
*/
void stampaScacchiera() {
    int i, j;
    ped *mc_ped_squadra;
    struct timespec arg_sleep;

    /* mosse rimanenti */
    for(i = 0; i < SO_NUM_G; i++) {
        giocatori[i].tot_mosse_rim = 0;
        
        mc_ped_squadra = (ped *) shmat(giocatori[i].mc_id_squadra, NULL, 0);
        TEST_ERROR;

        for(j = 0; j < SO_NUM_P; j++)
            giocatori[i].tot_mosse_rim += mc_ped_squadra[j].mosse_rim;

        shmdt(mc_ped_squadra);
        TEST_ERROR;
    }

    #if ((defined (LINUX) || defined (__linux__) || defined (__APPLE__)) && !DEBUG)
    /* clear console, supportato solo su UNIX */
    printf("\n\e[1;1H\e[2J");
    #endif

    /* stampa matrice caratteri */
    for(i = 0; i < (SO_ALTEZZA * SO_BASE); i++) {
        #if ENABLE_COLORS
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
            case '0':
                printf("\033[0m");
                break;
            default:
                printf("\033[0;33m");
                break;
        }
        #endif
        printf("%c", mc_char_scac[i]);
        if(!((i + 1) % SO_BASE)) printf("\n");
    }

    #if ENABLE_COLORS
    printf("\033[0m");
    #endif

    printf("Risultati round #%d:\n", num_round);

    for(i = 0; i < SO_NUM_G; i++) 
        printf("- Punteggio giocatore %d: %d; %d mosse totali rimanenti\n", (i + 1), giocatori[i].punteggio, giocatori[i].tot_mosse_rim);

    arg_sleep.tv_sec = 0;
    arg_sleep.tv_nsec = SO_MIN_HOLD_NSEC;
    nanosleep(&arg_sleep, NULL);
}

/*
creazione ed esecuzione giocatori, creazione e valorizzazione token
param:
- modalitá selezionata
*/
void initGiocatori(char *mode) {
    int i;
    char *param_giocatori[9];
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
    0 - path relativo file giocatore
    1 - difficoltá gioco (per config)
    2 - id token
    3 - indice token squadra
    4 - id sem scacchiera
    5 - id mc squadra, array pedine
    6 - id coda msg
    7 - id mc char scacchiera
    */
    param_giocatori[0] = "./giocatore";
    param_giocatori[1] = mode;
    sprintf(tmp_params[0], "%d", token_gioc);
    param_giocatori[2] = tmp_params[0];
    sprintf(tmp_params[2], "%d", sem_id_scac);
    param_giocatori[4] = tmp_params[2];
    sprintf(tmp_params[4], "%d", msg_id_coda);
    param_giocatori[6] = tmp_params[4];
    sprintf(tmp_params[5], "%d", mc_id_scac);
    param_giocatori[7] = tmp_params[5];
    param_giocatori[8] = NULL;

    for(i = 0; i < SO_NUM_G; i++) {
        giocatori[i].punteggio = 0;

        /* mc di squadra, array di pedine, visibile solo alla squadra */
        giocatori[i].mc_id_squadra = shmget(IPC_PRIVATE, sizeof(ped) * SO_NUM_P, S_IRUSR | S_IWUSR);
        TEST_ERROR;

        /* posizione token giocatore */
        sprintf(tmp_params[1], "%d", i);
        param_giocatori[3] = tmp_params[1];
        
        /* passaggio id mc di squadra come parametro a giocatori */
        sprintf(tmp_params[3], "%d", giocatori[i].mc_id_squadra);
        param_giocatori[5] = tmp_params[3];

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
            default:
                setpgid(giocatori[i].pid, 0);
                TEST_ERROR;
                break;
        }
    }

    #if DEBUG
    // printf("master: fine creazione giocatori\n");
    #endif
}

void gestRound() {
    int i, num_band;
    msg_band_presa msg_presa;
    semun sem_arg;

    /* usato per inizio e fine round */
    sem_arg.array = (unsigned short *) calloc(SO_NUM_G, sizeof(unsigned short));

    num_round = 1;

    // TODO controllare che ci siano tutte le periferiche
    do {
        /* creazione bandiere, valorizzazione array bandiere */
        num_band = initBandiere();
        
        for(i = 0; i < SO_NUM_G; i++)
            sem_arg.array[i] = 0;

        printf("------------inizio------------\n");

        stampaScacchiera();

        alarm(SO_MAX_TIME);

        semctl(token_gioc, 0, SETALL, sem_arg);
        TEST_ERROR;

        for(i = 0; i < num_band; i++) {
            msgrcv(msg_id_coda, &msg_presa, sizeof(msg_band_presa) - sizeof(long), (long) (getpid() + MSG_BANDIERA), 0);
            TEST_ERROR;

            if(!mc_bandiere[msg_presa.id_band].presa) {
                #if DEBUG
                printf("band %d presa da gioc %d, %d punti\n"
                    , msg_presa.id_band
                    , (msg_presa.pos_token + 1)
                    , mc_bandiere[msg_presa.id_band].punti);
                #endif

                mc_bandiere[msg_presa.id_band].presa = TRUE;
                giocatori[msg_presa.pos_token].punteggio += mc_bandiere[msg_presa.id_band].punti;
            } else i--;
        }

        for(i = 0; i < SO_NUM_G; i++)
            sem_arg.array[i] = 1;

        semctl(token_gioc, 0, SETALL, sem_arg);
        TEST_ERROR;

        alarm(0);

        stampaScacchiera();
        printf("-------------fine-------------\n");

        num_round++;
    } while(TRUE);
}

/*
piazzamento nuove bandiere e distribuzione loro punteggio
param:
- id token giocatori (usato successivamente per partenza round)
*/
int initBandiere() {
    int i, riga, colonna, tot_punti_rim, num_band;
    msg_band msg_new_band;
    msg_conf msg_perc;
    semun sem_arg;
    coord casella;

    num_band = (rand() % (SO_FLAG_MAX - SO_FLAG_MIN + 1)) + SO_FLAG_MIN;

    /* valgono tutte almeno un punto */
    tot_punti_rim = SO_ROUND_SCORE - num_band;

    printf("%d bandiere piazzate\n", num_band);

    /* gestione cancellazione dopo primo round */
    if(num_round > 1) {
        shmdt(mc_bandiere);
        TEST_ERROR;

        shmctl(mc_id_band, IPC_RMID, NULL);
        TEST_ERROR;
    }

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
            casella.y = rand() % SO_ALTEZZA;
            casella.x = rand() % SO_BASE;
        } while(!checkPosBandiere(casella, i));

        /* valorizzazione mc bandiere */
        mc_bandiere[i].pos_band = casella;
        mc_bandiere[i].presa = FALSE;
        
        /* 
        una bandiera vale al massimo la metá dei punti rimanenti 
        da distribuire a meno che non sia l'ultima da piazzare 
        */
        if(i != (num_band - 1))
            mc_bandiere[i].punti = rand() % (tot_punti_rim / 2);
        else
            mc_bandiere[i].punti = tot_punti_rim;
        tot_punti_rim -= mc_bandiere[i].punti;
        /* valgono almeno un punto */
        mc_bandiere[i].punti++;

        /* piazzamento bandiere su scacchiera */
        mc_char_scac[INDEX(casella)] = 'B';

        #if DEBUG_BAND
        mc_char_scac[INDEX(casella)] = (i + 'A');
        #endif
    }
    
    #if DEBUG
    testSemToken();
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
    testSemToken();
    #endif

    /* manda un messaggio per giocatore */
    for(i = 0; i < SO_NUM_G; i++) {
        #if DEBUG
        // printf("master: invio id mc band %d a giocatore %d\n", mc_id_band, (i + 1));
        #endif

        msg_new_band.id_band = num_band;
        msg_new_band.ind = mc_id_band;
        msg_new_band.mtype = (long) giocatori[i].pid + MSG_BANDIERA;
        msgsnd(msg_id_coda, &msg_new_band, sizeof(msg_band) - sizeof(long), 0);
        TEST_ERROR;

        #if DEBUG
        printf("master: calcolo percorsi di giocatore %d\n", (i + 1));
        #endif

        /* tra queste sc i giocatori assegnano obiettivi e pedine calcolano i percorsi */

        /* una alla volta le squadre calcolano i percorsi delle pedine */
        msgrcv(msg_id_coda, &msg_perc, sizeof(msg_conf) - sizeof(long), (long) (getpid() + MSG_PERCORSO), 0);
        TEST_ERROR;

        #if DEBUG
        printf("master: fine percorsi giocatore %d\n", (i + 1));
        #endif
    }

    return num_band;
}

/*
controlli per piazzamento nuova bandierina su posizione delle 
bandierine giá piazzate e su cella libera da pedine
TRUE se supera controlli, FALSE altrimenti
param:
- coordinate generate
- numero di bandierine da piazzare questo round
*/
int checkPosBandiere(coord casella, int num_band_piazzate) {
    int i;

    /* controllo su prima bandiera a essere piazzata */
    if(num_band_piazzate == 0 && mc_char_scac[INDEX(casella)] != '0') return FALSE;

    /* dalla seconda in poi controllo che la bandiera che il master piazza sia distante dalle altre */
    for(i = 0; i <= num_band_piazzate && (mc_bandiere[i].pos_band.x != -1 && mc_bandiere[i].pos_band.y != -1); i++)
        if(calcDist(casella, mc_bandiere[i].pos_band) < DIST_BAND
            || mc_char_scac[INDEX(casella)] != '0')
            return FALSE;

    return TRUE;
}

/* dist manhattan */
int calcDist(coord cas1, coord cas2) {
    return abs(cas1.x - cas2.x) + abs(cas1.y - cas2.y);
}

void signalHandler(int signal_number) {
    int status, i;
    
    errno = 0;

    for(i = 0; i < SO_NUM_G; i++)
        kill(-giocatori[i].pid, SIGUSR2);

    /* attesa terminazione di tutti i giocatori e pedine */
    while(wait(&status) > 0);

    stampaScacchiera();

    /* detach e rm mc, sem, msg */
    somebodyTookMaShmget();

    signal(SIGALRM, SIG_DFL);
    signal(SIGINT, SIG_DFL);

    exit(EXIT_SUCCESS);
}

#if DEBUG
/* stampa dei valori del semaforo token */
void testSemToken() {
    unsigned short *val_array;
    int i;

    val_array = (unsigned short *) calloc(SO_NUM_G, sizeof(unsigned short));

    semctl(token_gioc, 0, GETALL, val_array);

    for(i = 0; i < SO_NUM_G; i++) {
        printf("master: token gioc %d settato a %hu\n", (i + 1), val_array[i]);
    }
}

/* check config valorizzata correttamente */
void testConfig() {
    printf("valori configurazione:\n");
    printf("SO_NUM_G: %d\n", SO_NUM_G);
    printf("SO_NUM_P: %d\n", SO_NUM_P);
    printf("SO_MAX_TIME: %d\n", SO_MAX_TIME);
    printf("SO_BASE: %d\n", SO_BASE);
    printf("SO_ALTEZZA: %d\n", SO_ALTEZZA);
    printf("SO_FLAG_MIN: %d\n", SO_FLAG_MIN);
    printf("SO_FLAG_MAX: %d\n", SO_FLAG_MAX);
    printf("SO_ROUND_SCORE: %d\n", SO_ROUND_SCORE);
    printf("SO_N_MOVES: %d\n", SO_N_MOVES);
    printf("SO_MIN_HOLD_NSEC: %d\n", SO_MIN_HOLD_NSEC);
    printf("DIST_PED: %d\n", DIST_PED);
    printf("DIST_BAND: %d\n", DIST_BAND);
}
#endif