
mode ?= ""

debug: clean master giocatore pedina run

buildall: clean master giocatore pedina

clean:
	rm -f *o master giocatore pedina *~

master: master.c header.h Makefile
	gcc $(cflags) master.c -o master

giocatore: giocatore.c header.h Makefile
	gcc $(cflags) giocatore.c -o giocatore

pedina: pedina.c header.h Makefile
	gcc $(CFLAGS) pedina.c -o pedina

all: buildall run

run:
	./master $(mode)