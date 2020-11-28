#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <math.h>
#include <dirent.h>
#include <asm-generic/errno-base.h>

#define TRUE 1
#define FALSE 0

/* 
flag opzionali 
- DEBUG per stampare piú informazioni generali
- DEBUG_BAND info riguardo bandiere prese e punteggio (solo in easy e debug)
- PRINT_SCAN vedere area scan per assegnazione obiettivi (crash)
- ENABLE_COLORS per abilitare o meno i colori in console
*/
#define DEBUG 0
#define DEBUG_BAND 0
#define PRINT_SCAN 0
#define ENABLE_COLORS 1

#define MSG_OBJECTIVE 11111
#define MSG_PATH 22222
#define MSG_FLAG 33333
#define MSG_PLACEMENT 44444

#define INDEX(coord) (coord.y * SO_BASE) + coord.x

#define TEST_ERROR  if(errno) {fprintf(stderr, \
                    "%s:%d: PID=%5d: Error %d (%s)\n",\
                    __FILE__,\
                    __LINE__,\
                    getpid(),\
                    errno,\
                    strerror(errno));\
                    errno = 0;}

#ifndef __APPLE__
typedef union {
    int val; /* Value for SETVAL */
    struct semid_ds *buf; /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
#if (defined (LINUX) || defined (__linux__))
    struct seminfo *__buf; /* Buffer for IPC_INFO (Linux-specific) */
#endif
} semun;
#endif

typedef struct _coordinates {
    int x;
    int y;
} coord;

typedef struct _flag {
    coord position;
    int points;
} flag;

typedef struct _pawn {
    coord position;
    coord objective;
    int id_flag;
    int remaining_moves;
} pawn;

typedef struct _player {
    pid_t pid;
    int points;
    int sm_id_team;
    int total_rem_moves;
} player;

/* 
messaggio usato per segnalare nuova id mc bandiere 
e nuovo numero bandiere del prossimo round
*/
typedef struct _msg_flag {
    long mtype;
    int sm_id_flags;
    int num_flags;
} msg_flag;

/* messaggio usato per segnalare una bandiera presa */
typedef struct _msg_taken_flag {
    long mtype;
    int id_flag;
    int pos_token;
} msg_t_flag;

/* 
messaggio di conferma generale usato per
    fine piazzamento pedine da giocatore a master
    fine calcolo percorso da pedine a giocatore
*/
typedef struct _msg_confirmation {
    long mtype;
} msg_conf;

int SO_NUM_G;
int SO_NUM_P;
int SO_MAX_TIME;
int SO_BASE;
int SO_ALTEZZA;
int SO_FLAG_MIN;
int SO_FLAG_MAX;
int SO_ROUND_SCORE;
int SO_N_MOVES;
int SO_MIN_HOLD_NSEC;
int DIST_PED;
int DIST_BAND;

/* calcolo distanza tra due coordinate con distanza di manhattan */
int calc_dist(coord, coord);

/* 
valorizzazione globali da file secondo modalitá
param: 
- modalitá selezionata 
*/
void get_config(char *);

/*
funzione per gestire i segnali ricevuti 
param:
- numero identificativo del segnale
*/
void signal_handler(int);

/* funzione per la gestione dei round */
void play_round();

#if DEBUG
/* stampa dei valori del semaforo token */
void test_sem_token();
/* stampa valori configurazione selezionata */
void test_config();
#endif