# Makefile

CC = gcc
CFLAGS = -pthread

all: klient kasjer ratownik basen

klient:
	$(CC) $(CFLAGS) -o klient klient.c

kasjer:
	$(CC) $(CFLAGS) -o kasjer kasjer.c

ratownik:
	$(CC) $(CFLAGS) -o ratownik ratownik.c

basen:
	$(CC) $(CFLAGS) -o basen basen.c

clean:
	rm -f klient kasjer ratownik basen wynik_symulacji.txt
	
run:
	./basen
