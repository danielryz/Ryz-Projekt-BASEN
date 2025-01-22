// klient.c
#include "naglowki.h"
#include "narzedzia.c"

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
int      sem_id, kt_basen;
int      ban_tab[3];
pthread_t th_zomb;
volatile bool flaga_zomb;

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

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

    printf("<<Klienci - glowny>> Startuje generowanie osob.\n");

    flaga_zomb = true;
    memset(ban_tab, 0, sizeof(ban_tab));

    key_t klucz = ftok(".", 51);
    if (klucz == -1) {
        perror("ftok 51");
        exit(EXIT_FAILURE);
    }
    key_t klucz_cz = ftok(".", 52);
    if (klucz_cz == -1) {
        perror("ftok 52");
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

    while (*(int*)pam_czas < (CALA_DOBA_SYM - GODZINA_SYM)) {

        formatuj_czas(*(int*)pam_czas, bufor_godz);
        printf("[%s] (KLIENT GLOWNY) Za chwile pojawi sie nowy klient...\n", bufor_godz);

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

            if (setpgid(0, getppid()) == -1) {
                perror("setpgid - klient");
                exit(EXIT_FAILURE);
            }
            opusc_semafor(sem_id, 6);

            // Jezeli już po 21:00 (godziny symulacji)
            if (*(int*)pam_czas > (CALA_DOBA_SYM - GODZINA_SYM)) {
                podnies_semafor(sem_id, 6);
                odlacz_pamiec();
                exit(0);
            }

            DaneOsoby ds;
            ds.pid = getpid();
            ds.wiek = (rand() % 70) + 1;
            ds.wiek_opiekuna = (ds.wiek < 10) ? ((rand() % 53) + 18) : 0;
            ds.pampers = (ds.wiek <= 3);
            ds.czepek = (rand() % 2);
            ds.kasa = rand() % 50;
            ds.vip = ((rand() % 8) == 1); 
            ds.wpuszczony = false;
            ds.czas_wyjscia = 0;

            // VIP czy zwykly
            if (ds.vip) {
                opusc_semafor(sem_id, 5);
                formatuj_czas(*(int*)pam_czas, bufor_godz);
                printf("[%s] (KLIENT VIP) PID=%d, wiek=%d idzie do kasy.\n",
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
                // Zwykly klient
                formatuj_czas(*(int*)pam_czas, bufor_godz);
                printf("[%s] (KLIENT ZWYKLY) PID=%d, wiek=%d czeka w kolejce do kasy.\n",
                    bufor_godz, ds.pid, ds.wiek);

                opusc_semafor(sem_id, 1);
                memcpy(pam_dane, &ds, sizeof(DaneOsoby));
                podnies_semafor(sem_id, 2);

                opusc_semafor(sem_id, 3);
                memcpy(&ds, pam_dane, sizeof(DaneOsoby));
                podnies_semafor(sem_id, 1);
            }

            if (ds.wpuszczony) {
                formatuj_czas(*(int*)pam_czas, bufor_godz);
                printf("[%s] (KLIENT) PID=%d wchodzi do kompleksu.\n",
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
                        printf("[%s] (KLIENT) PID=%d: moj czas sie skonczyl, opuszczam budynek.\n",
                            bufor_godz, ds.pid);
                        break;
                    }

                    if (!kt_basen) {
                        int bas_choice = (rand() % 3) + 1;
                        while (ban_tab[bas_choice - 1]) {
                            bas_choice = (rand() % 3) + 1;
                        }
                        formatuj_czas(*(int*)pam_czas, bufor_godz);
                        printf("[%s] (KLIENT) PID=%d wybiera basen %d.\n",
                            bufor_godz, ds.pid, bas_choice);

                        zn.mtype = bas_choice + 2;
                        if (msgsnd(k_rat, &zn, sizeof(zn) - sizeof(long), 0) == -1) {
                            perror("msgsnd - wchodzenie basen");
                            exit(EXIT_FAILURE);
                        }
                        if (msgrcv(k_rat, &zn, sizeof(zn) - sizeof(long), ds.pid, 0) == -1) {
                            perror("msgrcv - wchodzenie basen");
                            exit(EXIT_FAILURE);
                        }
                        if (strcmp(zn.info, "OK") == 0) {
                            kt_basen = bas_choice;
                            formatuj_czas(*(int*)pam_czas, bufor_godz);
                            printf("[%s] (KLIENT) PID=%d: wszedlem do basenu #%d\n",
                                bufor_godz, ds.pid, kt_basen);
                        }
                        else {
                            printf(">> (KLIENT) PID=%d: niestety odmowa: %s\n",
                                ds.pid, zn.info);
                        }
                    }
                    moj_usleep(SEK_SYMULACJI * 120);
                }
            }
            else {
                // Nie wpuszczono
                formatuj_czas(*(int*)pam_czas, bufor_godz);
                printf("[%s] (KLIENT) PID=%d nie zostal wpuszczony do kompleksu.\n",
                    bufor_godz, ds.pid);
            }

            odlacz_pamiec();
            podnies_semafor(sem_id, 6);
            exit(0);
        }

        // Co pewien czas pojawia sie kolejny klient
        moj_usleep(SEK_SYMULACJI * ((rand() % 360) + 120));
    }

    // Po 21:00 w symulacji nie generujemy nowych
    flaga_zomb = false;
    st = pthread_join(th_zomb, NULL);
    sprawdz_err(st, "pthread_join - zombie klienci");

    while (wait(NULL) != -1) {}
    odlacz_pamiec();
    return 0;
}

static void sygnal_glowny(int s) {
    if (s == SIGINT) {
        printf("<<Klienci - glowny>> Otrzymano SIGINT, koncze.\n");
        while (wait(NULL) != -1) {}
        exit(0);
    }
}

// Obsluga sygnalow SIGUSR1 / SIGUSR2 (wyrzucenie z basenu / ponowne zezwolenie)
static void sygnal_usr(int sig, siginfo_t* info, void* ctx) {
    (void)ctx;
    int bas = info->si_value.sival_int;
    if (sig == SIGUSR1) {
        printf("(KLIENT PID=%d) Otrzymalem SIGUSR1 z basenu %d - musze wyjsc\n", getpid(), bas);
        ban_tab[bas - 1] = 1;
        if (kt_basen == bas) {
            kt_basen = 0;
        }
    }
    else if (sig == SIGUSR2) {
        printf("(KLIENT PID=%d) Otrzymalem SIGUSR2 z basenu %d - moge znow wchodzic\n", getpid(), bas);
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
    }
    return NULL;
}

// Wyjscie z aktualnie zajmowanego basenu (wysylamy PID w FIFO)
static void opuszczam_basen() {
    opusc_semafor(sem_id, 0);
    formatuj_czas(*(int*)pam_czas, bufor_godz);
    printf("[%s] (KLIENT) PID=%d: opuszczam basen %d.\n", bufor_godz, getpid(), kt_basen);

    char nazwa_pliku[32];
    snprintf(nazwa_pliku, sizeof(nazwa_pliku), "kana_bas_%d", kt_basen);

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

