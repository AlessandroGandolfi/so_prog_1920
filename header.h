#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <math.h>
#include <dirent.h>

#define TRUE 1
#define FALSE 0

/* 
flag opzionali 
- DEBUG per stampare piú informazioni
- ENABLE_COLORS per abilitare o meno i colori in console
*/
#define DEBUG 0
#define PRINT_SCAN 0
#define ENABLE_COLORS 1

#define MSG_OBIETTIVO 10001
#define MSG_PERCORSO 10002
#define MSG_BANDIERA 10003
#define MSG_PIAZZAMENTO 10004

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

typedef struct _coordinate {
    int x;
    int y;
} coord;

typedef struct _bandiera {
    coord pos_band;
    int punti;
    int presa;
} band;

typedef struct _pedina {
    coord pos_attuale;
    coord obiettivo;
    int id_band;
    int mosse_rim;
} ped;

typedef struct _giocatore {
    pid_t pid;
    int punteggio;
    int mc_id_squadra;
    int tot_mosse_rim;
} gioc;

/* messaggio usato per segnalare una bandierina presa e id mc bandiere */
typedef struct _msg_bandiera {
    long mtype;
    int ind;
    int num_band;
} msg_band;

/* messaggio usato per segnalare una bandiera presa */
typedef struct _msg_bandiera_presa {
    long mtype;
    int id_band;
    int pos_token;
} msg_band_presa;

/* 
messaggio di conferma generale usato per
    fine piazzamento pedine da giocatore a master
    fine calcolo percorso da pedine a giocatore
*/
typedef struct _msg_conferma {
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

int calcDist(coord, coord);
void getConfig(char *);

#if DEBUG
void testSemToken(int);
void testConfig();
#endif