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

#include "./config.h"

int main(int argc, char **argv) {
    /* id sm da parametri */
    int shid = atoi(argv[1]);
    /* collegamento a sm */
    int *shared_data = shmat(shid, NULL, 0);

    printf("%s: %d, val: %d\n", argv[0], getpid(), *shared_data);
    exit(EXIT_SUCCESS);
}