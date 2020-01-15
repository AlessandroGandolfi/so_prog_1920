cflags = -std=c89 -pedantic
mode ?= ""

debug: clean master giocatore run

buildall: clean master giocatore pedina

clean:
	rm -f *o master giocatore pedina *~

master: master.c config.h Makefile
	gcc $(cflags) master.c -o master

giocatore: giocatore.c config.h Makefile
	gcc $(cflags) giocatore.c -o giocatore

pedina: pedina.c config.h Makefile
	gcc $(CFLAGS) pedina.c -o pedina

all: buildall run

run:
	./master $(mode)