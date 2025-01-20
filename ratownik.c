// ratownik.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("Ratownik uruchomiony. PID: %d\n", getpid());
    for (int j = 0; j < 5; j++) {  // Ograniczamy pętlę do 5 iteracji
        // Logika nadzorowania basenu
        sleep(5); // symulacja pracy ratownika
    }
    printf("Ratownik kończy pracę. PID: %d\n", getpid());
    return 0;
}

