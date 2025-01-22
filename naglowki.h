#pragma once

#include <stdbool.h>
#include <sys/types.h>

// Czas symulacj
#define SEK_SYMULACJI        2500     // mikrosekundy na jedna s symulacji
#define GODZINA_SYM          3600     // 1 godz = 3600 s
#define MINUTA_SYM           60       // 1 min = 60 s
#define CALA_DOBA_SYM        43200    // 12 godz = 43200 s 

// Maks i rozmiary basenow
#define MAX_DANE_KOMUNIKATU  64
#define POJ_OLIMPIJKA        6
#define POJ_REKREACJA        16
#define POJ_BRODZIK          5

// Typy komunikatow do kolejek wiadomosci
#define MTYP_OSOBA_DO_NADZ   1
#define MTYP_NADZ_DO_OSOBA   2
#define MTYP_OSOBA_DO_KAS    3
#define MTYP_KAS_DO_OSOBA    4

// Struktura opisujaca pojedyncza osobe (klienta basenu)
typedef struct _DaneOsoby {
    int   pid;
    int   wiek;
    int   wiek_opiekuna;
    bool  pampers;
    bool  czepek;
    double kasa;
    bool  wpuszczony;
    bool  vip;
    int   czas_wyjscia;
} DaneOsoby;

// Komunikat wymieniany miedzy klientem VIP a kasjerem
typedef struct _KomunikatKasowy {
    long  mtype;
    pid_t pid_osoby;
    char  opis[MAX_DANE_KOMUNIKATU];
    int   wyjscie_czas;
} KomunikatKasowy;

// Komunikat między klientem a ratownikiem
typedef struct _KomunikatNadzoru {
    long  mtype;
    pid_t pid_osoby;
    char  info[MAX_DANE_KOMUNIKATU];
    int   wiek_osoby;
    int   wiek_opiekuna;
} KomunikatNadzoru;
