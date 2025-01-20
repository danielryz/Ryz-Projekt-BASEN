# Makefile dla projektu Basen

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11

# Lista programów do skompilowania
TARGETS = main ratownik kasjer klient

all: $(TARGETS)

main: main.o
	$(CC) $(CFLAGS) -o main main.o

ratownik: ratownik.o
	$(CC) $(CFLAGS) -o ratownik ratownik.o

kasjer: kasjer.o
	$(CC) $(CFLAGS) -o kasjer kasjer.o

klient: klient.o
	$(CC) $(CFLAGS) -o klient klient.o

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(TARGETS)

