#include "header.h"

/*
creazione e posizionamento pedine, sincronizzazione con token
param:
- id token sem
- posizione token squadra
- modalitá selezionata
*/
void init_pawns(char *);

/* 
inizializzazione parametri per creazione pedine
param:
- modalitá selezionata
- "array di stringhe"/matrice di char che verrá passato al processo pedina
- matrice char usata per allocazione memoria e valorizzazione parametri
*/
void init_params_pawns(char *, char **, char [][sizeof(char *)]);

/*
gestione del turno del giocatore per piazzare una pedina
param:
- id pedina
*/
void placement_turn(int);

/*
piazzamento singola pedina
param:
- numero identificativo pedina di array
*/
void place_pawn(int);

/*
controlli per piazzamento nuova pedina su posizione delle 
pedine giá piazzate e su cella libera
TRUE se supera controlli, FALSE altrimenti
param:
- coordinate generate
- numero delle pedine giá piazzate
*/
int check_pos_pawns(coord, int);

/* assegnazione degli obiettivi alle pedine */
void init_objectives();

/*
funzione per guardare pedine piú vicine alle bandiere
e se possibile assegnarle
param:
- id bandiera
*/
void scan_flag(int);

/*
controlli per assegnazione bandiera a pedina,
le bandiere con punteggi piú alti hanno la prioritá
param:
- coordinate della pedina trovata vicino alla bandiera
- id bandiera
- mosse richieste per raggiungere la bandiera
*/
int assign_objective(coord, int, int);