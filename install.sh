#!/bin/bash

# Skrypt instalacyjny dla Lum Media Player

echo "=== Lum Media Player - Instalator ==="
echo "Ten skrypt zainstaluje Lum Media Player w systemie."
echo "Wymagane są uprawnienia administratora."
echo ""

# Sprawdź, czy użytkownik ma uprawnienia roota
if [ "$EUID" -ne 0 ]; then
  echo "Uruchamianie z uprawnieniami administratora..."
  sudo "$0" "$@"
  exit $?
fi

# Ścieżka do katalogu instalacyjnego
INSTALL_DIR="/usr/local"
if [ "$1" == "--system" ]; then
  INSTALL_DIR="/usr"
fi

echo "Instalowanie do: $INSTALL_DIR"

# Kompilacja programu
echo "Kompilowanie programu..."
make clean
make

# Instalacja programu
echo "Instalowanie programu..."
make install PREFIX=$INSTALL_DIR

# Aktualizacja cache ikon
echo "Aktualizowanie cache ikon..."
if command -v gtk-update-icon-cache &> /dev/null; then
  gtk-update-icon-cache -f -t $INSTALL_DIR/share/icons/hicolor
fi

# Aktualizacja bazy danych aplikacji
echo "Aktualizowanie bazy danych aplikacji..."
if command -v update-desktop-database &> /dev/null; then
  update-desktop-database $INSTALL_DIR/share/applications
fi

# Ustawienie jako domyślny odtwarzacz
echo "Ustawianie jako domyślny odtwarzacz multimedialny..."
if command -v xdg-mime &> /dev/null; then
  # Ustawienie dla popularnych formatów wideo
  xdg-mime default lum-player.desktop video/mp4
  xdg-mime default lum-player.desktop video/x-matroska
  xdg-mime default lum-player.desktop video/webm
  xdg-mime default lum-player.desktop video/x-msvideo
  xdg-mime default lum-player.desktop video/mpeg
  xdg-mime default lum-player.desktop video/ogg
  xdg-mime default lum-player.desktop video/quicktime
  xdg-mime default lum-player.desktop video/x-flv
  xdg-mime default lum-player.desktop video/3gpp
  xdg-mime default lum-player.desktop video/dv
  xdg-mime default lum-player.desktop video/x-ms-wmv
  xdg-mime default lum-player.desktop video/x-theora+ogg
  xdg-mime default lum-player.desktop video/x-m4v
  
  # Ustawienie dla popularnych formatów audio
  xdg-mime default lum-player.desktop audio/mpeg
  xdg-mime default lum-player.desktop audio/mp4
  xdg-mime default lum-player.desktop audio/x-flac
  xdg-mime default lum-player.desktop audio/x-wav
  xdg-mime default lum-player.desktop audio/ogg
  xdg-mime default lum-player.desktop audio/x-mp3
  xdg-mime default lum-player.desktop audio/mp3
  xdg-mime default lum-player.desktop audio/flac
  xdg-mime default lum-player.desktop audio/x-ms-wma
  xdg-mime default lum-player.desktop audio/x-aac
  xdg-mime default lum-player.desktop application/ogg
  xdg-mime default lum-player.desktop application/x-ogg
  
  # Dodaj do menu kontekstowego "Otwórz za pomocą..."
  if command -v xdg-settings &> /dev/null; then
    # Próba ustawienia jako domyślny odtwarzacz wideo (jeśli system to obsługuje)
    xdg-settings check default-video-player &> /dev/null && xdg-settings set default-video-player lum-player.desktop
    
    # Dodaj do listy preferowanych aplikacji
    echo "Dodawanie do listy preferowanych aplikacji..."
    
    # Sprawdź, czy katalog .config istnieje
    if [ ! -d "$HOME/.config" ]; then
      mkdir -p "$HOME/.config"
    fi
    
    # Utwórz lub zaktualizuj plik mimeapps.list
    if [ -f "$HOME/.config/mimeapps.list" ]; then
      # Usuń istniejące wpisy dla lum-player
      grep -v "lum-player.desktop" "$HOME/.config/mimeapps.list" > "$HOME/.config/mimeapps.list.tmp"
      mv "$HOME/.config/mimeapps.list.tmp" "$HOME/.config/mimeapps.list"
    else
      # Utwórz nowy plik
      touch "$HOME/.config/mimeapps.list"
    fi
    
    # Dodaj sekcję [Added Associations], jeśli nie istnieje
    if ! grep -q "\[Added Associations\]" "$HOME/.config/mimeapps.list"; then
      echo "[Added Associations]" >> "$HOME/.config/mimeapps.list"
    fi
    
    # Dodaj wpisy dla popularnych formatów
    sed -i '/\[Added Associations\]/a video/mp4=lum-player.desktop;' "$HOME/.config/mimeapps.list"
    sed -i '/\[Added Associations\]/a video/x-matroska=lum-player.desktop;' "$HOME/.config/mimeapps.list"
    sed -i '/\[Added Associations\]/a video/webm=lum-player.desktop;' "$HOME/.config/mimeapps.list"
    sed -i '/\[Added Associations\]/a video/x-msvideo=lum-player.desktop;' "$HOME/.config/mimeapps.list"
    sed -i '/\[Added Associations\]/a audio/mpeg=lum-player.desktop;' "$HOME/.config/mimeapps.list"
    sed -i '/\[Added Associations\]/a audio/mp3=lum-player.desktop;' "$HOME/.config/mimeapps.list"
  fi
  
  echo "Lum Player został ustawiony jako domyślny odtwarzacz dla popularnych formatów multimedialnych."
else
  echo "Narzędzie xdg-mime nie jest dostępne. Ustaw Lum Player jako domyślny odtwarzacz ręcznie w ustawieniach systemu."
fi

echo ""
echo "=== Instalacja zakończona ==="
echo "Możesz uruchomić program komendą 'lum-player'"
echo "lub znaleźć go w menu aplikacji."