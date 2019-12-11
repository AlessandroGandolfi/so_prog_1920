#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
/* #include <signal.h> */
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <math.h>

#ifndef EASY
#ifndef HARD
#define EASY /* default facile */
#endif
#endif

#ifdef EASY
#define SO_NUM_G 2
#define SO_NUM_P 10
#define SO_MAX_TIME 3
#define SO_BASE 60
#define SO_ALTEZZA 20
#define SO_FLAG_MIN 5
#define SO_FLAG_MAX 5
#define SO_ROUND_SCORE 10
#define SO_N_MOVES 20
#define SO_MIN_HOLD_NSEC 100000000

#define DIST_PED_GIOC 60 /* < 10 */
#else
#ifdef HARD
#define SO_NUM_G 4
#define SO_NUM_P 400
#define SO_MAX_TIME 1
#define SO_BASE 120
#define SO_ALTEZZA 40
#define SO_FLAG_MIN 5
#define SO_FLAG_MAX 40
#define SO_ROUND_SCORE 200
#define SO_N_MOVES 200
#define SO_MIN_HOLD_NSEC 100000000

#define DIST_PED_GIOC 2 /* < 3 */
#endif
#endif

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

typedef union _semun{
    int val; /* Value for SETVAL */
    struct semid_ds *buf; /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* Array for GETALL, SETALL */
#if (defined (LINUX) || defined (__linux__))
    struct seminfo *__buf; /* Buffer for IPC_INFO (Linux-specific) */
#endif
} semun;

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

/* messaggio usato per segnalare una bandierina presa */
typedef struct _msg_band {
    long mtype;
    coord coord_band_presa;
} msg_band_presa;

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