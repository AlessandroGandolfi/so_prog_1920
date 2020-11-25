#include "giocatore.h"

/* globali */
pawn *sm_pawns_team;
char *sm_char_cb;
flag *sm_flags;
int sm_id_team /* id mc array pedine della squadra */
    , msg_id_queue /* id della coda di messaggi */
    , sm_id_cb /* id mc dell'array di caratteri della scacchiera */
    , sem_id_cb /* id set di semafori per l'array della scacchiera */
    , sm_id_flags /* id mc delle bandiere del round */
    , num_flags_round /* numero delle bandiere di un singolo round */
    , pos_token /* numero del giocatore */
    , token_players; /* id del semaforo usato per piazzare le pedine e per gestione del round */
pid_t *pids_pawns;
int end_game;

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

    get_config(argv[1]);

    srand(time(NULL) + getpid());
    
    new_signal_handler.sa_handler = &signal_handler;
	new_signal_handler.sa_flags = 0;
	sigemptyset(& new_signal_handler.sa_mask);
    
    sigaction(SIGCHLD, &new_signal_handler, 0);
	TEST_ERROR
    sigaction(SIGUSR1, &new_signal_handler, 0);
	TEST_ERROR
    // sigaction(SIGSTOP, &new_signal_handler, 0);
	// TEST_ERROR
    // sigaction(SIGCONT, &new_signal_handler, 0);
	// TEST_ERROR

    token_players = atoi(argv[2]);
    pos_token = atoi(argv[3]);
    sem_id_cb = atoi(argv[4]);
    sm_id_team = atoi(argv[5]);
    msg_id_queue = atoi(argv[6]);
    sm_id_cb = atoi(argv[7]);

    /* collegamento a mem cond */
    sm_pawns_team = (pawn *) shmat(sm_id_team, NULL, 0);
    TEST_ERROR;

    sm_char_cb = (char *) shmat(sm_id_cb, NULL, 0);
    TEST_ERROR;

    /* creazione pedine, valorizzazione pids_pawns */
    init_pawns(argv[1]);

    play_round();

    return 0;
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
        /* TODO CONTROLLARE */
        fscanf(fs, "%d%*[0]", &DIST_BAND);
    } else {
        printf("Errore apertura file di configurazione\n");
        exit(0);
    }

    fclose(fs);
}

void init_pawns(char *mode) {
    int i;
    char *param_pawns[11];
    char tmp_params[8][sizeof(char *)];
    struct sembuf sops;
    msg_conf warning_master;

    pids_pawns = (pid_t *) calloc(SO_NUM_P, sizeof(pid_t));

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
    param_pawns[0] = "./bin/pedina";
    param_pawns[1] = mode;
    sprintf(tmp_params[0], "%d", token_players);
    param_pawns[2] = tmp_params[0];
    sprintf(tmp_params[1], "%d", pos_token);
    param_pawns[3] = tmp_params[1];
    sprintf(tmp_params[2], "%d", sem_id_cb);
    param_pawns[4] = tmp_params[2];
    sprintf(tmp_params[3], "%d", sm_id_team);
    param_pawns[5] = tmp_params[3];
    sprintf(tmp_params[5], "%d", msg_id_queue);
    param_pawns[7] = tmp_params[5];
    sprintf(tmp_params[6], "%ld", (long) getppid());
    param_pawns[8] = tmp_params[6];
    sprintf(tmp_params[7], "%d", sm_id_cb);
    param_pawns[9] = tmp_params[7];
    param_pawns[10] = NULL;
    
    /* pedine ancora non piazzate con coord -1, -1 */
    for(i = 0; i < SO_NUM_P; i++) {
        sm_pawns_team[i].position.x = -1; 
        sm_pawns_team[i].position.y = -1;
        sm_pawns_team[i].objective.x = -1;
        sm_pawns_team[i].objective.y = -1;
        sm_pawns_team[i].id_flag = -1;
    }

    for(i = 0; i < SO_NUM_P; i++) {
        /* token per piazzamento pedine */
        sops.sem_num = pos_token;
        sops.sem_op = -1;
        sops.sem_flg = 0;
        semop(token_players, &sops, 1);

        #if DEBUG
        /* printf("gioc %d: ped %d piazzata\n", pos_token, i); */
        #endif

        place_pawn(i);
        
        /* 
        ultimo giocatore che piazza una pedina manda un messaggio al master
        che piazzerá a sua volta le bandierine, non rilascia primo token
        */
        if(i == (SO_NUM_P - 1) && pos_token == (SO_NUM_G - 1)) { 
        	warning_master.mtype = (long) getpid() + MSG_PLACEMENT;
            msgsnd(msg_id_queue, &warning_master, sizeof(msg_conf) - sizeof(long), 0);
            TEST_ERROR;

            #if DEBUG
            printf("gioc %d (%ld): ult ped %d, msg fine piazzam su coda %d\n", (pos_token + 1), (long) getpid(), i, msg_id_queue);
            #endif
        } else {
            sops.sem_num = (pos_token == (SO_NUM_G - 1)) ? 0 : pos_token + 1;
            sops.sem_op = 1;
            sops.sem_flg = 0;
            semop(token_players, &sops, 1);
            TEST_ERROR;
        }

        /* passo indice ciclo a pedina per accesso diretto a propria struttura in array */
        sprintf(tmp_params[4], "%d", i);
        param_pawns[6] = tmp_params[4];

        /* creazione proc pedine */
        pids_pawns[i] = fork();

        switch(pids_pawns[i]) {
            case -1:
                TEST_ERROR;
                exit(EXIT_FAILURE);
            case 0:
                execv("./bin/pedina", param_pawns);
                TEST_ERROR;
                exit(EXIT_FAILURE);
        }
    }
}

void place_pawn(int id_pawn) {
    coord box;
    struct sembuf sops;
    
    /* 
    randomizzo i valori fino a quando non trovo 
    una cella libera senza altre pedine "troppo vicine" 
    */
    do {
        box.y = rand() % SO_ALTEZZA;
        box.x = rand() % SO_BASE;
    } while(!check_pos_pawns(box, id_pawn));

    sops.sem_num = INDEX(box);
    sops.sem_op = -1;
    sops.sem_flg = 0;
    semop(sem_id_cb, &sops, 1);
    TEST_ERROR;

    /* valorizzazione array ped in mc */
    sm_pawns_team[id_pawn].remaining_moves = SO_N_MOVES;
    sm_pawns_team[id_pawn].position = box;

    /* piazzamento pedine su char scacchiera */
    sm_char_cb[INDEX(box)] = (pos_token + 1) + '0';
}

int check_pos_pawns(coord box, int num_pawns_placed) {
    int i;

    /* 
    per la prima controllo solo che non la casella 
    non sia occupata da una pedina nemica 
    */
    if(num_pawns_placed == 0 && sm_char_cb[INDEX(box)] != '0') return FALSE;

    /* 
    dalla seconda in poi controllo che ogni pedina sia "abbastanza lontana"
    dalle altre e che la casella non sia giá occupata
    */
    for(i = 0; i <= num_pawns_placed; i++)
        if(sm_char_cb[INDEX(box)] != '0'
            || calc_dist(box, sm_pawns_team[i].position) < DIST_PED) 
            return FALSE;

    return TRUE;
}


int calc_dist(coord cas1, coord cas2) {
    return abs(cas1.x - cas2.x) + abs(cas1.y - cas2.y);
}

void play_round() {
    msg_flag msg_new_band;
    int i;

    end_game = FALSE;

    do {
        #if DEBUG
        printf("gioc %d: ricezione msg band\n", (pos_token + 1));
        #endif

        /* ricezione messaggio con id di mc con bandiere */
        do {
            errno = 0;
            msgrcv(msg_id_queue, &msg_new_band, sizeof(msg_flag) - sizeof(long), (long) (getpid() + MSG_FLAG), 0);
        } while(errno == EINTR);

        sm_id_flags = msg_new_band.sm_id_flags;
        num_flags_round = msg_new_band.num_flags;

        for(i = 0; i < SO_NUM_P; i++) {
            sm_pawns_team[i].objective.x = -1;
            sm_pawns_team[i].objective.y = -1;
            sm_pawns_team[i].id_flag = -1;
        }

        #if DEBUG
        printf("gioc %d: messaggio fine band ricevuto %d\n", (pos_token + 1), sm_id_flags);
        #endif

        /* assegnazione obiettivi */
        init_objectives();

    } while(!end_game);
}

void init_objectives() {
    msg_conf msg_path, msg_objective;
    int i;

    sm_flags = (flag *) shmat(sm_id_flags, NULL, 0);
    TEST_ERROR;

    #if DEBUG
    printf("gioc %d: collegamento a ind bandiere %d riuscito\n", (pos_token + 1), sm_id_flags);
    #endif

    for(i = 0; i < num_flags_round; i++)
        scan_flag(i);

    shmdt(sm_flags);
    TEST_ERROR;

    for(i = 0; i < SO_NUM_P; i++) {
        if(sm_pawns_team[i].id_flag != -1) {
            msg_objective.mtype = (long) (pids_pawns[i] + MSG_OBJECTIVE);

            msgsnd(msg_id_queue, &msg_objective, sizeof(msg_conf) - sizeof(long), 0);
            TEST_ERROR;

            /* 
            msg per assicurarsi che prima dell'inizio di 
            un round tutte le pedine con un obiettivo
            calcolino il proprio percorso
            */
            msgrcv(msg_id_queue, &msg_path, sizeof(msg_conf) - sizeof(long), (long) (pids_pawns[i] + MSG_PATH), 0);
            TEST_ERROR;
        }
    }

    /* msg a master fine calcolo percorsi per inizio round */
    msg_path.mtype = (long) getppid() + MSG_PATH;
    msgsnd(msg_id_queue, &msg_path, sizeof(msg_conf) - sizeof(long), 0);

    #if DEBUG
    printf("gioc %d: invio msg a master inizio round\n", (pos_token + 1));
    #endif
}

void scan_flag(int id_flag) {
    int row, range_scan, team_pawn, pawns_count;
    coord check;

    team_pawn = FALSE;
    pawns_count = SO_NUM_P;
    range_scan = 0;

    do {
        range_scan++;
        /* scan riga da -range a +range */
        for(row = -range_scan; row <= range_scan; row++) {
            check.y = sm_flags[id_flag].position.y;
            check.y += row;
            if(check.y >= 0 && check.y < SO_ALTEZZA) {
                /* scan da sx a centro */
                check.x = sm_flags[id_flag].position.x;
                check.x -= (range_scan - abs(row));
                if(check.x >= 0 && sm_char_cb[INDEX(check)] == ((pos_token + 1) + '0')) {
                    pawns_count--;
                    team_pawn = assign_objective(check
                                            , id_flag
                                            , range_scan);
                }

                /* da dopo centro a dx */
                check.x += 2 * (range_scan - abs(row));
                if(check.x < SO_BASE 
                    && (range_scan - abs(row)) > 0 
                    && sm_char_cb[INDEX(check)] == ((pos_token + 1) + '0')) {
                    pawns_count--;
                    team_pawn = assign_objective(check
                                            , id_flag
                                            , range_scan);
                }
            }
        }

    /* 
    esco dal ciclo se una pedina é stata assegnata alla 
    bandiera oppure se nessuna delle pedine puó arrivarci
    */
    } while(!team_pawn && pawns_count);
    //if(pawns_count == 0) kill(getppid(), SIGINT);
}

int assign_objective(coord check, int id_flag, int required_moves) {
    int id_pawn;

    id_pawn = 0;

    while(calc_dist(sm_pawns_team[id_pawn].position, check)) id_pawn++;
    
    if(sm_pawns_team[id_pawn].remaining_moves < required_moves) /* se la pedina non ha abbastanza mosse */
        return FALSE;

    if(sm_pawns_team[id_pawn].id_flag != -1) { /* se la pedina ha giá un obiettivo assegnato */
        if(sm_flags[id_flag].points <= sm_flags[sm_pawns_team[id_pawn].id_flag].points) /* se la bandiera giá assegnata vale di piú di quella nuova */
            return FALSE; /* non assegno pedina */

        /* 
        nel caso una pedina venisse riassegnata ne 
        cerco un'altra che possa prendere quella bandiera 
        (se disponibile)
        */
        scan_flag(sm_pawns_team[id_pawn].id_flag);
    }

    sm_pawns_team[id_pawn].objective = sm_flags[id_flag].position;
    sm_pawns_team[id_pawn].id_flag = id_flag;

    #if DEBUG
    printf("gioc %d: band %d assegnata a ped\n", pos_token, id_flag);
    #endif

    return TRUE;
}

void signal_handler(int signal_number) {
    int i, status, kidpid_ped;
    
    errno = 0;

    switch(signal_number){
        case SIGUSR1:
            end_game = TRUE;
            while(TRUE) pause();
            break;

        case SIGCHLD:
            while((kidpid_ped = waitpid(-1, &status, WNOHANG)) > 0) {
				if (WEXITSTATUS(status) != 0) {
					printf("OPS: kid %d is dead status %d\n", kidpid_ped, WEXITSTATUS(status));
				}
			}

			if(errno == ECHILD) {
				printf("\npedine ammazzate tutte!!!!!\n");
                shmdt(sm_char_cb);
                shmdt(sm_pawns_team);
                exit(EXIT_SUCCESS);
			}
            break;
    }

    //signal(SIGUSR2, SIG_DFL);
    
    /* attesa terminazione di tutte le pedine */
    // while(wait(&status) > 0);

    // shmdt(sm_char_cb);

    // shmdt(sm_pawns_team);

    // exit(EXIT_SUCCESS);
}

#if DEBUG
void test_config() {
    if(SO_NUM_G)
        printf("gioc: valori configurazione correttamente estratti\n");
    else {
        printf("gioc: errore valorizzazione configurazione\n");
        exit(EXIT_FAILURE);
    }
}
#endif