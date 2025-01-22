// ratownik.c
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

static void* wpuszczanie_olimp(void*);
static void* wpuszczanie_rek(void*);
static void* wpuszczanie_brodz(void*);
static void* wychodzenie(void*);
static void* sygnaly_ratownika(void*);
static void obsluga_sig(int);

pid_t pid_nadz1, pid_nadz2, pid_nadz3;
char  buf_cz[16];
char* adr_czas;
pthread_mutex_t mut_olimp, mut_rek, mut_brodz;
pthread_t t_wp, t_wy, t_sig;
KomunikatNadzoru msg_nadz;
int msq_clifegu, sem_id;
volatile bool flaga_przyjec, flaga_zabron_wstepu;
int ktory_basen_global;

int main() {
    signal(SIGINT, obsluga_sig);
    signal(SIGTSTP, SIG_DFL);

    if (setpgid(0, 0) == -1) {
        perror("setpgid ratownik main");
        exit(EXIT_FAILURE);
    }

    printf("<<Ratownicy>> Start procesu glownego ratownikow.\n");
    flaga_zabron_wstepu = false;
    flaga_przyjec = true;

    key_t klucz = ftok(".", 51);
    if (klucz == -1) {
        perror("ftok - ratownik");
        exit(EXIT_FAILURE);
    }
    key_t klucz_czas = ftok(".", 52);
    if (klucz_czas == -1) {
        perror("ftok - ratownik czas");
        exit(EXIT_FAILURE);
    }

    msq_clifegu = msgget(klucz_czas, 0600);
    if (msq_clifegu == -1) {
        perror("msgget - ratownik");
        exit(EXIT_FAILURE);
    }

    sem_id = semget(klucz, 8, 0600);
    if (sem_id == -1) {
        perror("semget - ratownik");
        exit(EXIT_FAILURE);
    }

    int shm_time = shmget(klucz_czas, sizeof(int), 0600);
    if (shm_time == -1) {
        perror("shmget ratownik - czas");
        exit(EXIT_FAILURE);
    }
    adr_czas = (char*)shmat(shm_time, NULL, 0);
    if (adr_czas == (char*)(-1)) {
        perror("shmat - ratownik, czas");
        exit(EXIT_FAILURE);
    }

    // Tworzymy 3 procesy ratownicze
    pid_nadz1 = fork();
    if (pid_nadz1 < 0) {
        perror("fork rat1");
        exit(EXIT_FAILURE);
    }
    else if (pid_nadz1 == 0) {
        srand(getpid());
        signal(SIGTSTP, SIG_DFL);
        if (setpgid(0, getppid()) == -1) {
            perror("setpgid rat1");
            exit(EXIT_FAILURE);
        }
        ktory_basen_global = 1;

        int tab_olimp[POJ_OLIMPIJKA + 1];
        memset(tab_olimp, 0, sizeof(tab_olimp));

        int st = pthread_mutex_init(&mut_olimp, NULL);
        sprawdz_err(st, "mutex_init olimp");

        st = pthread_create(&t_wp, NULL, wpuszczanie_olimp, tab_olimp);
        sprawdz_err(st, "pthread_create wpuszczanie olimp");

        st = pthread_create(&t_sig, NULL, sygnaly_ratownika, tab_olimp);
        sprawdz_err(st, "pthread_create sygnaly olimp");

        st = pthread_create(&t_wy, NULL, wychodzenie, tab_olimp);
        sprawdz_err(st, "pthread_create wychodzenie olimp");

        pthread_join(t_sig, NULL);
        pthread_join(t_wp, NULL);
        pthread_join(t_wy, NULL);

        pthread_mutex_destroy(&mut_olimp);
        exit(0);
    }
    else {
        pid_nadz2 = fork();
        if (pid_nadz2 < 0) {
            perror("fork rat2");
            kill(pid_nadz1, SIGINT);
            exit(EXIT_FAILURE);
        }
        else if (pid_nadz2 == 0) {
            srand(getpid());
            signal(SIGTSTP, SIG_DFL);
            if (setpgid(0, getppid()) == -1) {
                perror("setpgid rat2");
                exit(EXIT_FAILURE);
            }
            ktory_basen_global = 2;

            int tab_rek[2][POJ_REKREACJA + 1];
            memset(tab_rek, 0, sizeof(tab_rek));

            int st = pthread_mutex_init(&mut_rek, NULL);
            sprawdz_err(st, "mutex_init rek");

            st = pthread_create(&t_wp, NULL, wpuszczanie_rek, tab_rek);
            sprawdz_err(st, "pthread_create wpuszczanie rek");

            st = pthread_create(&t_sig, NULL, sygnaly_ratownika, tab_rek);
            sprawdz_err(st, "pthread_create sygnaly rek");

            st = pthread_create(&t_wy, NULL, wychodzenie, tab_rek);
            sprawdz_err(st, "pthread_create wychodzenie rek");

            pthread_join(t_sig, NULL);
            pthread_join(t_wp, NULL);
            pthread_join(t_wy, NULL);

            pthread_mutex_destroy(&mut_rek);
            exit(0);
        }
        else {
            pid_nadz3 = fork();
            if (pid_nadz3 < 0) {
                perror("fork rat3");
                kill(pid_nadz1, SIGINT);
                kill(pid_nadz2, SIGINT);
                exit(EXIT_FAILURE);
            }
            else if (pid_nadz3 == 0) {
                srand(getpid());
                signal(SIGTSTP, SIG_DFL);
                if (setpgid(0, getppid()) == -1) {
                    perror("setpgid rat3");
                    exit(EXIT_FAILURE);
                }
                ktory_basen_global = 3;

                int tab_brodz[POJ_BRODZIK + 1];
                memset(tab_brodz, 0, sizeof(tab_brodz));

                int st = pthread_mutex_init(&mut_brodz, NULL);
                sprawdz_err(st, "mutex_init brodz");

                st = pthread_create(&t_wp, NULL, wpuszczanie_brodz, tab_brodz);
                sprawdz_err(st, "pthread_create wpuszczanie brodz");

                st = pthread_create(&t_sig, NULL, sygnaly_ratownika, tab_brodz);
                sprawdz_err(st, "pthread_create sygnaly brodz");

                st = pthread_create(&t_wy, NULL, wychodzenie, tab_brodz);
                sprawdz_err(st, "pthread_create wychodzenie brodz");

                pthread_join(t_sig, NULL);
                pthread_join(t_wp, NULL);
                pthread_join(t_wy, NULL);

                pthread_mutex_destroy(&mut_brodz);
                exit(0);
            }
        }
    }

    waitpid(pid_nadz1, NULL, 0);
    waitpid(pid_nadz2, NULL, 0);
    waitpid(pid_nadz3, NULL, 0);

    shmdt(adr_czas);
    return 0;
}

static void obsluga_sig(int s) {
    flaga_przyjec = false;
    int r;
    if ((r = pthread_cancel(t_wp)) != 0 && r != ESRCH) {
        perror("pthread_cancel - t_wp");
        exit(EXIT_FAILURE);
    }
    if ((r = pthread_cancel(t_wy)) != 0 && r != ESRCH) {
        perror("pthread_cancel - t_wy");
        exit(EXIT_FAILURE);
    }
    if ((r = pthread_cancel(t_sig)) != 0 && r != ESRCH) {
        perror("pthread_cancel - t_sig");
        exit(EXIT_FAILURE);
    }
}

static void* wpuszczanie_olimp(void* arg) {
    int* arr = (int*)arg;
    KomunikatNadzoru km;
    while (flaga_przyjec) {
        if (msgrcv(msq_clifegu, &km, sizeof(km) - sizeof(long), 3, 0) == -1) {
            if (errno != EINTR) {
                perror("msgrcv - rat1");
                exit(EXIT_FAILURE);
            }
        }
        strcpy(km.info, "OK");
        km.mtype = km.pid_osoby;

        blokada_mutex(&mut_olimp);
        if (flaga_zabron_wstepu) {
            strcpy(km.info, "wstep_zabroniony");
        }
        else if (km.wiek_osoby < 18) {
            strcpy(km.info, "za_mlody_na_olimpijski");
        }
        else if (arr[0] == POJ_OLIMPIJKA) {
            strcpy(km.info, "olimp_pelny");
        }
        else {
            arr[0]++;
            wstaw_pid(arr, POJ_OLIMPIJKA, km.pid_osoby);
        }
        odblokada_mutex(&mut_olimp);

        if (msgsnd(msq_clifegu, &km, sizeof(km) - sizeof(long), 0) == -1) {
            perror("msgsnd - rat1");
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

static void* wpuszczanie_rek(void* arg) {
    int(*arr)[POJ_REKREACJA + 1] = (int(*)[POJ_REKREACJA + 1])arg;
    KomunikatNadzoru km;
    while (flaga_przyjec) {
        if (msgrcv(msq_clifegu, &km, sizeof(km) - sizeof(long), 4, 0) == -1) {
            if (errno != EINTR) {
                perror("msgrcv - rat2");
                exit(EXIT_FAILURE);
            }
        }
        strcpy(km.info, "OK");
        km.mtype = km.pid_osoby;

        int nowi = (km.wiek_osoby < 10) ? 2 : 1;
        blokada_mutex(&mut_rek);
        double sr = oblicz_srednia(arr[1], POJ_REKREACJA, km.wiek_osoby + km.wiek_opiekuna);

        if (flaga_zabron_wstepu) {
            strcpy(km.info, "wstep_zabroniony");
        }
        else if (arr[0][0] + nowi > POJ_REKREACJA) {
            strcpy(km.info, "rek_pelny");
        }
        else if (sr > 40) {
            strcpy(km.info, "rek_srednia_zbyt_wysoka");
        }
        else {
            arr[0][0] += nowi;
            wstaw_pid_wiek(arr, POJ_REKREACJA, km.pid_osoby, km.wiek_osoby);
            if (km.wiek_opiekuna) {
                wstaw_pid_wiek(arr, POJ_REKREACJA, km.pid_osoby, km.wiek_opiekuna);
            }
        }
        odblokada_mutex(&mut_rek);

        if (msgsnd(msq_clifegu, &km, sizeof(km) - sizeof(long), 0) == -1) {
            perror("msgsnd - rat2");
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

static void* wpuszczanie_brodz(void* arg) {
    int* arr = (int*)arg;
    KomunikatNadzoru km;
    while (flaga_przyjec) {
        if (msgrcv(msq_clifegu, &km, sizeof(km) - sizeof(long), 5, 0) == -1) {
            if (errno != EINTR) {
                perror("msgrcv - rat3");
                exit(EXIT_FAILURE);
            }
        }
        strcpy(km.info, "OK");
        km.mtype = km.pid_osoby;

        blokada_mutex(&mut_brodz);
        if (flaga_zabron_wstepu) {
            strcpy(km.info, "wstep_zabroniony");
        }
        else if (km.wiek_osoby > 5) {
            strcpy(km.info, "za_stary_na_brodzik");
        }
        else if (arr[0] == POJ_BRODZIK) {
            strcpy(km.info, "brodzik_pelny");
        }
        else {
            arr[0]++;
            wstaw_pid(arr, POJ_BRODZIK, km.pid_osoby);
        }
        odblokada_mutex(&mut_brodz);

        if (msgsnd(msq_clifegu, &km, sizeof(km) - sizeof(long), 0) == -1) {
            perror("msgsnd - rat3");
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

static void* wychodzenie(void* arg) {
    int* arr = (int*)arg;
    int  pid_tmp;

    char plik_fifo[32];
    snprintf(plik_fifo, sizeof(plik_fifo), "kana_bas_%d", ktory_basen_global);

    int fd_fifo = open(plik_fifo, O_RDONLY);
    if (fd_fifo < 0) {
        perror("open fifo - ratownik");
        exit(EXIT_FAILURE);
    }

    while (flaga_przyjec) {
        ssize_t nread = read(fd_fifo, &pid_tmp, sizeof(pid_tmp));
        if (nread == -1) {
            perror("read fifo - ratownik");
            exit(EXIT_FAILURE);
        }
        else if (nread == 0) {
            continue;
        }

        switch (ktory_basen_global) {
        case 1:
            blokada_mutex(&mut_olimp);
            formatuj_czas(*(int*)adr_czas, buf_cz);
            printf("[%s] (RATOWNIK OLIMPIJSKI) Klient PID=%d wychodzi.\n", buf_cz, pid_tmp);

            arr[0]--;
            usun_pid(arr, POJ_OLIMPIJKA, pid_tmp);
            odblokada_mutex(&mut_olimp);
            break;
        case 2: {
            blokada_mutex(&mut_rek);
            formatuj_czas(*(int*)adr_czas, buf_cz);
            printf("[%s] (RATOWNIK REKREACYJNY) Klient PID=%d wychodzi.\n", buf_cz, pid_tmp);

            int(*arr2)[POJ_REKREACJA + 1] = (int(*)[POJ_REKREACJA + 1])arr;
            int ile = zlicz_pid(arr2[0], POJ_REKREACJA, pid_tmp);
            arr2[0][0] -= ile;
            for (int i = 0; i < ile; i++) {
                usun_pid_wiek(arr2, POJ_REKREACJA, pid_tmp);
            }
            odblokada_mutex(&mut_rek);
            break;
        }
        case 3:
            blokada_mutex(&mut_brodz);
            formatuj_czas(*(int*)adr_czas, buf_cz);
            printf("[%s] (RATOWNIK BRODZIK) Klient PID=%d wychodzi.\n", buf_cz, pid_tmp);

            arr[0]--;
            usun_pid(arr, POJ_BRODZIK, pid_tmp);
            odblokada_mutex(&mut_brodz);
            break;
        }
    }

    close(fd_fifo);
    return NULL;
}

// Watek wysylajacy sygnaly SIGUSR1/SIGUSR2 w pewnych odstepach, wyrzucajac ludzi z basenu
static void* sygnaly_ratownika(void* arg) {
    int* arr = (int*)arg;
    union sigval sig_data;
    sig_data.sival_int = ktory_basen_global;

    while (*(int*)adr_czas < (CALA_DOBA_SYM - 2 * GODZINA_SYM)) {
        int t_biez, t1, t2, rozn;
        t_biez = *(int*)adr_czas;
        t1 = t_biez + ((rand() % (31 * MINUTA_SYM)) + 30 * MINUTA_SYM);
        rozn = (rand() % (36 * MINUTA_SYM)) + 15 * MINUTA_SYM;
        t2 = t1 + rozn;
        if (t2 > CALA_DOBA_SYM - 2 * GODZINA_SYM) {
            continue;
        }

        while (*(int*)adr_czas < t1) {
            pthread_testcancel();
        }
        formatuj_czas(*(int*)adr_czas, buf_cz);
        printf("[%s] (RATOWNIK) Wysylamy SIGUSR1 do basenu #%d\n", buf_cz, ktory_basen_global);

        int rozmiar_basen = (ktory_basen_global == 1 ? POJ_OLIMPIJKA :
            (ktory_basen_global == 2 ? POJ_REKREACJA : POJ_BRODZIK));
        int wyrzuceni[rozmiar_basen];
        memset(wyrzuceni, 0, sizeof(wyrzuceni));

        switch (ktory_basen_global) {
        case 1:
            blokada_mutex(&mut_olimp);
            flaga_zabron_wstepu = true;
            for (int i = 1; i <= POJ_OLIMPIJKA; i++) {
                if (arr[i]) {
                    wyrzuceni[i - 1] = arr[i];
                    if (sigqueue(arr[i], SIGUSR1, sig_data) == -1 && errno != ESRCH) {
                        perror("sigqueue - olimp1");
                        exit(EXIT_FAILURE);
                    }
                    arr[i] = 0;
                }
            }
            arr[0] = 0;
            odblokada_mutex(&mut_olimp);
            break;
        case 2: {
            blokada_mutex(&mut_rek);
            flaga_zabron_wstepu = true;
            int(*arr2)[POJ_REKREACJA + 1] = (int(*)[POJ_REKREACJA + 1])arr;
            for (int i = 1; i <= POJ_REKREACJA; i++) {
                if (arr2[0][i]) {
                    wyrzuceni[i - 1] = arr2[0][i];
                    if (sigqueue(arr2[0][i], SIGUSR1, sig_data) == -1 && errno != ESRCH) {
                        perror("sigqueue - rek1");
                        exit(EXIT_FAILURE);
                    }
                    arr2[0][i] = 0;
                    arr2[1][i] = 0;
                }
            }
            arr2[0][0] = 0;
            odblokada_mutex(&mut_rek);
            break;
        }
        case 3:
            blokada_mutex(&mut_brodz);
            flaga_zabron_wstepu = true;
            for (int i = 1; i <= POJ_BRODZIK; i++) {
                if (arr[i]) {
                    wyrzuceni[i - 1] = arr[i];
                    if (sigqueue(arr[i], SIGUSR1, sig_data) == -1 && errno != ESRCH) {
                        perror("sigqueue - brodz1");
                        exit(EXIT_FAILURE);
                    }
                    arr[i] = 0;
                }
            }
            arr[0] = 0;
            odblokada_mutex(&mut_brodz);
            break;
        }

        while (*(int*)adr_czas < t2) {
            pthread_testcancel();
        }

        // Wysylamy SIGUSR2 do tych wyrzuconych
        switch (ktory_basen_global) {
        case 1:
            blokada_mutex(&mut_olimp);
            for (int i = 0; i < POJ_OLIMPIJKA; i++) {
                if (wyrzuceni[i]) {
                    if (sigqueue(wyrzuceni[i], SIGUSR2, sig_data) == -1 && errno != ESRCH) {
                        perror("sigqueue - olimp2");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            odblokada_mutex(&mut_olimp);
            break;
        case 2: {
            blokada_mutex(&mut_rek);
            int(*arr2)[POJ_REKREACJA + 1] = (int(*)[POJ_REKREACJA + 1])arr;
            for (int i = 0; i < POJ_REKREACJA; i++) {
                if (wyrzuceni[i]) {
                    if (sigqueue(wyrzuceni[i], SIGUSR2, sig_data) == -1 && errno != ESRCH) {
                        perror("sigqueue - rek2");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            odblokada_mutex(&mut_rek);
            break;
        }
        case 3:
            blokada_mutex(&mut_brodz);
            for (int i = 0; i < POJ_BRODZIK; i++) {
                if (wyrzuceni[i]) {
                    if (sigqueue(wyrzuceni[i], SIGUSR2, sig_data) == -1 && errno != ESRCH) {
                        perror("sigqueue - brodz2");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            odblokada_mutex(&mut_brodz);
            break;
        }

        formatuj_czas(*(int*)adr_czas, buf_cz);
        printf("[%s] (RATOWNIK) Wysylamy SIGUSR2 do basenu #%d, koniec czyszczenia.\n", buf_cz, ktory_basen_global);

        flaga_zabron_wstepu = false;
    }

    return NULL;
}

