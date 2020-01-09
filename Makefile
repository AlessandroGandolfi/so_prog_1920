#CFLAGS = -std=c89 -pedantic

buildm: clean master

buildg: clean giocatore

buildp: clean pedina

buildall: clean master giocatore pedina

clean:
	rm -f *o master giocatore pedina *~

master: master.c config.h Makefile
	gcc $(CFLAGS) master.c -o master

giocatore: giocatore.c config.h Makefile
	gcc $(CFLAGS) giocatore.c -o giocatore

pedina: pedina.c config.h Makefile
	gcc $(CFLAGS) pedina.c -o pedina

run: ./master $(mode)