#include "master.h"

/* globali */
player *players;
char *sm_char_cb;
flag *sm_flags;
int sm_id_cb
    , sm_id_flags
    , msg_id_queue
    , sem_id_cb
    , token_players
    , num_round
    , end_game;
time_t start_time, end_time;

/* 
parametri a master
0 - path relativo file master
1 - modalitá di gioco
*/
int main(int argc, char **argv) {

    check_mode(argc, argv[1]);

    #if DEBUG
    test_config();
    #endif

    /* time torna int secondi da mezzanotte primo gennaio 1970 */
    start_time = time(NULL);
    srand(start_time + getpid());

    signal(SIGINT, &signal_handler);
    TEST_ERROR
    signal(SIGALRM, &signal_handler);
    TEST_ERROR
    signal(SIGCHLD, &signal_handler);
    TEST_ERROR

    init_resources();

    /* creazione giocatori, valorizzazione pids_giocatori */
    init_players(argv[1]);

    play_round();

    return 0;
}

void check_mode(int argc, char *mode) {
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
        get_config(mode);
    }
}

void get_config(char *mode) {
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
        fscanf(fs, "%d%*[0]", &DIST_BAND);
    } else {
        printf("Errore apertura file di configurazione\n");
        exit(0);
    }

    free(config_file);
    fclose(fs);
}

void init_resources() {
    /* creazione coda msg */
    msg_id_queue = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600);
    TEST_ERROR

    /* creazione e collegamento a mc scacchiera */ 
    sm_id_cb = shmget(IPC_PRIVATE, sizeof(char) * SO_ALTEZZA * SO_BASE, S_IRUSR | S_IWUSR);
    TEST_ERROR

    sm_char_cb = (char *) shmat(sm_id_cb, NULL, 0);
    TEST_ERROR
    
    /* scacchiera tutta a 0, caratteri sovrascritti da semafori e pedine in futuro */
    memset(sm_char_cb, '0', sizeof(char) * SO_ALTEZZA * SO_BASE);

    /* semafori scacchiera */
    init_sem_chessboard();

    players = (player *) calloc(SO_NUM_G, sizeof(player));
}

void init_sem_chessboard() {
    int i;
    semun sem_arg;

    sem_arg.array = (unsigned short *) calloc(SO_BASE * SO_ALTEZZA, sizeof(unsigned short));
    
    /* memset cambia 1 a 127 per cast parametro da unsigned short a int */
    for(i = 0; i < (SO_BASE * SO_ALTEZZA); i++) sem_arg.array[i] = 1; 

    sem_id_cb = semget(IPC_PRIVATE, SO_BASE * SO_ALTEZZA, 0600);
    TEST_ERROR

    semctl(sem_id_cb, 0, SETALL, sem_arg);
    TEST_ERROR
}

void remove_resources() {
    int i;
    errno = 0;

    semctl(sem_id_cb, 0, IPC_RMID);
    TEST_ERROR

    for(i = 0; i < SO_NUM_G; i++) {
        shmctl(players[i].sm_id_team, IPC_RMID, NULL);
        TEST_ERROR
    }

    free(players);
    
    shmdt(sm_flags);
    TEST_ERROR
	shmctl(sm_id_flags, IPC_RMID, NULL);
    TEST_ERROR

    shmdt(sm_char_cb);
    TEST_ERROR
	shmctl(sm_id_cb, IPC_RMID, NULL);
    TEST_ERROR

    semctl(token_players, 0, IPC_RMID);
    TEST_ERROR

    msgctl(msg_id_queue, IPC_RMID, NULL);
    TEST_ERROR
}

void print_chessboard() {
    int i;
    errno = 0;

    remaining_moves();
    
    /* se supportato, la console viene "pulita" prima della stampa della scacchiera */
    CLEAR

    /* stampa matrice caratteri */
    for(i = 0; i < (SO_ALTEZZA * SO_BASE); i++) {
        #if ENABLE_COLORS
        switch(sm_char_cb[i]) {
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
        printf("%c", sm_char_cb[i]);
        if(!((i + 1) % SO_BASE)) printf("\n");
    }

    #if ENABLE_COLORS
    printf("\033[0m");
    #endif

    print_info_round();
}

void remaining_moves(){
    pawn *mc_ped_squadra;
    int i, j;
    
    for(i = 0; i < SO_NUM_G; i++) {
        players[i].total_rem_moves = 0;
        
        mc_ped_squadra = (pawn *) shmat(players[i].sm_id_team, NULL, 0);
        TEST_ERROR

        for(j = 0; j < SO_NUM_P; j++)
            players[i].total_rem_moves += mc_ped_squadra[j].remaining_moves;

        shmdt(mc_ped_squadra);
        TEST_ERROR
    }
}

void print_info_round(){
    int i, total_points;
    float used_moves;

    printf("Risultati round #%d:\n", num_round);

    for(i = 0; i < SO_NUM_G; i++) 
        printf("- Punteggio giocatore %d: %d; %d mosse totali rimanenti\n", (i + 1), players[i].points, players[i].total_rem_moves);

    if(end_game) {
        total_points = 0;
        used_moves = 0.0;

        printf("\n------------------- Statistiche di fine gioco -------------------\n");

        for(i = 0; i < SO_NUM_G; i++) {
            total_points += players[i].points;
            used_moves = (SO_NUM_P * SO_N_MOVES) - players[i].total_rem_moves;
            printf("Rapporti giocatore %d:\n", i + 1);
            printf("Percentuale mosse utilizzate: %.3f%%\n", used_moves / (SO_NUM_P * SO_N_MOVES) * 100.0);
            printf("Punti ottenuti/mosse utilizzate: %.3f\n\n", players[i].points / used_moves);
        }

        printf("Il gioco é durato %d minuti e %d secondi\n", (int) (end_time - start_time) / 60, (int) (end_time - start_time) % 60);
        printf("Rapporto punti totali/tempo di gioco: %.3f\n", (float) total_points / (end_time - start_time));
    }
}

void init_players(char *mode) {
    int i;
    char *param_players[9];
    char tmp_params[5][sizeof(char *)];
    semun sem_arg;
    msg_conf msg_pawn;

    sem_arg.array = (unsigned short *) calloc(SO_BASE, sizeof(unsigned short));

    /* init token posizionamento pedine */
    for(i = 0; i < SO_NUM_G; i++) sem_arg.array[i] = i ? 0 : 1;
    
    token_players = semget(IPC_PRIVATE, SO_NUM_G, 0600);
    TEST_ERROR
    semctl(token_players, 0, SETALL, sem_arg);
    TEST_ERROR

    init_params_players(mode, param_players, tmp_params);
    
    create_players(param_players, tmp_params);

    /* 
    aspetta che conferma che i giocatori abbiano piazzato le pedine,
    solo l'ultimo giocatore avvisa il master 
    */
    #if DEBUG
    /* printf("master: attesa msg ultimo piazzam da %ld, id coda %d\n", (long) giocatori[SO_NUM_G - 1].pid, msg_id_queue); */
    #endif

    msgrcv(msg_id_queue, &msg_pawn, sizeof(msg_conf) - sizeof(long), (long) (players[SO_NUM_G - 1].pid + MSG_PLACEMENT), 0);
    TEST_ERROR
}

void init_params_players(char *mode, char **param_players, char tmp_params[][sizeof(char *)]){
    param_players[0] = "./bin/giocatore";
    param_players[1] = mode;
    sprintf(tmp_params[0], "%d", token_players);
    param_players[2] = tmp_params[0];
    sprintf(tmp_params[2], "%d", sem_id_cb);
    param_players[4] = tmp_params[2];
    sprintf(tmp_params[4], "%d", msg_id_queue);
    param_players[6] = tmp_params[4];
    sprintf(tmp_params[5], "%d", sm_id_cb);
    param_players[7] = tmp_params[5];
    param_players[8] = NULL;
}

void create_players(char ** param_players, char tmp_params[][sizeof(char *)]){
    int i;

    for(i = 0; i < SO_NUM_G; i++) {
        players[i].points = 0;

        /* mc di squadra, array di pedine, visibile solo alla squadra */
        players[i].sm_id_team = shmget(IPC_PRIVATE, sizeof(pawn) * SO_NUM_P, S_IRUSR | S_IWUSR);
        TEST_ERROR

        /* posizione token giocatore */
        sprintf(tmp_params[1], "%d", i);
        param_players[3] = tmp_params[1];
        
        /* passaggio id mc di squadra come parametro a giocatori */
        sprintf(tmp_params[3], "%d", players[i].sm_id_team);
        param_players[5] = tmp_params[3];

        players[i].pid = fork();
        TEST_ERROR

        switch(players[i].pid) {
            case -1:
                TEST_ERROR
                exit(EXIT_FAILURE);
            case 0:
                execv("./bin/giocatore", param_players);
                TEST_ERROR
                exit(EXIT_FAILURE);
            default:
                /* valorizzazione pid di gruppo per gestione fine o interruzione gioco */ 
                setpgid(players[i].pid, 0);
                TEST_ERROR
                break;
        }
    }

    #if DEBUG
    /* printf("master: fine creazione giocatori\n"); */
    #endif
}

void play_round() {
    int i, num_flags;
    semun sem_arg;

    /* usato per inizio e fine round */
    sem_arg.array = (unsigned short *) calloc(SO_NUM_G, sizeof(unsigned short));

    num_round = 1;
    end_game = FALSE;

    do {
        /* creazione bandiere, valorizzazione array bandiere */
        num_flags = init_flags();
        
        for(i = 0; i < SO_NUM_G; i++)
            sem_arg.array[i] = 0;

        print_chessboard();

        alarm(SO_MAX_TIME);
        
        semctl(token_players, 0, SETALL, sem_arg);
        TEST_ERROR

        wait_flag_taken(num_flags);
        
        for(i = 0; i < SO_NUM_G; i++)
            sem_arg.array[i] = 1;

        semctl(token_players, 0, SETALL, sem_arg);
        TEST_ERROR

        alarm(0);
        
        print_chessboard();

        num_round++;
    } while(!end_game);

}

void wait_flag_taken(int num_flags){
    msg_t_flag msg_taken;
    int i;

    /* aspetta tanti messaggi quante bandiere ha piazzato per il round attuale */
    for(i = 0; i < num_flags; i++) {
        msgrcv(msg_id_queue, &msg_taken, sizeof(msg_t_flag) - sizeof(long), (long) (getpid() + MSG_FLAG), 0);
        TEST_ERROR

        if(!sm_flags[msg_taken.id_flag].taken) {
            #if DEBUG
            printf("band %d presa da gioc %d, %d punti\n"
                , msg_taken.id_flag
                , (msg_taken.pos_token + 1)
                , sm_flags[msg_taken.id_flag].points);
            #endif

            sm_flags[msg_taken.id_flag].taken = TRUE;
            players[msg_taken.pos_token].points += sm_flags[msg_taken.id_flag].points;
        } else i--;
    }
}

int init_flags() {
    int i, tot_rem_points, num_flags;
    msg_flag msg_new_band;
    msg_conf msg_perc;
    semun sem_arg;
    coord box;

    num_flags = (rand() % (SO_FLAG_MAX - SO_FLAG_MIN + 1)) + SO_FLAG_MIN;

    /* valgono tutte almeno un punto */
    tot_rem_points = SO_ROUND_SCORE - num_flags;

    /* gestione cancellazione dopo primo round */
    if(num_round > 1) {
        shmdt(sm_flags);
        TEST_ERROR

        shmctl(sm_id_flags, IPC_RMID, NULL);
        TEST_ERROR
    }

    sm_id_flags = shmget(IPC_PRIVATE, num_flags * sizeof(flag), S_IRUSR | S_IWUSR);
    TEST_ERROR

    /* creazione array bandiere in mc */
    sm_flags = (flag *) shmat(sm_id_flags, NULL, 0);
    TEST_ERROR

    /* bandiere ancora non piazzate con coord -1, -1 */
    for(i = 0; i < num_flags; i++) {
        sm_flags[i].position.x = -1;
        sm_flags[i].position.y = -1;
    }

    /* 
    per ogni bandiera randomizzo i valori fino a quando non trovo 
    una cella libera senza altre bandiere "troppo vicine" 
    */
    for(i = 0; i < num_flags; i++) {
        do {
            box.y = rand() % SO_ALTEZZA;
            box.x = rand() % SO_BASE;
        } while(!check_flags_pos(box, i));

        set_flags_pos_points(i, num_flags, tot_rem_points, box);
    }
    
    #if DEBUG
    test_sem_token();
    #endif

    /* 
    setto token a 1 per pedine in wait for 0 successivamente
    da decrementare a inizio round per inizio movimenti 
    */
    sem_arg.array = (unsigned short *) calloc(SO_NUM_G, sizeof(unsigned short));
    for(i = 0; i < SO_NUM_G; i++) sem_arg.array[i] = 1;
    semctl(token_players, 0, SETALL, sem_arg);
    free(sem_arg.array);

    #if DEBUG
    test_sem_token();
    #endif

    /* manda un messaggio per giocatore */
    for(i = 0; i < SO_NUM_G; i++) {
        send_msg_new_band(i, num_flags, msg_new_band);

        /* tra queste funzioni i giocatori assegnano obiettivi e pedine calcolano i percorsi */

        rcv_msg_path_calculation(msg_perc);      
    }
    
    return num_flags;
}

void set_flags_pos_points(int i, int num_flags, int tot_rem_points, coord box){

    /* valorizzazione mc bandiere */
    sm_flags[i].position = box;
    sm_flags[i].taken = FALSE;
    
    /* 
    una bandiera vale al massimo la metá dei punti rimanenti 
    da distribuire a meno che non sia l'ultima da piazzare 
    */
    if(i != (num_flags - 1))
        sm_flags[i].points = rand() % (tot_rem_points / 2);
    else
        sm_flags[i].points = tot_rem_points;
    tot_rem_points -= sm_flags[i].points;
    /* valgono almeno un punto */
    sm_flags[i].points++;

    /* piazzamento bandiere su scacchiera */
    sm_char_cb[INDEX(box)] = 'B';

    #if DEBUG_BAND
    sm_char_cb[INDEX(box)] = (i + 'A');
    #endif
}

void send_msg_new_band(int i, int num_flags, msg_flag msg_new_band){
    #if DEBUG
    /* printf("master: invio id mc band %d a giocatore %d\n", sm_id_flags, (i + 1)); */
    #endif

    msg_new_band.num_flags = num_flags;
    msg_new_band.sm_id_flags = sm_id_flags;
    msg_new_band.mtype = (long) players[i].pid + MSG_FLAG;
    msgsnd(msg_id_queue, &msg_new_band, sizeof(msg_flag) - sizeof(long), 0);
    TEST_ERROR
}

void rcv_msg_path_calculation(msg_conf msg_perc){
    #if DEBUG
    printf("master: calcolo percorsi di giocatore %d\n", (i + 1));
    #endif

    /* una alla volta le squadre calcolano i percorsi delle pedine */
    msgrcv(msg_id_queue, &msg_perc, sizeof(msg_conf) - sizeof(long), (long) (getpid() + MSG_PATH), 0);
    TEST_ERROR

    #if DEBUG
    printf("master: fine percorsi giocatore %d\n", (i + 1));
    #endif
}

int check_flags_pos(coord box, int num_placed_flags) {
    int i;

    /* controllo su prima bandiera a essere piazzata */
    if(num_placed_flags == 0 && sm_char_cb[INDEX(box)] != '0') return FALSE;

    /* dalla seconda in poi controllo che la bandiera che il master piazza sia distante dalle altre */
    for(i = 0; i <= num_placed_flags; i++)
        if(sm_char_cb[INDEX(box)] != '0'
            || calc_dist(box, sm_flags[i].position) < DIST_BAND)
            return FALSE;

    return TRUE;
}

int calc_dist(coord box1, coord box2) {
    return abs(box1.x - box2.x) + abs(box1.y - box2.y);
}

void signal_handler(int signal_number) {
    int status, i, kidpid;
    
    errno = 0;

    switch(signal_number){
        case SIGALRM:
        case SIGINT:

            end_game = TRUE;
            end_time = time(NULL);

            /* invio segnale fine gioco a giocatori e relative pedine */
            for(i = 0; i < SO_NUM_G; i++)
                kill(-players[i].pid, SIGUSR1);

            while(TRUE) pause();
            break;
        
        case SIGCHLD:
            while((kidpid = waitpid(-1, &status, WNOHANG)) > 0) {
				if (WEXITSTATUS(status) != 0) {
					printf("OPS: kid %d is dead status %d\n", kidpid, WEXITSTATUS(status));
				}
			}

            /* quando non ci sono piú giocatori termino il processo */
			if(errno == ECHILD) {
                errno = 0;
                
                print_chessboard();

                remove_resources();

                signal(SIGALRM, SIG_DFL);
                TEST_ERROR
                signal(SIGINT, SIG_DFL);
                TEST_ERROR
                signal(SIGCHLD, SIG_DFL);
                TEST_ERROR

                exit(EXIT_SUCCESS);
			}
            break;

    }
}

#if DEBUG
void test_sem_token() {
    unsigned short *val_array;
    int i;

    val_array = (unsigned short *) calloc(SO_NUM_G, sizeof(unsigned short));

    semctl(token_players, 0, GETALL, val_array);

    for(i = 0; i < SO_NUM_G; i++) {
        printf("master: token gioc %d settato a %hu\n", (i + 1), val_array[i]);
    }

    free(val_array);
}

void test_config() {
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