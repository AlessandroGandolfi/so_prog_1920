CFLAGS = -std=c89 -pedantic

all: clean master giocatore pedina

clean:
	rm -f *o master giocatore pedina *~

master: master.c config.h Makefile
	gcc $(CFLAGS) master.c -o master -D$(MODE)

giocatore: giocatore.c config.h Makefile
	gcc $(CFLAGS) giocatore.c -o giocatore -D$(MODE)

pedina: pedina.c config.h Makefile
	gcc $(CFLAGS) pedina.c -o pedina -D$(MODE)

run: all
	.\master