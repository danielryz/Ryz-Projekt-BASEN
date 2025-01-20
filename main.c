// main.c
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#define LICZBA_RATOWNIKÓW 3  // na przykład dla 3 basenów
#define ILOSC_KLIENTÓW 10    // liczba klientów do wygenerowania

int main() {
    pid_t pid;
    int i;

    // Tworzenie procesów ratowników
    for (i = 0; i < LICZBA_RATOWNIKÓW; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork ratownik error");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Proces potomny uruchamiający ratownika
            execlp("./ratownik", "ratownik", NULL);
            perror("execlp ratownik error");
            exit(EXIT_FAILURE);
        }
    }

    // Tworzenie procesu kasjera
    pid = fork();
    if (pid < 0) {
        perror("fork kasjer error");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        execlp("./kasjer", "kasjer", NULL);
        perror("execlp kasjer error");
        exit(EXIT_FAILURE);
    }

    // Generowanie kilku klientów na próbę
    for (i = 0; i < ILOSC_KLIENTÓW; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork klient error");
        } else if (pid == 0) {
            execlp("./klient", "klient", NULL);
            perror("execlp klient error");
            exit(EXIT_FAILURE);
        }
        // Krótka przerwa między uruchomieniami klientów
        usleep(500000); // 0.5 sekundy
    }

    // Oczekiwanie na zakończenie wszystkich procesów potomnych
    while (wait(NULL) > 0);

    return 0;
}

