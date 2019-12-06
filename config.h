#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/sem.h>

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
#endif
#endif

#define DEBUG 1

#define TEST_ERROR  if(errno) {fprintf(stderr, \
                    "%s:%d: PID=%5d: Error %d (%s)\n",\
                    __FILE__,\
                    __LINE__,\
                    getpid(),\
                    errno,\
                    strerror(errno));}

typedef union {
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
    int presa;
} band;