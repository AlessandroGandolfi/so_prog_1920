#cflags = -std=c89 -pedantic
mode ?= ""

debug: clean master giocatore pedina run

buildall: master giocatore pedina

clean:
	rm -f *o master giocatore pedina *~
	ipcrm -a
	pkill -f ./pedina
	pkill -f ./giocatore

master: master.c header.h Makefile
	gcc $(cflags) master.c -o master

giocatore: giocatore.c header.h Makefile
	gcc $(cflags) giocatore.c -o giocatore

pedina: pedina.c header.h Makefile
	gcc $(cflags) pedina.c -o pedina

all: buildall run

run:
	./master $(mode)