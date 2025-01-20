// kasjer.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("Kasjer uruchomiony. PID: %d\n", getpid());
    for (int j = 0; j < 5; j++) {  // Ograniczamy pętlę do 5 iteracji
        // Logika obsługi klientów
        sleep(5); // symulacja pracy kasjera
    }
    printf("Kasjer kończy pracę. PID: %d\n", getpid());
    return 0;
}

