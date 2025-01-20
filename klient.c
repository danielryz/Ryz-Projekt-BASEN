// klient.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("Klient pojawił się. PID: %d\n", getpid());
    // Symulacja aktywności klienta
    sleep(10);
    printf("Klient opuszcza pływalnię. PID: %d\n", getpid());
    return 0;
}

