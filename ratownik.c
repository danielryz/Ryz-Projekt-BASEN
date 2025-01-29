//ratownik.c

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

static void* wpuszczanie_olimp(void*);
static void* wpuszczanie_rek(void*);
static void* wpuszczanie_brodz(void*);
static void* wychodzenie(void*);
static void* sygnaly_ratownika(void*);
static void sprzatanie_ratownika();
static void obsluga_sig(int);

BasenyData* gl_basen;

pid_t pid_nadz1, pid_nadz2, pid_nadz3;
char  buf_cz[16];
char* adr_czas;
pthread_mutex_t mut_olimp, mut_rek, mut_brodz;
pthread_t t_wp, t_wy, t_sig;
int msq_clifegu, sem_id;
volatile bool flaga_przyjec, flaga_zabron_wstepu;
int ktory_basen_global = 0;

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

    printf("Proces ratownik: Otrzymano POJ_OLIMPIJKA=%d, POJ_REKREACJA=%d, POJ_BRODZIK=%d, SEK_SYMULACJI=%d\n",
        POJ_OLIMPIJKA, POJ_REKREACJA, POJ_BRODZIK, SEK_SYMULACJI);

    signal(SIGINT, obsluga_sig);
    signal(SIGTSTP, SIG_DFL);

    // Tworzymy grupę procesów dla ratowników
    if (setpgid(0, 0) == -1) {
        perror("setpgid ratownik main");
        exit(EXIT_FAILURE);
    }

    printf(BLUE " <<Ratownicy>> Start procesu glownego ratownikow. " RESET "\n");
    flaga_zabron_wstepu = false;
    flaga_przyjec = true;

    key_t klucz = ftok(".", 100);
    if (klucz == -1) {
        perror("ftok - ratownik");
        exit(EXIT_FAILURE);
    }
    key_t klucz_czas = ftok(".", 200);
    if (klucz_czas == -1) {
        perror("ftok - ratownik czas");
        exit(EXIT_FAILURE);
    }

    // Pamięć wspólna basen
    int shm_id_basen = shmget(klucz + 10, sizeof(BasenyData), 0600);
    if (shm_id_basen == -1) {
        perror("shmget basen data - ratownik");
        exit(EXIT_FAILURE);
    }
    gl_basen = (BasenyData*)shmat(shm_id_basen, NULL, 0);
    if (gl_basen == (void*)-1) {
        perror("shmat basen data - ratownik");
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

    // Tworzymy 3 procesy (ratownicy)
    pid_nadz1 = fork();
    if (pid_nadz1 < 0) {
        perror("fork rat1");
        exit(EXIT_FAILURE);
    }
    else if (pid_nadz1 == 0) {
        srand(getpid() ^ time(NULL));      // aby sygnaly byly bardziej losowe
        usleep(rand() % 500000);           // lekkie opoznienie startu
        signal(SIGTSTP, SIG_DFL);
        if (setpgid(0, getppid()) == -1) {
            perror("setpgid rat1");
        }
        ktory_basen_global = 1;

        int st = pthread_mutex_init(&mut_olimp, NULL);
        sprawdz_err(st, "mutex_init olimp");

        st = pthread_create(&t_wp, NULL, wpuszczanie_olimp, gl_basen->tab_olimp);
        sprawdz_err(st, "pthread_create wpuszczanie olimp");

        st = pthread_create(&t_sig, NULL, sygnaly_ratownika, gl_basen->tab_olimp);
        sprawdz_err(st, "pthread_create sygnaly olimp");

        st = pthread_create(&t_wy, NULL, wychodzenie, gl_basen->tab_olimp);
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
            srand(getpid() ^ time(NULL));
            usleep(rand() % 500000);
            signal(SIGTSTP, SIG_DFL);
            if (setpgid(0, getppid()) == -1) {
                perror("setpgid rat2");
            }
            ktory_basen_global = 2;

            int st = pthread_mutex_init(&mut_rek, NULL);
            sprawdz_err(st, "mutex_init rek");

            st = pthread_create(&t_wp, NULL, wpuszczanie_rek, gl_basen->tab_rek);
            sprawdz_err(st, "pthread_create wpuszczanie rek");

            st = pthread_create(&t_sig, NULL, sygnaly_ratownika, gl_basen->tab_rek);
            sprawdz_err(st, "pthread_create sygnaly rek");

            st = pthread_create(&t_wy, NULL, wychodzenie, gl_basen->tab_rek);
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
                srand(getpid() ^ time(NULL));
                usleep(rand() % 500000);
                signal(SIGTSTP, SIG_DFL);
                if (setpgid(0, getppid()) == -1) {
                    perror("setpgid rat3");
                }
                ktory_basen_global = 3;

                int st = pthread_mutex_init(&mut_brodz, NULL);
                sprawdz_err(st, "mutex_init brodz");

                st = pthread_create(&t_wp, NULL, wpuszczanie_brodz, gl_basen->tab_brodz);
                sprawdz_err(st, "pthread_create wpuszczanie brodz");

                st = pthread_create(&t_sig, NULL, sygnaly_ratownika, gl_basen->tab_brodz);
                sprawdz_err(st, "pthread_create sygnaly brodz");

                st = pthread_create(&t_wy, NULL, wychodzenie, gl_basen->tab_brodz);
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
    shmdt(gl_basen);
    return 0;
}

static void sprzatanie_ratownika() {
    int st;

    if (shmdt(adr_czas) == -1) {
        perror("shmdt - czas");
    }
    if (shmdt(gl_basen) == -1) {
        perror("shmdt - basen");
    }

    if ((st = pthread_cancel(t_wp)) != 0 && st != ESRCH) {
        fprintf(stderr, "pthread_cancel wpuszczanie - kod: %d\n", st);
    }
    pthread_join(t_wp, NULL);

    if ((st = pthread_cancel(t_wy)) != 0 && st != ESRCH) {
        fprintf(stderr, "pthread_cancel wychodzenie - kod: %d\n", st);
    }
    pthread_join(t_wy, NULL);

    if ((st = pthread_cancel(t_sig)) != 0 && st != ESRCH) {
        fprintf(stderr, "pthread_cancel sygnaly - kod: %d\n", st);
    }
    pthread_join(t_sig, NULL);

    if (ktory_basen_global == 1) {
        pthread_mutex_destroy(&mut_olimp);
    }
    else if (ktory_basen_global == 2) {
        pthread_mutex_destroy(&mut_rek);
    }
    else if (ktory_basen_global == 3) {
        pthread_mutex_destroy(&mut_brodz);
    }
}

static void obsluga_sig(int sig) {
    if (sig == SIGINT) {
        flaga_przyjec = false;
        printf(BLUE " <<Ratownik>> Otrzymano SIGINT, kończę pracę ratownika. " RESET "\n");
        sprzatanie_ratownika();
        exit(0);
    }
}

static void* wpuszczanie_olimp(void* arg) {
    int* tab_olimp = (int*)arg;
    KomunikatNadzoru km;
    while (flaga_przyjec) {
        if (msgrcv(msq_clifegu, &km, sizeof(km) - sizeof(long), 3, 0) == -1) {
            if (errno == EINTR) continue;
            perror("msgrcv - rat1");
            exit(EXIT_FAILURE);
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
        else if (tab_olimp[0] == POJ_OLIMPIJKA) {
            strcpy(km.info, "olimp_pelny");
        }
        else {
            tab_olimp[0]++;
            wstaw_pid(tab_olimp, POJ_OLIMPIJKA, km.pid_osoby);

            // Wizualizacja
            podnies_semafor(sem_id, 7);
            wizualizacja_stanu_basenu("Basen Olimpijski",
                tab_olimp[0], // ilu w środku
                POJ_OLIMPIJKA,
                tab_olimp,
                POJ_OLIMPIJKA + 1);
            opusc_semafor(sem_id, 7);
        }
        odblokowanie_mutex(&mut_olimp);

        if (msgsnd(msq_clifegu, &km, sizeof(km) - sizeof(long), 0) == -1) {
            perror("msgsnd - rat1");
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

static void* wpuszczanie_rek(void* arg) {
    int(*tab_rek)[MAX_POJ_REKREACJA + 1] = (int(*)[MAX_POJ_REKREACJA + 1])arg;
    KomunikatNadzoru km;
    while (flaga_przyjec) {
        if (msgrcv(msq_clifegu, &km, sizeof(km) - sizeof(long), 4, 0) == -1) {
            if (errno == EINTR) continue;
            perror("msgrcv - rat2");
            exit(EXIT_FAILURE);
        }
        strcpy(km.info, "OK");
        km.mtype = km.pid_osoby;

        int nowi = (km.wiek_osoby < 10) ? 2 : 1;

        blokada_mutex(&mut_rek);
        double sr = oblicz_srednia(tab_rek[1], POJ_REKREACJA, km.wiek_osoby + km.wiek_opiekuna);

        if (flaga_zabron_wstepu) {
            strcpy(km.info, "wstep_zabroniony");
        }
        else if (tab_rek[0][0] + nowi > POJ_REKREACJA) {
            strcpy(km.info, "rek_pelny");
        }
        else if (sr > 40) {
            strcpy(km.info, "rek_srednia_zbyt_wysoka");
        }
        else {
            tab_rek[0][0] += nowi;
            wstaw_pid_wiek(tab_rek, POJ_REKREACJA, km.pid_osoby, km.wiek_osoby);
            if (km.wiek_opiekuna) {
                wstaw_pid_wiek(tab_rek, POJ_REKREACJA, km.pid_osoby, km.wiek_opiekuna);
            }

            podnies_semafor(sem_id, 7);
            wizualizacja_stanu_basenu_rek("Basen Rekreacyjny",
                tab_rek[0][0],
                POJ_REKREACJA,
                tab_rek,
                POJ_REKREACJA + 1,
                sr);
            opusc_semafor(sem_id, 7);
        }
        odblokowanie_mutex(&mut_rek);

        if (msgsnd(msq_clifegu, &km, sizeof(km) - sizeof(long), 0) == -1) {
            perror("msgsnd - rat2");
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

static void* wpuszczanie_brodz(void* arg) {
    int* tab_brodz = (int*)arg;
    KomunikatNadzoru km;
    while (flaga_przyjec) {
        if (msgrcv(msq_clifegu, &km, sizeof(km) - sizeof(long), 5, 0) == -1) {
            if (errno == EINTR) continue;
            perror("msgrcv - rat3");
            exit(EXIT_FAILURE);
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
        else if (tab_brodz[0] == POJ_BRODZIK) {
            strcpy(km.info, "brodzik_pelny");
        }
        else {
            tab_brodz[0]++;
            wstaw_pid(tab_brodz, POJ_BRODZIK, km.pid_osoby);

            podnies_semafor(sem_id, 7);
            wizualizacja_stanu_basenu("  Basen Brodzik ",
                tab_brodz[0],
                POJ_BRODZIK,
                tab_brodz,
                POJ_BRODZIK + 1);
            opusc_semafor(sem_id, 7);
        }
        odblokowanie_mutex(&mut_brodz);

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
    snprintf(plik_fifo, sizeof(plik_fifo), "kanal_bas_%d", ktory_basen_global);

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
            printf("[%s] " BG_BLUE BLACK " (RATOWNIK OLIMPIJSKI): " RESET BLUE
                " Klient PID=%d wychodzi. " RESET "\n", buf_cz, pid_tmp);

            arr[0]--;
            usun_pid(arr, POJ_OLIMPIJKA, pid_tmp);
            odblokowanie_mutex(&mut_olimp);
            break;
        case 2: {
            blokada_mutex(&mut_rek);
            formatuj_czas(*(int*)adr_czas, buf_cz);
            printf("[%s] " BG_BLUE BLACK " (RATOWNIK REKREACYJNY): " RESET BLUE
                " Klient PID=%d wychodzi. " RESET "\n", buf_cz, pid_tmp);

            int(*arr2)[MAX_POJ_REKREACJA + 1] = (int(*)[MAX_POJ_REKREACJA + 1])arr;
            int ile = zlicz_pid(arr2[0], POJ_REKREACJA, pid_tmp);
            arr2[0][0] -= ile;
            for (int i = 0; i < ile; i++) {
                usun_pid_wiek(arr2, POJ_REKREACJA, pid_tmp);
            }
            odblokowanie_mutex(&mut_rek);
            break;
        }
        case 3:
            blokada_mutex(&mut_brodz);
            formatuj_czas(*(int*)adr_czas, buf_cz);
            printf("[%s] " BG_BLUE BLACK " (RATOWNIK BRODZIK): " RESET BLUE
                " Klient PID=%d wychodzi. " RESET "\n", buf_cz, pid_tmp);

            arr[0]--;
            usun_pid(arr, POJ_BRODZIK, pid_tmp);
            odblokowanie_mutex(&mut_brodz);
            break;
        }
    }

    close(fd_fifo);
    return NULL;
}

// Watek wysylajacy sygnaly SIGUSR1/SIGUSR2
static void* sygnaly_ratownika(void* arg) {
    int* arr = (int*)arg;
    union sigval sig_data;
    sig_data.sival_int = ktory_basen_global;

    while (*(int*)adr_czas < (CALY_CZAS_SYM - 2 * GODZINA_SYM)) {
        int t_biez, t1, t2, rozn;
        t_biez = *(int*)adr_czas;
        t1 = t_biez + ((rand() % (31 * MINUTA_SYM)) + 30 * MINUTA_SYM);
        rozn = (rand() % (36 * MINUTA_SYM)) + 15 * MINUTA_SYM;
        t2 = t1 + rozn;
        if (t2 > CALY_CZAS_SYM - 2 * GODZINA_SYM) {
            continue;
        }

        while (*(int*)adr_czas < t1) {
            pthread_testcancel();
        }
        formatuj_czas(*(int*)adr_czas, buf_cz);
        printf("[%s] " BG_BLUE BLACK " (RATOWNIK): " RESET BLUE " Wysylamy SIGUSR1 do basenu #%d " RESET "\n", buf_cz, ktory_basen_global);

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
            odblokowanie_mutex(&mut_olimp);
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
            odblokowanie_mutex(&mut_rek);
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
            odblokowanie_mutex(&mut_brodz);
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
            odblokowanie_mutex(&mut_olimp);
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
            odblokowanie_mutex(&mut_rek);
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
            odblokowanie_mutex(&mut_brodz);
            break;
        }

        formatuj_czas(*(int*)adr_czas, buf_cz);
        printf("[%s] " BG_BLUE BLACK " (RATOWNIK): " RESET BLUE " Wysylamy SIGUSR2 do basenu #%d, koniec czyszczenia. " RESET "\n", buf_cz, ktory_basen_global);

        flaga_zabron_wstepu = false;
    }

    return NULL;
}
