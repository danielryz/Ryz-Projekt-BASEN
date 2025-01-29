//basen.c

#include "struktury.h"
#include "funkcje.c"

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


// -----------------------------------------

static void* steruj_czasem(void*);
static void posprzataj();
static void sygnaly_glowne(int);
static void zapisz_stan_basenow_do_pliku(const char*, const char*, int, int, int);

static int shm_id_basen;
static BasenyData* gl_basen;  // wskaznik globalny do stanu basenow

pid_t pid_ratownicy, pid_kasjer, pid_klienci;
pthread_t th_time;
char* wsp_czas;
int shm_id_1, shm_id_2, sem_id, msq_1, msq_2;
volatile bool flaga_stop = false;



int main(int argc, char* argv[]) {

    // Pobieranie argumentow od uzytkownika
    // Uzycie: ./baseny POJ_OLIMPIJKA POJ_REKREACJA POJ_BRODZIK SEK_SYMULACJI
    if (argc == 5) {
        POJ_OLIMPIJKA = atoi(argv[1]);
        POJ_REKREACJA = atoi(argv[2]);
        POJ_BRODZIK = atoi(argv[3]);
        SEK_SYMULACJI = atoi(argv[4]);

        // Proste walidacje
        if (POJ_OLIMPIJKA <= 0 || POJ_OLIMPIJKA > MAX_POJ_OLIMPIJKA ||
            POJ_REKREACJA <= 0 || POJ_REKREACJA > MAX_POJ_REKREACJA ||
            POJ_BRODZIK <= 0 || POJ_BRODZIK > MAX_POJ_BRODZIK ||
            SEK_SYMULACJI <= 0)
        {
            fprintf(stderr, "Bledne argumenty! Wartosc > 0 i <= MAX.\n");
            exit(EXIT_FAILURE);
        }
        printf("Ustawione pojemnosci: O=%d, R=%d, B=%d, SEK_SYM=%d\n",
            POJ_OLIMPIJKA, POJ_REKREACJA, POJ_BRODZIK, SEK_SYMULACJI);
    }
    else {
        printf("Uzycie: %s POJ_OLIMPIJKA POJ_REKREACJA POJ_BRODZIK SEK_SYMULACJI\n"
            "Brak argumentow - dzialam na domyslnych (8,16,10,5000).\n",
            argv[0]);
    }

    // --------------------------------------------
    signal(SIGINT, sygnaly_glowne);   // Ctrl + C
    signal(SIGTSTP, sygnaly_glowne);  // Ctrl + Z
    signal(SIGCONT, sygnaly_glowne);  // Kontynuacja

    // Klucze IPC
    key_t klucz = ftok(".", 100);
    if (klucz == -1) {
        perror("ftok 100 - glowny");
        exit(EXIT_FAILURE);
    }
    key_t klucz2 = ftok(".", 200);
    if (klucz2 == -1) {
        perror("ftok 200 - glowny");
        exit(EXIT_FAILURE);
    }

    sem_id = semget(klucz, 8, 0600 | IPC_CREAT);
    if (sem_id == -1) {
        perror("semget - glowny");
        exit(EXIT_FAILURE);
    }
    // Inicjalizacja semaforow
    if (semctl(sem_id, 0, SETVAL, 1) == -1) { perror("semctl s0"); exit(EXIT_FAILURE); } // Ochrona pamieci wspoldzielonej z danymi klientow
    if (semctl(sem_id, 1, SETVAL, 1) == -1) { perror("semctl s1"); exit(EXIT_FAILURE); } // Ochrona pamieci wpoldzielonej unikanie kolizji klient-kasjer
    if (semctl(sem_id, 2, SETVAL, 0) == -1) { perror("semctl s2"); exit(EXIT_FAILURE); } // Synchronizacja komunikacji klient-kasjer obsluga
    if (semctl(sem_id, 3, SETVAL, 0) == -1) { perror("semctl s3"); exit(EXIT_FAILURE); } // Synchronizacja komunikacji klient-kasjer zakonczenie obslugi
    if (semctl(sem_id, 4, SETVAL, 1) == -1) { perror("semctl s4"); exit(EXIT_FAILURE); } // Blokowanie dostepu klientow podczas przerwy technicznej
    if (semctl(sem_id, 5, SETVAL, 1) == -1) { perror("semctl s5"); exit(EXIT_FAILURE); } // Obsluga Klientow VIP

    // Maksymalna liczba klientów
    if (semctl(sem_id, 6, SETVAL, (POJ_OLIMPIJKA + POJ_REKREACJA + POJ_BRODZIK) * 3) == -1) {
        perror("semctl s6");
        exit(EXIT_FAILURE);
    } // Ograniczenie maksymalnej liczby klientow w basenie
    if (semctl(sem_id, 7, SETVAL, 1) == -1) { perror("semctl s7"); exit(EXIT_FAILURE); } // Ochrona wizualizacji satnu basenow

    // Kolejki komunikatów
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

    // Pamięć wspólna (dla DaneOsoby)
    shm_id_1 = shmget(klucz, sizeof(DaneOsoby), 0600 | IPC_CREAT);
    if (shm_id_1 == -1) {
        perror("shmget1 - glowny");
        exit(EXIT_FAILURE);
    }

    // Pamięć wspólna dla czasu
    shm_id_2 = shmget(klucz2, sizeof(int), 0600 | IPC_CREAT);
    if (shm_id_2 == -1) {
        perror("shmget2 - glowny");
        exit(EXIT_FAILURE);
    }

    // Dolaczanie segmentu czasu
    wsp_czas = (char*)shmat(shm_id_2, NULL, 0);
    if (wsp_czas == (char*)(-1)) {
        perror("shmat - glowny");
        exit(EXIT_FAILURE);
    }
    *((int*)wsp_czas) = 0; // start 10:00 = 0

    // Segment pamięci dla BasenyData
    shm_id_basen = shmget(klucz + 10, sizeof(BasenyData), 0600 | IPC_CREAT);
    if (shm_id_basen == -1) {
        perror("shmget3 - basenyData");
        exit(EXIT_FAILURE);
    }
    // Dołączamy
    gl_basen = (BasenyData*)shmat(shm_id_basen, NULL, 0);
    if (gl_basen == (void*)-1) {
        perror("shmat - basenyData");
        exit(EXIT_FAILURE);
    }
    // Zerujemy
    memset(gl_basen, 0, sizeof(BasenyData));

    // Tworzymy FIFOs
    if (mkfifo("kanal_bas_1", 0600) == -1 && errno != EEXIST) {
        perror("mkfifo1");
        exit(EXIT_FAILURE);
    }
    if (mkfifo("kanal_bas_2", 0600) == -1 && errno != EEXIST) {
        perror("mkfifo2");
        exit(EXIT_FAILURE);
    }
    if (mkfifo("kanal_bas_3", 0600) == -1 && errno != EEXIST) {
        perror("mkfifo3");
        exit(EXIT_FAILURE);
    }

    printf(BG_RED WHITE " <<GLOWNY>> " RESET RED " Tworze procesy ratownicy, kasjer, klienci... " RESET "\n");

    char arg_olimp[16], arg_rek[16], arg_brodz[16], arg_sek[16];

    sprintf(arg_olimp, "%d", POJ_OLIMPIJKA);
    sprintf(arg_rek, "%d", POJ_REKREACJA);
    sprintf(arg_brodz, "%d", POJ_BRODZIK);
    sprintf(arg_sek, "%d", SEK_SYMULACJI);


    // fork ratownicy
    pid_ratownicy = fork();
    if (pid_ratownicy < 0) {
        perror("fork - ratownicy");
        exit(EXIT_FAILURE);
    }
    else if (pid_ratownicy == 0) {
        execl("./ratownik", "ratownik", arg_olimp, arg_rek, arg_brodz, arg_sek, NULL);
        perror("execl - ratownik");
        exit(EXIT_FAILURE);
    }
    else {
        // fork kasjer
        pid_kasjer = fork();
        if (pid_kasjer < 0) {
            perror("fork - kasjer");
            kill(pid_ratownicy, SIGINT);
            exit(EXIT_FAILURE);
        }
        else if (pid_kasjer == 0) {
            execl("./kasjer", "kasjer", arg_olimp, arg_rek, arg_brodz, arg_sek, NULL);
            perror("execl - kasjer");
            exit(EXIT_FAILURE);
        }
        else {
            // fork klienci
            pid_klienci = fork();
            if (pid_klienci < 0) {
                perror("fork - klienci");
                kill(pid_ratownicy, SIGINT);
                kill(pid_kasjer, SIGINT);
                exit(EXIT_FAILURE);
            }
            else if (pid_klienci == 0) {
                execl("./klient", "klient", arg_olimp, arg_rek, arg_brodz, arg_sek, NULL);
                perror("execl - klient");
                exit(EXIT_FAILURE);
            }
        }
    }

    printf(RED " Klienci PID=%d, kasjer PID=%d, ratownicy PID=%d " RESET "\n\n",
        pid_klienci, pid_kasjer, pid_ratownicy);

    // Wątek sterowania czasem
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
    int poprzedni_czas = 0;
    char buf_start[16];
    char buf_end[16];

    // petla do ~12.5h (45000 sek. symulacji) 
    while (*pczas < (CALY_CZAS_SYM + 1800) && !flaga_stop) {
        if (usleep(SEK_SYMULACJI) != 0 && errno != EINTR) {
            perror("usleep - steruj_czasem");
            exit(EXIT_FAILURE);
        }
        (*pczas)++;

        // Co 30 min zapis do pliku
        if ((*pczas - poprzedni_czas) >= 1800) {
            formatuj_czas(poprzedni_czas, buf_start);
            formatuj_czas(*pczas, buf_end);

            // Odczyt 3 liczb:
            podnies_semafor(sem_id, 0);
            int oli = gl_basen->tab_olimp[0];
            opusc_semafor(sem_id, 0);

            podnies_semafor(sem_id, 0);
            int rek = gl_basen->tab_rek[0][0];
            opusc_semafor(sem_id, 0);

            podnies_semafor(sem_id, 0);
            int brod = gl_basen->tab_brodz[0];
            opusc_semafor(sem_id, 0);

            // Zapis
            zapisz_stan_basenow_do_pliku(buf_start, buf_end, oli, rek, brod);
            poprzedni_czas = *pczas;
        }
    }

    // Zakonczenie
    if (kill(-pid_ratownicy, SIGINT) != 0 && errno != ESRCH) {
        perror("kill ratownicy");
    }
    if (kill(-pid_klienci, SIGINT) != 0 && errno != ESRCH) {
        perror("kill klienci");
    }
    if (kill(pid_kasjer, SIGINT) != 0 && errno != ESRCH) {
        perror("kill kasjer");
    }
    

    return NULL;
}

static void posprzataj() {
    int status;
    pid_t ret1, ret2, ret3;

   
    // czekamy na klientów
    ret1 = waitpid(pid_klienci, &status, 0);
    if (ret1 == -1) {
        perror("waitpid klienci");
    }
    else if (WIFEXITED(status)) {
        printf(GREEN " Klienci (PID=%d) zakonczyli sie z kodem %d " RESET "\n", ret1, WEXITSTATUS(status));
    }
    else {
        printf(RED " Klienci (PID=%d) zakonczyli sie w nietypowy sposob, status=%d " RESET "\n", ret1, status);
    }

    // czekamy na ratownikow
    ret3 = waitpid(pid_ratownicy, &status, 0);
    if (ret3 == -1) {
        perror("waitpid ratownicy");
    }
    else if (WIFEXITED(status)) {
        printf(GREEN " Ratownicy (PID=%d) zakonczyl sie z kodem %d " RESET "\n", ret3, WEXITSTATUS(status));
    }
    else {
        printf(RED " Ratownicy (PID=%d) zakonczyl sie w nietypowy sposob, status=%d " RESET "\n", ret3, status);
    }

    // czekamy na kasjera
    ret2 = waitpid(pid_kasjer, &status, 0);
    if (ret2 == -1) {
        perror("waitpid kasjer");
    }
    else if (WIFEXITED(status)) {
        printf(GREEN " Kasjer (PID=%d) zakonczyl sie z kodem %d " RESET "\n", ret2, WEXITSTATUS(status));
    }
    else {
        printf(RED " Kasjer (PID=%d) zakonczyl sie w nietypowy sposob, status=%d " RESET "\n", ret2, status);
    }

    

    pthread_join(th_time, NULL);

    if (unlink("kanal_bas_1") == -1 || unlink("kanal_bas_2") == -1 || unlink("kanal_bas_3") == -1) {
        perror("unlink fifo");
    }

    if (shmctl(shm_id_1, IPC_RMID, 0) == -1) {
        perror("shmctl - 1");
    }
    if (shmctl(shm_id_2, IPC_RMID, 0) == -1) {
        perror("shmctl - 2");
    }
    if (shmdt(gl_basen) == -1) {
        perror("shmdt gl_basen");
    }
    if (shmctl(shm_id_basen, IPC_RMID, 0) == -1) {
        perror("shmctl - basenyData");
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
        printf(BG_RED WHITE " <<GLOWNY>> SIGINT. " RESET RED " Sprzatamy i konczymy. " RESET "\n");
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

static void zapisz_stan_basenow_do_pliku(const char* start, const char* end,
    int oli, int rek, int brod)
{
    FILE* f = fopen("wynik_symulacji.txt", "a");
    if (!f) {
        perror("fopen - wynik_symulacji.txt");
        return;
    }
    fprintf(f, "Stan basenu %s - %s:\n\n", start, end);
    fprintf(f, "  Basen Olimpijski:   %d klientów\n", oli);
    fprintf(f, "  Basen Rekreacyjny:  %d klientów\n", rek);
    fprintf(f, "  Basen Brodzik:      %d klientów\n\n", brod);
    fclose(f);
}
