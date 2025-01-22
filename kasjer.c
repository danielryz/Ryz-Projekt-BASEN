// kasjer.c
#include "naglowki.h"
#include "narzedzia.c"

#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static void sprzatanie_kasjera();
static void obsluga_sygnalu_kasjer(int);
static void* watek_vip(void*);
static void* watek_przerwa(void*);

char* adr_czas;
char    buf_godz[16];
DaneOsoby* wsp_dane;
pthread_t th_vipy, th_zamkniecie;
pthread_mutex_t mut_godz_wyj;
int sem_id;
volatile bool global_zamkniecie;
int ostatni_wpuszczony;
key_t klucz;

int main() {
    signal(SIGINT, obsluga_sygnalu_kasjer);
    signal(SIGTSTP, SIG_DFL);

    srand(time(NULL));

    printf("<<Kasjer>> Rozpoczeto prace kasjera.\n");

    klucz = ftok(".", 51);
    if (klucz == -1) {
        perror("ftok 51");
        exit(EXIT_FAILURE);
    }

    key_t klucz_czas = ftok(".", 52);
    if (klucz_czas == -1) {
        perror("ftok 52");
        exit(EXIT_FAILURE);
    }

    sem_id = semget(klucz, 8, 0600);
    if (sem_id == -1) {
        perror("semget - kasjer");
        exit(EXIT_FAILURE);
    }

    int id_pam = shmget(klucz, sizeof(DaneOsoby), 0600);
    if (id_pam == -1) {
        perror("shmget - kasjer");
        exit(EXIT_FAILURE);
    }
    wsp_dane = (DaneOsoby*)shmat(id_pam, NULL, 0);
    if (wsp_dane == (DaneOsoby*)(-1)) {
        perror("shmat - kasjer");
        exit(EXIT_FAILURE);
    }

    int id_cz = shmget(klucz_czas, sizeof(int), 0600);
    if (id_cz == -1) {
        perror("shmget - kasjer, czas");
        exit(EXIT_FAILURE);
    }
    adr_czas = (char*)shmat(id_cz, NULL, 0);
    if (adr_czas == (char*)(-1)) {
        perror("shmat - kasjer, czas");
        exit(EXIT_FAILURE);
    }

    int sts = pthread_mutex_init(&mut_godz_wyj, NULL);
    sprawdz_err(sts, "pthread_mutex_init - kasjer");

    sts = pthread_create(&th_vipy, NULL, watek_vip, NULL);
    sprawdz_err(sts, "pthread_create - kasjer VIP");

    sts = pthread_create(&th_zamkniecie, NULL, watek_przerwa, NULL);
    sprawdz_err(sts, "pthread_create - kasjer przerwy");

    global_zamkniecie = false;

    DaneOsoby temp;
    while (1) {
        opusc_semafor(sem_id, 2);
        memcpy(&temp, wsp_dane, sizeof(DaneOsoby));

        // Sprawdzamy godzine i status przerwy
        if ((*(int*)adr_czas) > CALA_DOBA_SYM - GODZINA_SYM || global_zamkniecie) {
            // Po 21:00 lub przerwa
            temp.wpuszczony = false;
            formatuj_czas(*(int*)adr_czas, buf_godz);
            printf("[%s] (KASJER) Klient PID=%d przybyl za pozno albo trafil na zamkniecie.\n",
                buf_godz, temp.pid);
        }
        else {
            // Normalna sprzedaz biletow
            if ((temp.wiek > 18 || temp.wiek < 10) && temp.kasa >= 40) {
                temp.kasa -= 30;
                temp.wpuszczony = true;
                temp.czas_wyjscia = (*(int*)adr_czas) + GODZINA_SYM;

                blokada_mutex(&mut_godz_wyj);
                ostatni_wpuszczony = temp.czas_wyjscia;
                odblokada_mutex(&mut_godz_wyj);

                formatuj_czas(*(int*)adr_czas, buf_godz);
                printf("[%s] (KASJER) Sprzedano bilet normalny (PID=%d)\n", buf_godz, temp.pid);
            }
            else if (temp.kasa >= 15) {
                temp.kasa -= 15;
                temp.wpuszczony = true;
                temp.czas_wyjscia = (*(int*)adr_czas) + GODZINA_SYM;

                blokada_mutex(&mut_godz_wyj);
                ostatni_wpuszczony = temp.czas_wyjscia;
                odblokada_mutex(&mut_godz_wyj);

                formatuj_czas(*(int*)adr_czas, buf_godz);
                printf("[%s] (KASJER) Sprzedano bilet ulgowy (PID=%d)\n", buf_godz, temp.pid);
            }
            else {
                temp.wpuszczony = false;
                formatuj_czas(*(int*)adr_czas, buf_godz);
                printf("[%s] (KASJER) Klient PID=%d ma za malo pieniedzy, niestety.\n",
                    buf_godz, temp.pid);
            }
        }

        memcpy(wsp_dane, &temp, sizeof(DaneOsoby));

        formatuj_czas((*(int*)adr_czas), buf_godz);
        printf("[%s] (KASJER) Zakonczono obsluge PID=%d.\n", buf_godz, temp.pid);

        podnies_semafor(sem_id, 3);
    }

    return 0;
}

static void sprzatanie_kasjera() {
    int st;

    if ((st = pthread_cancel(th_zamkniecie)) != 0 && st != ESRCH) {
        fprintf(stderr, "pthread_cancel - przerwa, kod: %d\n", st);
        exit(EXIT_FAILURE);
    }
    st = pthread_join(th_zamkniecie, NULL);
    sprawdz_err(st, "pthread_join - przerwa kasjer");

    if (shmdt(wsp_dane) == -1 || shmdt(adr_czas) == -1) {
        perror("shmdt - kasjer");
        exit(EXIT_FAILURE);
    }

    st = pthread_cancel(th_vipy);
    sprawdz_err(st, "pthread_cancel - vip kasjer");
    st = pthread_join(th_vipy, NULL);
    sprawdz_err(st, "pthread_join - vip kasjer");

    st = pthread_mutex_destroy(&mut_godz_wyj);
    sprawdz_err(st, "pthread_mutex_destroy - kasjer");
}

static void obsluga_sygnalu_kasjer(int sig) {
    if (sig == SIGINT) {
        printf("<<Kasjer>> Otrzymano SIGINT, koncze prace kasjera.\n");
        sprzatanie_kasjera();
        exit(0);
    }
}

// Watek obslugujacy klientow VIP
static void* watek_vip(void* arg) {
    (void)arg;

    printf("<<Kasjer>> Watek VIP dziala.\n");

    int msqid_vip = msgget(klucz, 0600);
    if (msqid_vip == -1) {
        perror("msgget kasjer VIP");
        exit(EXIT_FAILURE);
    }

    KomunikatKasowy kom;
    while (1) {
        kom.mtype = MTYP_OSOBA_DO_KAS;
        if (msgrcv(msqid_vip, &kom, sizeof(kom) - sizeof(long), kom.mtype, 0) == -1) {
            if (errno != EINTR) {
                perror("msgrcv kasjer VIP");
                exit(EXIT_FAILURE);
            }
        }

        // Odpowiedz
        kom.mtype = kom.pid_osoby;
        if (rand() % 30 == 16) {
            strcpy(kom.opis, "karnet niewazny");
        }
        else if ((*(int*)adr_czas) > CALA_DOBA_SYM - GODZINA_SYM || global_zamkniecie) {
            strcpy(kom.opis, "kasa zamknieta");
        }
        else {
            strcpy(kom.opis, "mozesz wejsc");
            kom.wyjscie_czas = (*(int*)adr_czas) + GODZINA_SYM;

            blokada_mutex(&mut_godz_wyj);
            ostatni_wpuszczony = kom.wyjscie_czas;
            odblokada_mutex(&mut_godz_wyj);
        }

        if (msgsnd(msqid_vip, &kom, sizeof(kom) - sizeof(long), 0) == -1) {
            perror("msgsnd kasjer VIP");
            exit(EXIT_FAILURE);
        }

        formatuj_czas((*(int*)adr_czas), buf_godz);
        printf("[%s] (KASJER) Obsluzono VIP PID=%d.\n", buf_godz, kom.pid_osoby);
    }

    return NULL;
}

// Watek przerwy, zamyka osrodek po 4h od startu
static void* watek_przerwa(void* arg) {
    (void)arg;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    int czas_lokalny;
    // czeka do 14:00
    while ((czas_lokalny = (*(int*)adr_czas)) < 4 * GODZINA_SYM) {
        pthread_testcancel();
    }

    opusc_semafor(sem_id, 4);
    global_zamkniecie = true;

    formatuj_czas((*(int*)adr_czas), buf_godz);
    printf("[%s] (KASJER) Nastapila przerwa! Czekamy az wszyscy wyjda...\n", buf_godz);

    // czekamy, az wyjdzie ostatni
    while ((czas_lokalny = (*(int*)adr_czas)) < ostatni_wpuszczony) {
        pthread_testcancel();
    }

    formatuj_czas((*(int*)adr_czas), buf_godz);
    printf("[%s] (KASJER) Osrodek pusty, przerwa trwa 60 minut.\n", buf_godz);

    // 1h przerwy
    while ((czas_lokalny = (*(int*)adr_czas)) < (ostatni_wpuszczony + GODZINA_SYM)) {
        pthread_testcancel();
    }

    formatuj_czas((*(int*)adr_czas), buf_godz);
    printf("[%s] (KASJER) Koniec przerwy, otwieramy ponownie!\n", buf_godz);

    global_zamkniecie = false;
    podnies_semafor(sem_id, 4);

    return NULL;
}

