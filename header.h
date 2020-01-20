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
#define DEBUG 1
#define ENABLE_COLORS 1

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

typedef struct _bandierina {
    coord pos_band;
    int punti;
    int presa;
} band;

typedef struct _pedina {
    band obiettivo;
    coord pos_attuale;
    coord *percorso;
    int mosse_rim;
} ped;

typedef struct _giocatore {
    pid_t pid;
    int punteggio;
    int mc_id_squadra;
    int tot_mosse_rim;
} gioc;

/* messaggio usato per segnalare una bandierina presa e id mc bandiere */
typedef struct _msg_band {
    long mtype;
    int ind;
} msg_band;

/* messaggio usato per segnalare bisogno nuovo obiettivo */
typedef struct _msg_nuovo_obiettivo {
    long mtype;
    long pid_ped;
    int ind_mc_ped;
} msg_new_obj;

/* messaggio usato per segnalare fine piazzamento */
typedef struct _msg_piaz {
    long mtype;
    int fine_piaz;
} msg_fine_piaz;

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