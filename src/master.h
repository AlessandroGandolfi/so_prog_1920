#include "header.h"


/* clear console, supportato solo su UNIX */
#if ((defined (LINUX) || defined (__linux__) || defined (__APPLE__)) && !DEBUG)
    #define CLEAR printf("\033[2J\033[H");
#endif

/* controllo su numero parametri esatti e corretta modalitá selezionata
param: 
- numero parametri passati a lancio esecuzione
- modalitá selezionata (def a "" in makefile) */
void checkMode(int, char *);

/* init risorse globali (memorie condivise, coda msg, semafori, ...) */
void initRisorse();

/* creazione ed esecuzione giocatori, creazione e valorizzazione token.
param:
- modalitá selezionata */
void initGiocatori(char *);

/* init array semafori scacchiera, tutti a 1 */
void initSemScacchiera();

/* eliminazione risorse (memorie condivise, coda msg, semafori, ...) */
void somebodyTookMaShmget();

/* stampa scacchiera */
void stampaScacchiera();

/* calcola mosse rimanenti per squadra */
void remaining_moves();

/* stampa informazioni per round */
void print_info_round();

/* inizializzazione parametri per creazione giocatori:
    0 - path relativo file giocatore
    1 - difficoltá gioco (per config)
    2 - id token
    3 - indice token squadra
    4 - id sem scacchiera
    5 - id mc squadra, array pedine
    6 - id coda msg
    7 - id mc char scacchiera */
void init_params_gamers(char*, char**, char [][sizeof(char *)]);

/* creazione dei processi giocatori */
void create_gamers(char**, char [][sizeof(char *)]);

/* aspetta il messaggio di bandiera presa */
void wait_flag_taken(int);

/* piazzamento nuove bandiere e distribuzione loro punteggio.
    param:
    - id token giocatori (usato successivamente per partenza round) */
int initBandiere();

/* valorizzazione mc bandiere, assegnazione punti bandiere e 
    piazzamento su scacchiera */
void value_and_place_flag(int, int, int, coord);

/* invia messaggio a giocatori per piazzamento bandierina */
void send_msg_new_band(int, int, msg_band);

/* in ricezione di messaggio di fine calcolo percorso pedina*/
void rcv_msg_route_calculation(msg_conf);

/* controlli per piazzamento nuova bandierina su posizione delle 
    bandierine giá piazzate e su cella libera da pedine
    TRUE se supera controlli, FALSE altrimenti
    param:
    - coordinate generate
    - numero di bandierine da piazzare questo round */
int checkPosBandiere(coord, int);