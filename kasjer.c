//kasjer.c

#include "struktury.h"
#include "funkcje.c"

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
char  buf_godz[16];
DaneOsoby* wsp_dane;
pthread_t th_vipy, th_zamkniecie;
pthread_mutex_t mut_godz_wyj;
int sem_id;
volatile bool global_zamkniecie;
int ostatni_wpuszczony;
key_t klucz;

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


    signal(SIGINT, obsluga_sygnalu_kasjer);
    signal(SIGTSTP, SIG_DFL);

    srand(time(NULL)); // seed dla ewentualnych losowości

    printf(YELLOW " <<Kasjer>> Rozpoczeto prace kasjera. " RESET "\n");

    klucz = ftok(".", 100);
    if (klucz == -1) {
        perror("ftok 100");
        exit(EXIT_FAILURE);
    }

    key_t klucz_czas = ftok(".", 200);
    if (klucz_czas == -1) {
        perror("ftok 200");
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
        // czekamy na semafor 2 – klient wysyła DaneOsoby
        opusc_semafor(sem_id, 2);

        memcpy(&temp, wsp_dane, sizeof(DaneOsoby));

        // Sprawdzamy godzine i status przerwy
        if ((*(int*)adr_czas) > (CALY_CZAS_SYM - GODZINA_SYM) || global_zamkniecie) {
            // Po 21:00 lub przerwa
            temp.wpuszczony = false;
            formatuj_czas(*(int*)adr_czas, buf_godz);
            printf("[%s] " BG_YELLOW BLACK " (KASJER): " RESET YELLOW
                " Klient PID=%d - zamkniete/przerwa. " RESET "\n",
                buf_godz, temp.pid);
        }
        else {
            // Normalna sprzedaz biletow
            if ((temp.wiek > 18 || temp.wiek < 10) && temp.kasa >= 30) {
                temp.kasa -= 30;
                temp.wpuszczony = true;
                temp.czas_wyjscia = (*(int*)adr_czas) + GODZINA_SYM;

                blokada_mutex(&mut_godz_wyj);
                ostatni_wpuszczony = temp.czas_wyjscia;
                odblokowanie_mutex(&mut_godz_wyj);

                formatuj_czas(*(int*)adr_czas, buf_godz);
                printf("[%s] " BG_YELLOW BLACK " (KASJER): " RESET YELLOW
                    " Sprzedano bilet normalny (PID=%d). " RESET "\n",
                    buf_godz, temp.pid);
            }
            else if (temp.kasa >= 20) {
                temp.kasa -= 20;
                temp.wpuszczony = true;
                temp.czas_wyjscia = (*(int*)adr_czas) + GODZINA_SYM;

                blokada_mutex(&mut_godz_wyj);
                ostatni_wpuszczony = temp.czas_wyjscia;
                odblokowanie_mutex(&mut_godz_wyj);

                formatuj_czas(*(int*)adr_czas, buf_godz);
                printf("[%s] " BG_YELLOW BLACK " (KASJER): " RESET YELLOW
                    " Sprzedano bilet ulgowy (PID=%d). " RESET "\n",
                    buf_godz, temp.pid);
            }
            else {
                temp.wpuszczony = false;
                formatuj_czas(*(int*)adr_czas, buf_godz);
                printf("[%s] " BG_YELLOW BLACK " (KASJER): " RESET YELLOW
                    " Klient PID=%d - za malo pieniedzy. " RESET "\n",
                    buf_godz, temp.pid);
            }
        }

        memcpy(wsp_dane, &temp, sizeof(DaneOsoby));
        formatuj_czas((*(int*)adr_czas), buf_godz);
        printf("[%s] " BG_YELLOW BLACK " (KASJER): " RESET YELLOW
            " Zakonczono obsluge PID=%d. " RESET "\n",
            buf_godz, temp.pid);

        // zwalniamy semafor 3 – klient moze isc dalej
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
        printf(YELLOW " <<Kasjer>> Otrzymano SIGINT, koncze prace kasjera. " RESET "\n");
        sprzatanie_kasjera();
        exit(0);
    }
}

// Watek VIP
static void* watek_vip(void* arg) {
    (void)arg;

    printf(YELLOW " <<Kasjer>> Watek VIP dziala. " RESET "\n");

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
        if (rand() % 20 == 10) {
            strcpy(kom.opis, "karnet niewazny");
        }
        else if ((*(int*)adr_czas) > CALY_CZAS_SYM - GODZINA_SYM || global_zamkniecie) {
            strcpy(kom.opis, "kasa zamknieta");
        }
        else {
            strcpy(kom.opis, "mozesz wejsc");
            kom.wyjscie_czas = (*(int*)adr_czas) + GODZINA_SYM;

            blokada_mutex(&mut_godz_wyj);
            ostatni_wpuszczony = kom.wyjscie_czas;
            odblokowanie_mutex(&mut_godz_wyj);
        }

        if (msgsnd(msqid_vip, &kom, sizeof(kom) - sizeof(long), 0) == -1) {
            perror("msgsnd kasjer VIP");
            exit(EXIT_FAILURE);
        }

        formatuj_czas((*(int*)adr_czas), buf_godz);
        printf("[%s] " BG_YELLOW BLACK " (KASJER): " RESET YELLOW
            " Obsluzono VIP PID=%d. " RESET "\n", buf_godz, kom.pid_osoby);
    }

    return NULL;
}

// Watek przerwy, tu zrobimy przerwe w losowym momencie np. 3–6h
static void* watek_przerwa(void* arg) {
    (void)arg;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    int czas_lokalny;
    // Losowy moment startu przerwy: 3–6 h od startu
    int czas_przerwy = (rand() % 4 + 3) * GODZINA_SYM; // 3..6h
    while ((czas_lokalny = (*(int*)adr_czas)) < czas_przerwy) {
        pthread_testcancel();
    }
    opusc_semafor(sem_id, 4);
    global_zamkniecie = true;

    formatuj_czas((*(int*)adr_czas), buf_godz);
    printf("[%s] " BG_YELLOW BLACK " (KASJER): " RESET YELLOW
        " Nastapila przerwa! Czekamy az wszyscy wyjda... " RESET "\n", buf_godz);

    // czekamy, az wyjdzie ostatni
    while ((czas_lokalny = (*(int*)adr_czas)) < ostatni_wpuszczony) {
        pthread_testcancel();
    }

    formatuj_czas((*(int*)adr_czas), buf_godz);
    printf("[%s] " BG_YELLOW BLACK " (KASJER): " RESET YELLOW
        " Osrodek pusty, przerwa trwa 60 minut. " RESET "\n", buf_godz);

    // 1h przerwy
    while ((czas_lokalny = (*(int*)adr_czas)) < (ostatni_wpuszczony + GODZINA_SYM)) {
        pthread_testcancel();
    }

    formatuj_czas((*(int*)adr_czas), buf_godz);
    printf("[%s] " BG_YELLOW BLACK " (KASJER): " RESET YELLOW
        " Koniec przerwy, otwieramy ponownie! " RESET "\n", buf_godz);

    global_zamkniecie = false;
    podnies_semafor(sem_id, 4);

    return NULL;
}
