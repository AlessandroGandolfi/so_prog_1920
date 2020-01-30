/* 
i movimenti vengono decisi dalla pedina una volta che le viene assegnato un obiettivo dal giocatore
si puó usare lista per salvare movimenti da eseguire
    una volta eseguito un movimento viene eliminato dalla lista
se la pedina incontra un ostacolo puó decidere di ricalcolare il percorso o aspettare
    ricalcolo consigliato per evitare pedina senza mosse di altri giocatori
        deviazione del percorso sempre verso il centro della scacchiera
    semtimedop ogni 3 (?) ricalcoli
se la pedina non puó raggiungere piú il proprio obiettivo aspetta che il giocatore gliene assegni un altro
    il giocatore potrebbe ridare lo stesso obiettivo se
        non é stata reclamata
            pedina (qualsiasi) solo di passaggio
        la pedina non ha abbastanza mosse rimanenti da raggiungerne un'altra
            potrebbe ostacolare pedine nemiche
se la pedina finisce su una bandierina non obiettivo si ferma su di essa se
    non é l'unica pedina verso il proprio obiettivo
    vale di piú di quella obiettivo (improbabile ?)
    ce n'era un altro ma mi sono dimenticato
se non ha abbastanza mosse per raggiungere nessun obiettivo rimane ferma
*/

#include "header.h"

void waitObj();
void calcPercorso(int);

/* globali */
ped *mc_ped_squadra;
char *mc_char_scac;
band *mc_bandiere;
int mc_id_squadra, msg_id_coda, mc_id_scac, sem_id_scac, ind_ped_sq;
coord *percorso;

/* 
parametri a pedine
0 - path relativo file pedina
1 - difficoltá gioco (per config)
2 - id token
3 - indice token squadra
4 - id sem scacchiera
5 - id mc squadra, array pedine
6 - indice identificativo pedina dell'array in mc squadra
7 - id coda msg
*/
int main(int argc, char **argv) {
    struct timespec arg_sleep;

    mc_id_squadra = atoi(argv[5]);
    ind_ped_sq = atoi(argv[6]);
    msg_id_coda = atoi(argv[7]);

    mc_ped_squadra = (ped *) shmat(mc_id_squadra, NULL, 0);

    waitObj();

    arg_sleep.tv_sec = 0;
    arg_sleep.tv_nsec = SO_MIN_HOLD_NSEC;
    nanosleep(&arg_sleep, NULL);

    exit(EXIT_SUCCESS);
}

void waitObj() {
    msg_new_obj msg_obj;

    /* ricezione messaggio con id di mc con bandiere */
    if(msgrcv(msg_id_coda, &msg_obj, sizeof(msg_new_obj) - sizeof(long), getpid(), 0) == -1) TEST_ERROR;
    
    if(msg_obj.band_assegnata) calcPercorso(msg_obj.mc_id_band);
    else waitObj();
}

void calcPercorso(int mc_id_band) {
    int i, num_mosse;
    band *mc_bandiere;
    coord cont;
    mc_bandiere = (band *) shmat(mc_id_band, NULL, 0);

    /* 
    nel caso di bandierina o casella occupata nel corso del round
    elimino e rialloco array di mosse
    */
    if(percorso) free(percorso);
    
    num_mosse = calcDist(mc_ped_squadra[ind_ped_sq].pos_attuale, mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band);

    /* alloco array locale di grandezza = numero di mosse necessarie a raggiungere bandiera */
    percorso = (coord *) calloc(num_mosse, sizeof(coord));
    
    /*calcolo il percorso della pedina*/
    for(i=0;i<num_mosse;i++){
        if(mc_ped_squadra[ind_ped_sq].pos_attuale.x-mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.x==0){
            if(mc_ped_squadra[ind_ped_sq].pos_attuale.y-mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.y==0){
                cont.x=mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.x;
                cont.y=mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.y;
                percorso[i]=cont;
            }
            else if(mc_ped_squadra[ind_ped_sq].pos_attuale.y-mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.y>0){
                cont.x=mc_ped_squadra[ind_ped_sq].pos_attuale.x;
                cont.y=mc_ped_squadra[ind_ped_sq].pos_attuale.y-1;
                percorso[i]=cont;
            }
            else{
                cont.x=mc_ped_squadra[ind_ped_sq].pos_attuale.x;
                cont.y=mc_ped_squadra[ind_ped_sq].pos_attuale.y+1;
                percorso[i]=cont;
            }    
        }
        else if(mc_ped_squadra[ind_ped_sq].pos_attuale.x-mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.x>0){
            cont.x=mc_ped_squadra[ind_ped_sq].pos_attuale.x-1;
            cont.y=mc_ped_squadra[ind_ped_sq].pos_attuale.y;
            percorso[i]=cont;
        }
        else{
            cont.x=mc_ped_squadra[ind_ped_sq].pos_attuale.x+1;
            cont.y=mc_ped_squadra[ind_ped_sq].pos_attuale.y;
            percorso[i]=cont;
        }
    }


}

/* dist manhattan */
int calcDist(coord cas1, coord cas2) {
    return abs(cas1.x - cas2.x) + abs(cas1.y - cas2.y);
}