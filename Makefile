# cflags = -std=c89 -pedantic
mode ?= ""

buildall: clock master giocatore pedina

clean:
	rm -f *o bin/master bin/giocatore bin/pedina *~
	ipcrm -a
	-pkill -f ./bin/pedina
	-pkill -f ./bin/giocatore

master: src/master.c src/header.h Makefile
	gcc $(cflags) src/master.c -o bin/master

giocatore: src/giocatore.c src/header.h Makefile
	gcc $(cflags) src/giocatore.c -o bin/giocatore

pedina: src/pedina.c src/header.h Makefile
	gcc $(cflags) src/pedina.c -o bin/pedina

clock:
	touch src/*.c src/*.h

all: buildall run

run:
	./bin/master $(mode)