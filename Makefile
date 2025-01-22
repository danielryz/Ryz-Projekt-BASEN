# Makefile dla projektu Basen

CC = gcc
CFLAGS = -pthread

# Lista programów do skompilowania
TARGETS = basen ratownik kasjer klient

all: $(TARGETS)

basen:
	$(CC) $(CFLAGS) -o basen basen.c

ratownik:
	$(CC) $(CFLAGS) -o ratownik ratownik.c

kasjer:
	$(CC) $(CFLAGS) -o kasjer kasjer.c

klient: klient.o
	$(CC) $(CFLAGS) -o klient klient.c

clean:
	rm -f $(TARGETS)

