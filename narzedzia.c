#pragma once

#include "naglowki.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/resource.h>

// Konwersja sekund symulacji na hh:mm:ss
   
void formatuj_czas(int sekundy, char* buf) {
    int sec_local = sekundy % 60;
    int min_local = sekundy / 60;
    int hr_local = (min_local / 60) + 10; // start 10:00
    int min_res = min_local % 60;

    snprintf(buf, 16, "%02d:%02d:%02d", hr_local, min_res, sec_local);
}

//Funkcje operujace na semaforach
void podnies_semafor(int semid, int indeks) {
    struct sembuf sb;
    sb.sem_num = indeks;
    sb.sem_op = 1;
    sb.sem_flg = 0;

    while (semop(semid, &sb, 1) == -1) {
        if (errno != EINTR) {
            perror("Blad semop (podnies_semafor)");
            exit(EXIT_FAILURE);
        }
    }
}

void opusc_semafor(int semid, int indeks) {
    struct sembuf sb;
    sb.sem_num = indeks;
    sb.sem_op = -1;
    sb.sem_flg = 0;

    while (semop(semid, &sb, 1) == -1) {
        if (errno != EINTR) {
            perror("Blad semop (opusc_semafor)");
            exit(EXIT_FAILURE);
        }
    }
}

//Dodawanie i usuwanie do tablic pid, wiek
void wstaw_pid(int* tab, int rozmiar, int pid) {
    for (int i = 1; i <= rozmiar; i++) {
        if (tab[i] <= 0) {
            tab[i] = pid;
            break;
        }
    }
}

void usun_pid(int* tab, int rozmiar, int pid) {
    for (int i = 1; i <= rozmiar; i++) {
        if (tab[i] == pid) {
            tab[i] = 0;
            break;
        }
    }
}

void wstaw_pid_wiek(int(*tab)[POJ_REKREACJA + 1], int rozmiar, int pid, int wiek) {
    for (int i = 1; i <= rozmiar; i++) {
        if (tab[0][i] <= 0) {
            tab[0][i] = pid;
            tab[1][i] = wiek;
            break;
        }
    }
}

void usun_pid_wiek(int(*tab)[POJ_REKREACJA + 1], int rozmiar, int pid) {
    for (int i = 1; i <= rozmiar; i++) {
        if (tab[0][i] == pid) {
            tab[0][i] = 0;
            tab[1][i] = 0;
        }
    }
}

// Liczenie wystapien PID
int zlicz_pid(int* tab, int rozmiar, int szukany_pid) {
    int licz = 0;
    for (int i = 1; i <= rozmiar; i++) {
        if (tab[i] == szukany_pid) {
            licz++;
        }
    }
    return licz;
}
// Srednia wieku
double oblicz_srednia(int* tab, int rozmiar, int nowy) {
    int suma = 0, ile = 0;
    for (int i = 1; i <= rozmiar; i++) {
        if (tab[i] != 0) {
            suma += tab[i];
            ile++;
        }
    }
    return (double)(suma + nowy) / (double)(ile + 1);
}

//Mutex i usleep
void blokada_mutex(pthread_mutex_t* mtx) {
    int err = pthread_mutex_lock(mtx);
    if (err != 0) {
        fprintf(stderr, "pthread_mutex_lock - blad: %d\n", err);
        exit(EXIT_FAILURE);
    }
}

void odblokada_mutex(pthread_mutex_t* mtx) {
    int err = pthread_mutex_unlock(mtx);
    if (err != 0) {
        fprintf(stderr, "pthread_mutex_unlock - blad: %d\n", err);
        exit(EXIT_FAILURE);
    }
}

// Prosta funkcja usleep z ominięciem EINTR
void moj_usleep(int ms) {
    if (usleep(ms) != 0 && errno != EINTR) {
        perror("usleep");
        exit(EXIT_FAILURE);
    }
}

// Sprawdzanie statusu funkcji (np. tworzenie wątku)
void sprawdz_err(int status, const char* kom) {
    if (status != 0) {
        fprintf(stderr, "%s, kod: %d\n", kom, status);
        exit(EXIT_FAILURE);
    }
}
