#include "header.h"

/* 
funzione bloccante fino a quando non viene 
assegnato un obiettivo alla pedine
*/
int wait_obj();

/* calcolo del percorso alla bandiera assegnata */
int calc_path();

/*
movimento della pedina
perim:
- numero mosse richieste (lunghezza del percorso)
*/
int move_pawn(int);

/* 
aggiornamento dei dati riguardo i movimenti delle pedine
(memoria condivisa di squadra, scacchiera di caratteri, numero di mosse, ...)
*/
void update_status();