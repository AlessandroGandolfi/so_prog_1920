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
piazzamento nuove bandiere e distribuzione loro punteggio
param:
- numero identificativo pedina di array
- posizione id token giocatori/squadra
*/
void place_pawn(int);

/*
controlli per piazzamento nuova pedina su posizione delle 
pedine giá piazzate e su cella libera
TRUE se supera controlli, FALSE altrimenti
param:
- coordinate generate
*/
int check_pos_pawns(coord, int);

/* assegnazione degli obiettivi alle pedine */
void init_objectives();

void scan_flag(int);

int assign_objective(coord, int, int);