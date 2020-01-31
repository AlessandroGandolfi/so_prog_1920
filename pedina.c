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
struct timespec arg_sleep;
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
    cont=mc_ped_squadra[ind_ped_sq].pos_attuale;
    num_mosse = calcDist(mc_ped_squadra[ind_ped_sq].pos_attuale, mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band);
    printf("numero mosse: %d\n", num_mosse);
    /* alloco array locale di grandezza = numero di mosse necessarie a raggiungere bandiera */
    percorso = (coord *) calloc(num_mosse, sizeof(coord));
    
    /*calcolo il percorso della pedina*/
    for(i=0;i<num_mosse;i++){
        if(cont.x-mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.x==0){
            if(cont.y-mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.y==0){
                percorso[i].x=mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.x;
                percorso[i].y=mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.y;
                
            }
            else if(cont.y-mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.y>0){
                percorso[i].x=cont.x;
                percorso[i].y=cont.y-1;
            }
            else{
                percorso[i].x=cont.x;
                percorso[i].y=cont.y+1;
            }    
        }
        else if(cont.x-mc_bandiere[mc_ped_squadra[ind_ped_sq].obiettivo].pos_band.x>0){
            percorso[i].x=cont.x-1;
            percorso[i].y=cont.y;
            
        }
        else{
            percorso[i].x=cont.x+1;
            percorso[i].y=cont.y;
        }
        cont=percorso[i];
    }
    #if DEBUG
        for(i=0;i<num_mosse;i++){
            printf("ped: %d, %d, coord: %d %d\n",mc_ped_squadra[ind_ped_sq].pos_attuale.x,mc_ped_squadra[ind_ped_sq].pos_attuale.y,percorso[i].x, percorso[i].y);
        }
    #endif
}

/*void muoviPedina(){
    int i, dim;
    struct sembuf sops;
    ped pedina;
    arg_sleep.tv_sec = 0;
    arg_sleep.tv_nsec = SO_MIN_HOLD_NSEC;
    dim=sizeof(percorso)/sizeof(percorso[0]);
    for(i=0;i<dim;i++){
        if(mc_char_scac[INDEX(percorso[i])]=='0'){
            sops.sem_num = INDEX(percorso[i]);
            sops.sem_op = -1;
            sops.sem_flg = 0;
            semop(sem_id_scac, &sops, 1);

            sops.sem_num = INDEX(pedina.pos_attuale);
	        sops.sem_op = 1;
	        sops.sem_flg = 0;
	
	        semop(sem_id_scac, &sops, 1);

            nanosleep(&arg_sleep, NULL);

            pedina.pos_attuale=percorso[i];
            pedina.mosse_rim=-1;
        }
        else if(mc_char_scac[INDEX(percorso[i])]=="B"){


        }
        else {

        }
        

    }
}*/

/* dist manhattan */
int calcDist(coord cas1, coord cas2) {
    return abs(cas1.x - cas2.x) + abs(cas1.y - cas2.y);
}
void getConfig(char *mode) {
    FILE *fs;
    char *config_file;

    config_file = (char *) malloc(sizeof(char));
    strcpy(config_file, "./config/");
    strcat(config_file, mode);
    strcat(config_file, ".txt");
    
    #if DEBUG
    printf("path file conf: %s\n", config_file);
    #endif

    fs = fopen(config_file, "r");

    if(fs) {
        fscanf(fs, "%d%*[^\n]", &SO_NUM_G);
        fscanf(fs, "%d%*[^\n]", &SO_NUM_P);
        fscanf(fs, "%d%*[^\n]", &SO_MAX_TIME);
        fscanf(fs, "%d%*[^\n]", &SO_BASE);
        fscanf(fs, "%d%*[^\n]", &SO_ALTEZZA);
        fscanf(fs, "%d%*[^\n]", &SO_FLAG_MIN);
        fscanf(fs, "%d%*[^\n]", &SO_FLAG_MAX);
        fscanf(fs, "%d%*[^\n]", &SO_ROUND_SCORE);
        fscanf(fs, "%d%*[^\n]", &SO_N_MOVES);
        fscanf(fs, "%d%*[^\n]", &SO_MIN_HOLD_NSEC);
        fscanf(fs, "%d%*[^\n]", &DIST_PED);
        fscanf(fs, "%d%*[^\0]", &DIST_BAND);
    } else {
        printf("Errore apertura file di configurazione\n");
        exit(0);
    }

    fclose(fs);
}