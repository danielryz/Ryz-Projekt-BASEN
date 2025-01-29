//klient.c

#include "struktury.h"
#include "funkcje.c"

#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void sygnal_glowny(int);
static void sygnal_usr(int, siginfo_t*, void*);
static void* wat_zombie(void*);
static void opuszczam_basen();
static void odlacz_pamiec();

char     bufor_godz[16];
char* pam_czas;
DaneOsoby* pam_dane;
int  sem_id, kt_basen;
int  ban_tab[3];
pthread_t th_zomb;
volatile bool flaga_zomb;

int main(int argc, char* argv[]) {

    if (argc < 5) {
        fprintf(stderr, "Błąd: Brak argumentów! Oczekiwano: POJ_OLIMPIJKA POJ_REKREACJA POJ_BRODZIK SEK_SYMULACJI\n");
        exit(EXIT_FAILURE);
    }

    // Konwersja argumentów na liczby
    POJ_OLIMPIJKA = atoi(argv[1]);
    POJ_REKREACJA = atoi(argv[2]);
    POJ_BRODZIK = atoi(argv[3]);
    SEK_SYMULACJI = atoi(argv[4]);

    printf("Proces %s: Otrzymano POJ_OLIMPIJKA=%d, POJ_REKREACJA=%d, POJ_BRODZIK=%d, SEK_SYMULACJI=%d\n",
        argv[0], POJ_OLIMPIJKA, POJ_REKREACJA, POJ_BRODZIK, SEK_SYMULACJI);

    if (setpgid(0, 0) == -1) {
        perror("setpgid - klienci glowny");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sygnal_glowny);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sygnal_usr;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction - SIGUSR1 klient");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("sigaction - SIGUSR2 klient");
        exit(EXIT_FAILURE);
    }

    printf(GREEN " <<Klienci - glowny>> Startuje generowanie osob. " RESET "\n");

    flaga_zomb = true;
    memset(ban_tab, 0, sizeof(ban_tab));

    key_t klucz = ftok(".", 100);
    if (klucz == -1) {
        perror("ftok 100");
        exit(EXIT_FAILURE);
    }
    key_t klucz_cz = ftok(".", 200);
    if (klucz_cz == -1) {
        perror("ftok 200");
        exit(EXIT_FAILURE);
    }

    sem_id = semget(klucz, 8, 0600);
    if (sem_id == -1) {
        perror("semget - klienci");
        exit(EXIT_FAILURE);
    }

    int id_shm = shmget(klucz, sizeof(DaneOsoby), 0600);
    if (id_shm == -1) {
        perror("shmget - klienci");
        exit(EXIT_FAILURE);
    }
    pam_dane = (DaneOsoby*)shmat(id_shm, NULL, 0);
    if (pam_dane == (DaneOsoby*)(-1)) {
        perror("shmat - klienci");
        exit(EXIT_FAILURE);
    }

    int id_shm_time = shmget(klucz_cz, sizeof(int), 0600);
    if (id_shm_time == -1) {
        perror("shmget - klienci time");
        exit(EXIT_FAILURE);
    }
    pam_czas = (char*)shmat(id_shm_time, NULL, 0);
    if (pam_czas == (char*)(-1)) {
        perror("shmat - klienci time");
        exit(EXIT_FAILURE);
    }

    int k_vip = msgget(klucz, 0600);
    if (k_vip == -1) {
        perror("msgget - vip");
        exit(EXIT_FAILURE);
    }
    int k_rat = msgget(klucz_cz, 0600);
    if (k_rat == -1) {
        perror("msgget - ratownicy");
        exit(EXIT_FAILURE);
    }

    int st = pthread_create(&th_zomb, NULL, wat_zombie, NULL);
    sprawdz_err(st, "pthread_create - zombie klienci");

    srand(time(NULL) ^ getpid());

    // Petla generowania nowych klientow
    while (*(int*)pam_czas < (CALY_CZAS_SYM - GODZINA_SYM)) {
        formatuj_czas(*(int*)pam_czas, bufor_godz);
        printf("[%s] " BG_MAGENTA BLACK " (KLIENT GLOWNY): " RESET MAGENTA
            " Za chwile pojawi sie nowy klient... " RESET "\n", bufor_godz);

        // co losowy czas pojawia sie klient
        moj_usleep(SEK_SYMULACJI * ((rand() % 300) + 40));
        

        opusc_semafor(sem_id, 4);
        pid_t nowy = fork();
        if (nowy < 0) {
            perror("fork - klienci");
            kill(-getpid(), SIGINT);
            exit(EXIT_FAILURE);
        }
        else if (nowy == 0) {
            srand(getpid());
            signal(SIGTSTP, SIG_DFL);
            podnies_semafor(sem_id, 4);

            opusc_semafor(sem_id, 6);

            if (*(int*)pam_czas > (CALY_CZAS_SYM - GODZINA_SYM)) {
                podnies_semafor(sem_id, 6);
                odlacz_pamiec();
                exit(0);
            }

            DaneOsoby ds;
            ds.pid = getpid();
            ds.wiek = (rand() % 70) + 1;
            ds.wiek_opiekuna = (ds.wiek < 10) ? ((rand() % 70) + 18) : 0;
            ds.pampers = (ds.wiek <= 3);
            ds.czepek = (rand() % 2);
            ds.kasa = rand() % 100;
            ds.vip = ((rand() % 6) == 1);
            ds.wpuszczony = false;
            ds.czas_wyjscia = 0;

            // VIP czy zwykly
            if (ds.vip) {
                opusc_semafor(sem_id, 5);
                formatuj_czas(*(int*)pam_czas, bufor_godz);
                printf("[%s] " BG_CYAN BLACK " (KLIENT VIP): " RESET CYAN
                    " PID=%d, wiek=%d idzie do kasy. " RESET "\n",
                    bufor_godz, ds.pid, ds.wiek);

                KomunikatKasowy vipmsg;
                vipmsg.mtype = MTYP_OSOBA_DO_KAS;
                vipmsg.pid_osoby = ds.pid;
                strcpy(vipmsg.opis, "okazuj_karnet_VIP");

                if (msgsnd(k_vip, &vipmsg, sizeof(vipmsg) - sizeof(long), 0) == -1) {
                    perror("msgsnd - vip");
                    exit(EXIT_FAILURE);
                }
                if (msgrcv(k_vip, &vipmsg, sizeof(vipmsg) - sizeof(long), ds.pid, 0) == -1) {
                    perror("msgrcv - vip");
                    exit(EXIT_FAILURE);
                }
                if (strcmp(vipmsg.opis, "mozesz wejsc") == 0) {
                    ds.wpuszczony = true;
                    ds.czas_wyjscia = vipmsg.wyjscie_czas;
                }
                opusc_semafor(sem_id, 5);
            }
            else {
                // zwykly
                formatuj_czas(*(int*)pam_czas, bufor_godz);
                printf("[%s] " BG_GREEN BLACK " (KLIENT ZWYKLY): " RESET GREEN
                    " PID=%d, wiek=%d czeka w kolejce do kasy. " RESET "\n",
                    bufor_godz, ds.pid, ds.wiek);

                // zajmujemy semafor 1
                opusc_semafor(sem_id, 1);

                memcpy(pam_dane, &ds, sizeof(DaneOsoby));
                podnies_semafor(sem_id, 2);

                opusc_semafor(sem_id, 3);
                memcpy(&ds, pam_dane, sizeof(DaneOsoby));

                podnies_semafor(sem_id, 1);
            }

            if (ds.wpuszczony) {
                formatuj_czas(*(int*)pam_czas, bufor_godz);
                printf("[%s] " BG_GREEN BLACK " (KLIENT): " RESET GREEN
                    " PID=%d wchodzi do kompleksu. " RESET "\n",
                    bufor_godz, ds.pid);

                kt_basen = 0;
                KomunikatNadzoru zn;
                zn.pid_osoby = ds.pid;
                zn.wiek_osoby = ds.wiek;
                zn.wiek_opiekuna = ds.wiek_opiekuna;

                while (1) {
                    if (*(int*)pam_czas > ds.czas_wyjscia) {
                        if (kt_basen) {
                            opuszczam_basen();
                        }
                        formatuj_czas(*(int*)pam_czas, bufor_godz);
                        printf("[%s] " BG_GREEN BLACK " (KLIENT): " RESET GREEN
                            " PID=%d: moj czas sie skonczyl, opuszczam budynek. " RESET "\n",
                            bufor_godz, ds.pid);
                        break;
                    }

                    // jesli nie w basenie, probujemy wchodzic
                    if (!kt_basen) {
                        int bas_choice = (rand() % 3) + 1;
                        // sprawdzamy, czy nie zbanowany
                        while (ban_tab[bas_choice - 1]) {
                            bas_choice = (rand() % 3) + 1;
                        }
                        formatuj_czas(*(int*)pam_czas, bufor_godz);
                        printf("[%s] " BG_GREEN BLACK " (KLIENT): " RESET GREEN
                            " PID=%d wybiera basen %d. " RESET "\n",
                            bufor_godz, ds.pid, bas_choice);

                        zn.mtype = bas_choice + 2;
                        if (msgsnd(k_rat, &zn, sizeof(zn) - sizeof(long), 0) == -1) {
                            perror("msgsnd - wchodzenie do basen");
                            exit(EXIT_FAILURE);
                        }
                        if (msgrcv(k_rat, &zn, sizeof(zn) - sizeof(long), ds.pid, 0) == -1) {
                            perror("msgrcv - wchodzenie do basenu");
                            exit(EXIT_FAILURE);
                        }
                        if (strcmp(zn.info, "OK") == 0) {
                            kt_basen = bas_choice;
                            formatuj_czas(*(int*)pam_czas, bufor_godz);
                            printf("[%s] " BG_GREEN BLACK " (KLIENT): " RESET GREEN
                                " PID=%d: wszedlem do basenu #%d " RESET "\n",
                                bufor_godz, ds.pid, kt_basen);
                        }
                        else {
                            printf(">> " BG_GREEN BLACK " (KLIENT): " RESET GREEN
                                " PID=%d: niestety odmowa: %s " RESET "\n",
                                ds.pid, zn.info);
                        }
                    }

                    // "pływamy"
                    moj_usleep(SEK_SYMULACJI * 120);
                }
            }
            else {
                formatuj_czas(*(int*)pam_czas, bufor_godz);
                printf("[%s] " BG_GREEN BLACK " (KLIENT): " RESET GREEN
                    " PID=%d nie zostal wpuszczony do kompleksu. " RESET "\n",
                    bufor_godz, ds.pid);
            }

            odlacz_pamiec();
            podnies_semafor(sem_id, 6);
            exit(0);
        }
    }

    // Po 21:00 - koniec generowania
    flaga_zomb = false;
    st = pthread_join(th_zomb, NULL);
    sprawdz_err(st, "pthread_join - zombie klienci");

    while (wait(NULL) != -1) {}
    odlacz_pamiec();
    return 0;
}

static void sygnal_glowny(int s) {
    if (s == SIGINT) {
        printf(GREEN "<<Klienci - glowny>> Otrzymano SIGINT, koncze. " RESET "\n");
        while (wait(NULL) != -1) {}
        exit(0);
    }
}

// Obsluga SIGUSR1 / SIGUSR2
static void sygnal_usr(int sig, siginfo_t* info, void* ctx) {
    (void)ctx;
    int bas = info->si_value.sival_int;
    if (sig == SIGUSR1) {
        printf(BG_GREEN BLACK "(KLIENT PID=%d): " RESET GREEN
            " Otrzymalem SIGUSR1 z basenu %d - musze wyjsc " RESET "\n",
            getpid(), bas);
        ban_tab[bas - 1] = 1;
        if (kt_basen == bas) {
            kt_basen = 0;
        }
    }
    else if (sig == SIGUSR2) {
        printf(BG_GREEN BLACK "(KLIENT PID=%d): " RESET GREEN
            " Otrzymalem SIGUSR2 z basenu %d - moge znow wchodzic " RESET "\n",
            getpid(), bas);
        ban_tab[bas - 1] = 0;
    }
}

static void* wat_zombie(void* arg) {
    (void)arg;
    while (flaga_zomb) {
        if (wait(NULL) == -1 && errno != ECHILD) {
            perror("wait - zombie klienci");
            exit(EXIT_FAILURE);
        }
        usleep(10000);
    }
    return NULL;
}

static void opuszczam_basen() {
    opusc_semafor(sem_id, 0);
    formatuj_czas(*(int*)pam_czas, bufor_godz);
    printf("[%s] " BG_GREEN BLACK "(KLIENT): " RESET GREEN
        " PID=%d: opuszczam basen %d. " RESET "\n",
        bufor_godz, getpid(), kt_basen);

    char nazwa_pliku[32];
    snprintf(nazwa_pliku, sizeof(nazwa_pliku), "kanal_bas_%d", kt_basen);

    int fd = open(nazwa_pliku, O_WRONLY);
    if (fd == -1) {
        perror("open FIFO - klient wychodzi");
        exit(EXIT_FAILURE);
    }
    pid_t p = getpid();
    if (write(fd, &p, sizeof(p)) == -1) {
        perror("write FIFO - klient wychodzi");
        exit(EXIT_FAILURE);
    }
    close(fd);

    kt_basen = 0;
    podnies_semafor(sem_id, 0);
}

static void odlacz_pamiec() {
    if (shmdt(pam_dane) == -1 || shmdt(pam_czas) == -1) {
        perror("shmdt - klient");
        exit(EXIT_FAILURE);
    }
}
