


> # Temat 8: Basen

## Opis Tematu

W miasteczku znajduje się kompleks basenów krytych dostępny w godzinach od Tp do Tk. Kompleks składa się z trzech basenów:  
- **Basen olimpijski** – maksymalna liczba osób: X1  
- **Basen rekreacyjny** – maksymalna liczba osób: X2  
- **Brodzik dla dzieci** – maksymalna liczba osób: X3  

Osoby odwiedzające centrum basenowe kupują bilety czasowe, które pozwalają na korzystanie z dowolnych basenów zgodnie z regulaminem przez określony czas **Ti**. Dzieci poniżej 10 roku życia nie płacą za wstęp. Osoby z karnetami VIP mogą korzystać z basenu z pominięciem kolejki oczekujących.

Osoby odwiedzające basen pojawiają się w sposób losowy, w różnym wieku (od 1 roku do 70 lat). W skład obsługi basenu wchodzi ratownik przy każdym basenie oraz obsługa techniczna, która okresowo zamyka wszystkie baseny w celu wymiany wody.

---

## Zasady i Regulamin Basenu

1. **Ograniczenia wiekowe**:
   - Tylko osoby pełnoletnie mogą korzystać z basenu olimpijskiego.
   - Dzieci poniżej 10 roku życia mogą korzystać z basenów tylko w obecności opiekuna.
   - W brodziku mogą przebywać wyłącznie dzieci do 5 roku życia oraz ich opiekunowie.

2. **Dodatkowe zasady**:
   - Dzieci do 3 roku życia muszą nosić wodoodporne pieluchy.
   - Średnia wieku na basenie rekreacyjnym nie może przekroczyć 40 lat.
   - Noszenie czepków pływackich nie jest obowiązkowe.

---

## Opis Funkcjonalności Systemu

System zarządza użytkownikami oraz zasobami w kompleksie basenowym, zapewniając przestrzeganie regulaminu. System obejmuje:
1. **Procesy klientów (w tym dzieci i ich opiekunów)**:
   - Zakup biletu i wejście do odpowiedniego basenu zgodnie z zasadami.
   - Przenoszenie się pomiędzy basenami w przypadku sygnału od ratownika lub na podstawie preferencji.

2. **Procesy ratowników**:
   - Wydawanie sygnałów do natychmiastowego opuszczenia basenu w nadzorowanym obszarze.
   - Kontrola i zarządzanie bezpieczeństwem.

3. **Proces obsługi technicznej**:
   - Zamknięcie całego kompleksu w celu wymiany wody we wszystkich basenach.

---

**Testy:**
   -Test 1 sleepy zakomentowane
   -Test 2 zabicie procesu kasjera
   -Test 3 Usunieęcie plików FIFO w trakcie działania
   -Test 4 Zmiana obsługi semaforów np. w ratowniku

---
## Konfiguracja Programu
**Pobieranie**:
   Pobierz pliki basen.c, kasjer.c, ratownik.c, klient.c, funkcje.c, struktury.h oraz Makefile do jednego folderu.
   
**Kompilacja**:
   - Otwórz terminal przejdź w terminalu do lokalizacji pobranych plików za pomocą cd sciezka/do/folderu_z_plikami.
   - Wpisz make aby skompilować program.

**Uruchomienie**:
   Wpisz w terminalu make run lub ./basen Pojemność_Olimpijski(8) Pojemność_Rekreacji(16) Pojemność_Brodzika(10) Sekundy_Symulacji(w microsekundach:5000): (./basen 15 20 18 2500) aby uruchomić. (w nawiasch defaultowe zmienne)

## Link do Repozytorium GitHub
[Repozytorium projektu na GitHub](https://github.com/danielryz/Ryz-Projekt-BASEN)


