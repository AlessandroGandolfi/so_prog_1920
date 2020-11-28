#include "pedina.h"

/* globali */
pawn *sm_pawns_team;
char *sm_char_cb;
long pid_master;
int sm_id_team
    , msg_id_queue
    , sm_id_cb
    , sem_id_cb
    , id_pawn_team
    , pos_token
    , token_players
    , id_move
    , end_game;
coord *path;

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
    token_players = atoi(argv[2]);
    pos_token = atoi(argv[3]);
    sem_id_cb = atoi(argv[4]);
    sm_id_team = atoi(argv[5]);
    id_pawn_team = atoi(argv[6]);
    msg_id_queue = atoi(argv[7]);
    pid_master = atol(argv[8]);
    sm_id_cb = atoi(argv[9]);

    get_config(argv[1]);

    signal(SIGUSR1, &signal_handler);
    TEST_ERROR

    sm_pawns_team = (pawn *) shmat(sm_id_team, NULL, 0);
    sm_char_cb = (char *) shmat(sm_id_cb, NULL, 0);

    play_round();

    return 0;
}

int wait_obj() {
    msg_conf msg_objective;

    errno = 0;

    /* ricezione messaggio con id di mc con bandiere */
    msgrcv(msg_id_queue, &msg_objective, sizeof(msg_conf) - sizeof(long), (long) (getpid() + MSG_OBJECTIVE), 0);
    TEST_ERROR
    
    #if DEBUG
    printf("ped %d: msg obiettivo %d ricevuto\n", (id_pawn_team + 1), sm_pawns_team[id_pawn_team].id_flag);
    #endif
    
    /* solo quelle con obiettivo ricevono il messaggio */
    return calc_path();    
}

int calc_path() {
    int i, num_moves;
    coord box_path;
    msg_conf msg_end_path;

    errno = 0;

    /* elimino (dal secondo round) e alloco array di mosse */
    if(path != NULL) free(path);
    num_moves = calc_dist(sm_pawns_team[id_pawn_team].position, sm_pawns_team[id_pawn_team].objective);
    path = (coord *) calloc(num_moves, sizeof(coord));
    
    box_path = sm_pawns_team[id_pawn_team].position;

    /*calcolo il path della pedina*/
    for(i = 0; i < num_moves; i++) {
        if((box_path.x - sm_pawns_team[id_pawn_team].objective.x) == 0) {
            /* mossa finale path */
            if((box_path.y - sm_pawns_team[id_pawn_team].objective.y) == 0)
                path[i] = sm_pawns_team[id_pawn_team].objective;
            /* mossa in alto */
            else if((box_path.y - sm_pawns_team[id_pawn_team].objective.y) > 0) {
                path[i].x = box_path.x;
                path[i].y = box_path.y - 1;
            }
            /* mossa in basso */
            else {
                path[i].x = box_path.x;
                path[i].y = box_path.y + 1;
            }    
        }
        /* mossa a sinistra */
        else if((box_path.x - sm_pawns_team[id_pawn_team].objective.x) > 0) {
            path[i].x = box_path.x - 1;
            path[i].y = box_path.y;
        }
        /* mossa a destra */
        else {
            path[i].x = box_path.x + 1;
            path[i].y = box_path.y;
        }
        box_path = path[i];
    }

    /* msg send fine calcolo path a giocatore prima di inizio round */
    msg_end_path.mtype = (long) (getpid() + MSG_PATH);
    msgsnd(msg_id_queue, &msg_end_path, sizeof(msg_conf) - sizeof(long), 0);
    TEST_ERROR

    #if DEBUG
    /* printf("ped %d: msg fine calc path a gioc %d\n", (id_pawn_team + 1), pos_token); */
    #endif

    return num_moves;
}

int move_pawn(int num_moves) {
    int flag_taken;
    struct sembuf sops;
    struct timespec arg_sleep;

    flag_taken = TRUE;

    for(id_move = 0; id_move < num_moves && flag_taken; id_move++) {
        /* prima di ogni mossa controlla dalla scacchiera che l'obiettivo non sia stato giá preso */
        if(sm_char_cb[INDEX(sm_pawns_team[id_pawn_team].objective)] == 'B'
            #if DEBUG_BAND
            || sm_char_cb[INDEX(sm_pawns_team[id_pawn_team].objective)] == (sm_pawns_team[id_pawn_team].id_flag + 'A')
            #endif
        ) {
            sops.sem_num = INDEX(path[id_move]);
            sops.sem_op = -1;
            sops.sem_flg = IPC_NOWAIT;

            /* prova ad eseguire mossa subito */
            if(semop(sem_id_cb, &sops, 1) == -1) {
        
                arg_sleep.tv_sec = 0;
                arg_sleep.tv_nsec = SO_MIN_HOLD_NSEC / 2;

                /* riprova dopo (SO_MIN_HOLD_NSEC / 2) se non riesce al primo tentativo */
                if(semtimedop(sem_id_cb, &sops, 1, &arg_sleep) == -1)
                    flag_taken = FALSE; /* richiesta nuovo obiettivo */
                else update_status();

            } else update_status();
        } else flag_taken = FALSE;  /* richiesta nuovo obiettivo se viene preso quello attualmente assegnato */
    }

    return flag_taken;
}

void update_status() {
    struct sembuf sops;
    struct timespec arg_sleep;

    errno = 0;

    sm_char_cb[INDEX(sm_pawns_team[id_pawn_team].position)] = '0';

    /* libera risorsa precedente */
    sops.sem_num = INDEX(sm_pawns_team[id_pawn_team].position);
    sops.sem_op = 1;
    sops.sem_flg = 0;
    semop(sem_id_cb, &sops, 1);
    TEST_ERROR
    
    /* aggiorna i propri dati nella memoria condivisa */
    sm_pawns_team[id_pawn_team].position = path[id_move];
    sm_pawns_team[id_pawn_team].remaining_moves--;
    
    sm_char_cb[INDEX(sm_pawns_team[id_pawn_team].position)] = (pos_token + 1) + '0';

    arg_sleep.tv_sec = 0;
    arg_sleep.tv_nsec = SO_MIN_HOLD_NSEC;
    nanosleep(&arg_sleep, NULL);
}


int calc_dist(coord box1, coord box2) {
    return abs(box1.x - box2.x) + abs(box1.y - box2.y);
}

void get_config(char *mode) {
    FILE *fs;
    char *config_file;

    config_file = (char *) malloc(sizeof(char) * 10);
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

    free(config_file);
    fclose(fs);
}

void play_round() {
    int num_moves;
    struct sembuf sops;
    msg_t_flag msg_taken;

    end_game = TRUE;

    do {
        num_moves = wait_obj();

        /* bloccate fino a quando round non é in corso */
        sops.sem_num = pos_token;
        sops.sem_op = 0;
        sops.sem_flg = 0;
        semop(token_players, &sops, 1);

        if(move_pawn(num_moves)) {
            msg_taken.id_flag = sm_pawns_team[id_pawn_team].id_flag;
            msg_taken.pos_token = pos_token;
            msg_taken.mtype = pid_master + (long) MSG_FLAG;

            errno = 0;
            
            /* msg a master per bandiera presa */
            msgsnd(msg_id_queue, &msg_taken, sizeof(msg_t_flag) - sizeof(long), 0);
            TEST_ERROR
        }
        
    } while(end_game);
}

void signal_handler(int signal_number) {
    errno = 0;

    end_game = FALSE;

    shmdt(sm_char_cb);
    TEST_ERROR
    shmdt(sm_pawns_team);
    TEST_ERROR
    signal(SIGUSR1, SIG_DFL);
    TEST_ERROR
    free(path);
    TEST_ERROR

    exit(EXIT_SUCCESS);
}
