//struktury.h

#pragma once

#include <stdbool.h>
#include <sys/types.h>

// ----- Zmiana: maksymalne rozmiary tablic -----
#define MAX_POJ_OLIMPIJKA    200
#define MAX_POJ_REKREACJA    200
#define MAX_POJ_BRODZIK      200

// Zmienne globalne (faktyczne rozmiary i czas)
extern int POJ_OLIMPIJKA;
extern int POJ_REKREACJA;
extern int POJ_BRODZIK;
extern int SEK_SYMULACJI;

// Stałe czasowe
#define GODZINA_SYM     3600
#define MINUTA_SYM      60
#define CALY_CZAS_SYM   43200   // 12 godzin = 43200 s

// Maks. rozmiar łańcucha w komunikatach
#define MAX_DANE_KOMUNIKATU 64

// Typy komunikatów do kolejek wiadomości
#define MTYP_OSOBA_DO_NADZ   1
#define MTYP_NADZ_DO_OSOBA   2
#define MTYP_OSOBA_DO_KAS    3
#define MTYP_KAS_DO_OSOBA    4

// Reset koloru
#define RESET       "\x1B[0m"

// Kolory
#define BLACK       "\x1B[30m"
#define RED         "\x1B[31m"
#define GREEN       "\x1B[32m"
#define YELLOW      "\x1B[33m"
#define BLUE        "\x1B[34m"
#define MAGENTA     "\x1B[35m"
#define CYAN        "\x1B[36m"
#define WHITE       "\x1B[37m"

// Tła
#define BG_BLACK    "\x1B[40m"
#define BG_RED      "\x1B[41m"
#define BG_GREEN    "\x1B[42m"
#define BG_YELLOW   "\x1B[43m"
#define BG_BLUE     "\x1B[44m"
#define BG_MAGENTA  "\x1B[45m"
#define BG_CYAN     "\x1B[46m"
#define BG_WHITE    "\x1B[47m"

// Pogrubienie
#define BOLD      "\x1B[1m"


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

// Komunikat kasowy
typedef struct _KomunikatKasowy {
    long  mtype;
    int   pid_osoby;
    char  opis[MAX_DANE_KOMUNIKATU];
    int   wyjscie_czas;
} KomunikatKasowy;

// Komunikat nadzoru
typedef struct _KomunikatNadzoru {
    long  mtype;
    int   pid_osoby;
    char  info[MAX_DANE_KOMUNIKATU];
    int   wiek_osoby;
    int   wiek_opiekuna;
} KomunikatNadzoru;

// Struktura przechowujaca stan wszystkich basenow w pamieci wspoldzielonej.
typedef struct _BasenyData {
    // Olimpijski
    int tab_olimp[MAX_POJ_OLIMPIJKA + 1];

    // Rekreacyjny (dwuwymiarowa)
    int tab_rek[2][MAX_POJ_REKREACJA + 1];

    // Brodzik
    int tab_brodz[MAX_POJ_BRODZIK + 1];
} BasenyData;

