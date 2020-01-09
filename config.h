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

const int SO_NUM_G;
const int SO_NUM_P;
const int SO_MAX_TIME;
const int SO_BASE;
const int SO_ALTEZZA;
const int SO_FLAG_MIN;
const int SO_FLAG_MAX;
const int SO_ROUND_SCORE;
const int SO_N_MOVES;
const int SO_MIN_HOLD_NSEC;
const int DIST_PED_GIOC;

#define INIT_ENV(mode) \
/* overwrite abilitato nel caso progetto precedente non le rimuova */ \
setenv("SO_NUM_G", (mode == "easy") ? "2" : "4", 1); \
setenv("SO_NUM_P", (mode == "easy") ? "10" : "400", 1); \
setenv("SO_MAX_TIME", (mode == "easy") ? "3" : "1", 1); \
setenv("SO_BASE", (mode == "easy") ? "60" : "120", 1); \
setenv("SO_ALTEZZA", (mode == "easy") ? "20" : "40", 1); \
setenv("SO_FLAG_MIN", (mode == "easy") ? "5" : "5", 1); \
setenv("SO_FLAG_MAX", (mode == "easy") ? "5" : "40", 1); \
setenv("SO_ROUND_SCORE", (mode == "easy") ? "10" : "200", 1); \
setenv("SO_N_MOVES", (mode == "easy") ? "20" : "200", 1); \
setenv("SO_MIN_HOLD_NSEC", "100000000", 1); \
setenv("DIST_PED_GIOC", (mode == "easy") ? "8" : "2", 1); \

#define GET_CONFIG \
const int SO_NUM_G = atoi(getenv("SO_NUM_G")); \
const int SO_NUM_P = atoi(getenv("SO_NUM_P")); \
const int SO_MAX_TIME = atoi(getenv("SO_MAX_TIME")); \
const int SO_BASE = atoi(getenv("SO_BASE")); \
const int SO_ALTEZZA = atoi(getenv("SO_ALTEZZA")); \
const int SO_FLAG_MIN = atoi(getenv("SO_FLAG_MIN")); \
const int SO_FLAG_MAX = atoi(getenv("SO_FLAG_MAX")); \
const int SO_ROUND_SCORE = atoi(getenv("SO_ROUND_SCORE")); \
const int SO_N_MOVES = atoi(getenv("SO_N_MOVES")); \
const int SO_MIN_HOLD_NSEC = atoi(getenv("SO_MIN_HOLD_NSEC")); \
const int DIST_PED_GIOC = atoi(getenv("DIST_PED_GIOC")); \

#define RM_ENV \
unsetenv("SO_NUM_G"); \
unsetenv("SO_NUM_P"); \
unsetenv("SO_MAX_TIME"); \
unsetenv("SO_BASE"); \
unsetenv("SO_ALTEZZA"); \
unsetenv("SO_FLAG_MIN"); \
unsetenv("SO_FLAG_MAX"); \
unsetenv("SO_ROUND_SCORE"); \
unsetenv("SO_N_MOVES"); \
unsetenv("SO_MIN_HOLD_NSEC"); \
unsetenv("DIST_PED_GIOC"); \

#define DIST_BAND 8 /* < 10 */

/* 
flag opzionali 
- DEBUG per stampare piú informazioni
- ENABLE_COLORS per abilitare o meno i colori in console
*/
#define DEBUG 1
#define ENABLE_COLORS 1

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

int calcDist(int x1, int x2, int y1, int y2) {
    int distanza, dif_riga, dif_col, dif_min, dif_max;

    dif_riga = abs(y1 - y2);
    dif_col = abs(x1 - x2);
    
    dif_min = (dif_col <= dif_riga) ? dif_col : dif_riga;
    dif_max = (dif_col > dif_riga) ? dif_col : dif_riga;
    distanza = ((int) sqrt(2)) * dif_min + (dif_max - dif_min);

    return distanza;
}