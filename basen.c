// basen.c
#include "naglowki.h"
#include "narzedzia.c"

#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void* steruj_czasem(void*);
static void posprzataj();
static void sygnaly_glowne(int);

pid_t pid_ratownicy, pid_kasjer, pid_klienci;
pthread_t th_time;
char* wsp_czas;
int shm_id_1, shm_id_2, sem_id, msq_1, msq_2;
volatile bool flaga_stop;

int main() {
    signal(SIGINT, sygnaly_glowne);
    signal(SIGTSTP, sygnaly_glowne);
    signal(SIGCONT, sygnaly_glowne);

    flaga_stop = false;

    key_t klucz = ftok(".", 51);
    if (klucz == -1) {
        perror("ftok 51 - glowny");
        exit(EXIT_FAILURE);
    }
    key_t klucz2 = ftok(".", 52);
    if (klucz2 == -1) {
        perror("ftok 52 - glowny");
        exit(EXIT_FAILURE);
    }

    sem_id = semget(klucz, 8, 0600 | IPC_CREAT);
    if (sem_id == -1) {
        perror("semget - glowny");
        exit(EXIT_FAILURE);
    }
    if (semctl(sem_id, 0, SETVAL, 1) == -1) { perror("semctl s0"); exit(EXIT_FAILURE); }
    if (semctl(sem_id, 1, SETVAL, 1) == -1) { perror("semctl s1"); exit(EXIT_FAILURE); }
    if (semctl(sem_id, 2, SETVAL, 0) == -1) { perror("semctl s2"); exit(EXIT_FAILURE); }
    if (semctl(sem_id, 3, SETVAL, 0) == -1) { perror("semctl s3"); exit(EXIT_FAILURE); }
    if (semctl(sem_id, 4, SETVAL, 1) == -1) { perror("semctl s4"); exit(EXIT_FAILURE); }
    if (semctl(sem_id, 5, SETVAL, 1) == -1) { perror("semctl s5"); exit(EXIT_FAILURE); }
    if (semctl(sem_id, 6, SETVAL, (POJ_OLIMPIJKA + POJ_REKREACJA + POJ_BRODZIK) * 3) == -1) {
        perror("semctl s6");
        exit(EXIT_FAILURE);
    }
    if (semctl(sem_id, 7, SETVAL, 1) == -1) { perror("semctl s7"); exit(EXIT_FAILURE); }

    msq_1 = msgget(klucz, IPC_CREAT | 0600);
    if (msq_1 == -1) {
        perror("msgget msq1 - glowny");
        exit(EXIT_FAILURE);
    }
    msq_2 = msgget(klucz2, IPC_CREAT | 0600);
    if (msq_2 == -1) {
        perror("msgget msq2 - glowny");
        exit(EXIT_FAILURE);
    }

    shm_id_1 = shmget(klucz, sizeof(DaneOsoby), 0600 | IPC_CREAT);
    if (shm_id_1 == -1) {
        perror("shmget1 - glowny");
        exit(EXIT_FAILURE);
    }

    shm_id_2 = shmget(klucz2, sizeof(int), 0600 | IPC_CREAT);
    if (shm_id_2 == -1) {
        perror("shmget2 - glowny");
        exit(EXIT_FAILURE);
    }
    wsp_czas = (char*)shmat(shm_id_2, NULL, 0);
    if (wsp_czas == (char*)(-1)) {
        perror("shmat - glowny");
        exit(EXIT_FAILURE);
    }
    *((int*)wsp_czas) = 0; // Start symulacji = 0 sekund - 10:00

    if (mkfifo("kana_bas_1", 0600) == -1 && errno != EEXIST) {
        perror("mkfifo1");
        exit(EXIT_FAILURE);
    }
    if (mkfifo("kana_bas_2", 0600) == -1 && errno != EEXIST) {
        perror("mkfifo2");
        exit(EXIT_FAILURE);
    }
    if (mkfifo("kana_bas_3", 0600) == -1 && errno != EEXIST) {
        perror("mkfifo3");
        exit(EXIT_FAILURE);
    }

    printf("<<GLOWNY>> Tworze procesy ratownicy, kasjer, klienci...\n");

    pid_ratownicy = fork();
    if (pid_ratownicy < 0) {
        perror("fork - ratownicy");
        exit(EXIT_FAILURE);
    }
    else if (pid_ratownicy == 0) {
        execl("./ratownik", "ratownik", NULL);
        perror("execl - ratownik");
        exit(EXIT_FAILURE);
    }
    else {
        pid_kasjer = fork();
        if (pid_kasjer < 0) {
            perror("fork - kasjer");
            kill(pid_ratownicy, SIGINT);
            exit(EXIT_FAILURE);
        }
        else if (pid_kasjer == 0) {
            execl("./kasjer", "kasjer", NULL);
            perror("execl - kasjer");
            exit(EXIT_FAILURE);
        }
        else {
            pid_klienci = fork();
            if (pid_klienci < 0) {
                perror("fork - klienci");
                kill(pid_ratownicy, SIGINT);
                kill(pid_kasjer, SIGINT);
                exit(EXIT_FAILURE);
            }
            else if (pid_klienci == 0) {
                execl("./klient", "klient", NULL);
                perror("execl - klient");
                exit(EXIT_FAILURE);
            }
        }
    }

    printf("Klienci PID=%d, kasjer PID=%d, ratownicy PID=%d\n\n",
        pid_klienci, pid_kasjer, pid_ratownicy);

    if (pthread_create(&th_time, NULL, steruj_czasem, NULL) != 0) {
        perror("pthread_create - steruj_czasem");
        exit(EXIT_FAILURE);
    }

    posprzataj();
    return 0;
}

static void* steruj_czasem(void* arg) {
    (void)arg;
    int* pczas = (int*)wsp_czas;

       // Petla trwa do 43200 + 900 = 44100 sekund - 10:00 - 22:15
     
    while (*pczas < (CALA_DOBA_SYM + 900) && !flaga_stop) {
        if (usleep(SEK_SYMULACJI) != 0 && errno != EINTR) {
            perror("usleep - steruj_czasem");
            exit(EXIT_FAILURE);
        }
        (*pczas)++;
    }

    // O 22:15 konczymy
    if (kill(-pid_klienci, SIGINT) != 0 && errno != ESRCH) {
        perror("kill klienci");
        exit(EXIT_FAILURE);
    }
    if (kill(pid_kasjer, SIGINT) != 0 && errno != ESRCH) {
        perror("kill kasjer");
        exit(EXIT_FAILURE);
    }
    if (kill(-pid_ratownicy, SIGINT) != 0 && errno != ESRCH) {
        perror("kill ratownicy");
        exit(EXIT_FAILURE);
    }

    return NULL;
}

static void posprzataj() {
    int status;
    pid_t ret;

    ret = waitpid(pid_klienci, &status, 0);
    if (ret == -1) {
        perror("waitpid klienci");
    }
    else if (WIFEXITED(status)) {
        printf("Klienci (PID=%d) zakonczyli sie z kodem %d\n", ret, WEXITSTATUS(status));
    }
    else {
        printf("Klienci (PID=%d) zakonczyli sie w nietypowy sposob, status=%d\n", ret, status);
    }

    ret = waitpid(pid_kasjer, &status, 0);
    if (ret == -1) {
        perror("waitpid kasjer");
    }
    else if (WIFEXITED(status)) {
        printf("Kasjer (PID=%d) zakonczyl sie z kodem %d\n", ret, WEXITSTATUS(status));
    }
    else {
        printf("Kasjer (PID=%d) zakonczyl sie w nietypowy sposob, status=%d\n", ret, status);
    }

    ret = waitpid(pid_ratownicy, &status, 0);
    if (ret == -1) {
        perror("waitpid ratownicy");
    }
    else if (WIFEXITED(status)) {
        printf("Ratownicy (PID=%d) zakonczyl sie z kodem %d\n", ret, WEXITSTATUS(status));
    }
    else {
        printf("Ratownicy (PID=%d) zakonczyl sie w nietypowy sposob, status=%d\n", ret, status);
    }

    pthread_join(th_time, NULL);

    if (unlink("kana_bas_1") == -1 || unlink("kana_bas_2") == -1 || unlink("kana_bas_3") == -1) {
        perror("unlink fifo");
    }

    if (shmctl(shm_id_1, IPC_RMID, 0) == -1) {
        perror("shmctl - 1");
    }
    if (shmctl(shm_id_2, IPC_RMID, 0) == -1) {
        perror("shmctl - 2");
    }
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("semctl rm - glowny");
    }
    if (msgctl(msq_1, IPC_RMID, 0) == -1) {
        perror("msgctl msq1");
    }
    if (msgctl(msq_2, IPC_RMID, 0) == -1) {
        perror("msgctl msq2");
    }
}

static void sygnaly_glowne(int sig) {
    if (sig == SIGINT) {
        flaga_stop = true;
        printf("<<GLOWNY>> SIGINT. Sprzatamy i konczymy.\n");
        posprzataj();
        exit(0);
    }
    if (sig == SIGTSTP) {
        kill(-pid_ratownicy, SIGTSTP);
        kill(-pid_klienci, SIGTSTP);
        raise(SIGSTOP);
    }
    if (sig == SIGCONT) {
        kill(-pid_ratownicy, SIGCONT);
        kill(-pid_klienci, SIGCONT);
    }
}
