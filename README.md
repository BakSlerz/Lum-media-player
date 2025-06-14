# Lum Media Player

Lekki odtwarzacz multimedialny napisany w C z wykorzystaniem GTK3 i GStreamer.

## Funkcje

- Odtwarzanie plików audio i wideo
- Obsługa metadanych i okładek albumów
- Tryb pełnoekranowy z autoukrywaniem kontrolek
- Obsługa wielu formatów multimedialnych
- Wsparcie dla wielu języków (wykrywanie języka systemowego)
- Intuicyjny interfejs użytkownika

## Wymagania

- GTK 3.0 lub nowszy
- GStreamer 1.0 lub nowszy
- Biblioteki deweloperskie GTK i GStreamer

## Kompilacja i instalacja

### Metoda 1: Skrypt instalacyjny (zalecana)

```bash
# Instalacja w /usr/local (domyślnie)
./install.sh

# Instalacja w /usr (systemowo)
./install.sh --system
```

### Metoda 2: Ręczna instalacja

```bash
# Kompilacja
make

# Instalacja w /usr/local (domyślnie)
sudo make install

# Instalacja w /usr (systemowo)
sudo make install PREFIX=/usr
```

### Ustawienie jako domyślny odtwarzacz

Skrypt instalacyjny automatycznie ustawia Lum Player jako domyślny odtwarzacz dla popularnych formatów multimedialnych. Jeśli chcesz ręcznie ustawić go jako domyślny odtwarzacz:

1. Przez interfejs graficzny:
   - Kliknij prawym przyciskiem myszy na pliku multimedialnym
   - Wybierz "Właściwości" lub "Otwórz za pomocą"
   - Wybierz "Lum Media Player" z listy aplikacji
   - Zaznacz opcję "Zapamiętaj ten wybór" lub "Ustaw jako domyślny"

2. Przez terminal:
   ```bash
   xdg-mime default lum-player.desktop video/mp4
   ```
   (powtórz dla innych formatów, które chcesz obsługiwać)

## Użycie

```bash
lum-player [plik]
```

## Skróty klawiszowe

- **F** lub **f**: Przełączanie trybu pełnoekranowego
- **Spacja**: Odtwarzanie/pauza
- **Esc**: Wyjście z trybu pełnoekranowego
- **M** lub **m**: Wyciszenie
- **S** lub **s**: Stop
- **Strzałki lewo/prawo**: Przewijanie 10 sekund wstecz/do przodu
- **Strzałki góra/dół**: Zwiększanie/zmniejszanie głośności

## Licencja

Ten projekt jest udostępniany na licencji GPL

