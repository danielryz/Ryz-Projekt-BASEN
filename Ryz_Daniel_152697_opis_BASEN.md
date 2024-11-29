


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

## Testy

### Test 1: Przestrzeganie limitów wiekowych
- Symulacja kilku użytkowników o różnym wieku próbujących wejść na basen:
  - Osoby niepełnoletnie próbują wejść na basen olimpijski (oczekiwany wynik: brak dostępu).
  - Dzieci bez opiekunów próbują wejść do brodzika (oczekiwany wynik: brak dostępu).

### Test 2: Priorytet dla VIP
- Test dla sytuacji, w której pojawia się kolejka użytkowników, a osoba z karnetem VIP omija kolejkę.

### Test 3: Sygnały ratownika
- Symulacja sygnału do opuszczenia basenu rekreacyjnego, weryfikacja czy wszyscy opuszczają obszar i przenoszą się na inne baseny lub oczekują.

### Test 4: Kontrola średniej wieku na basenie rekreacyjnym
- Dodanie użytkowników do basenu rekreacyjnego w sposób przekraczający średnią wieku (oczekiwany wynik: odmowa wstępu kolejnych użytkowników).

### Test 5: Zamknięcie kompleksu
- Symulacja pełnego zamknięcia kompleksu w celu wymiany wody i ponowne otwarcie (oczekiwany wynik: nikt nie przebywa w kompleksie podczas zamknięcia).

---

## Link do Repozytorium GitHub
[Repozytorium projektu na GitHub](https://github.com/danielryz/Ryz-Projekt-BASEN)


