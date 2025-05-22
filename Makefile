CC = gcc
PKGCONFIG = pkg-config
CFLAGS = -Wall -g `$(PKGCONFIG) --cflags gtk+-3.0 gstreamer-1.0 gstreamer-video-1.0`
LDFLAGS = `$(PKGCONFIG) --libs gtk+-3.0 gstreamer-1.0 gstreamer-video-1.0`
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share
APPNAME = lum-player
LOCALEDIR = $(DATADIR)/locale

all: $(APPNAME)

$(APPNAME): lum_player.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: $(APPNAME) install-desktop install-icon install-locale
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(APPNAME) $(DESTDIR)$(BINDIR)

# Instalacja systemowa (wymaga uprawnień administratora)
install-system:
	@echo "Instalowanie Lum Media Player do systemu..."
	@sudo make install PREFIX=/usr
	@echo "Instalacja zakończona. Możesz uruchomić program komendą 'lum-player'"

install-desktop:
	install -d $(DESTDIR)$(DATADIR)/applications
	@echo "[Desktop Entry]" > $(APPNAME).desktop
	@echo "Name=Lum Media Player" >> $(APPNAME).desktop
	@echo "GenericName=Media Player" >> $(APPNAME).desktop
	@echo "Comment=Play your media files" >> $(APPNAME).desktop
	@echo "Exec=$(BINDIR)/$(APPNAME) %f" >> $(APPNAME).desktop
	@echo "Icon=$(APPNAME)" >> $(APPNAME).desktop
	@echo "Terminal=false" >> $(APPNAME).desktop
	@echo "Type=Application" >> $(APPNAME).desktop
	@echo "Categories=AudioVideo;Player;Video;Audio;GTK;" >> $(APPNAME).desktop
	@echo "MimeType=video/x-msvideo;video/x-matroska;video/webm;video/mp4;video/mpeg;video/ogg;video/quicktime;video/x-flv;video/3gpp;video/dv;video/x-ms-wmv;video/x-theora+ogg;video/x-m4v;audio/x-vorbis+ogg;audio/x-flac;audio/mp4;audio/mpeg;audio/x-wav;audio/x-mp3;audio/mp3;audio/ogg;audio/flac;audio/x-ms-wma;audio/x-aac;application/ogg;application/x-ogg;" >> $(APPNAME).desktop
	@echo "StartupNotify=true" >> $(APPNAME).desktop
	@echo "DBusActivatable=true" >> $(APPNAME).desktop
	@echo "X-KDE-Protocols=file,http,https,mms,rtmp,rtsp,sftp,smb" >> $(APPNAME).desktop
	@echo "Actions=OpenNewWindow;" >> $(APPNAME).desktop
	@echo "" >> $(APPNAME).desktop
	@echo "[Desktop Action OpenNewWindow]" >> $(APPNAME).desktop
	@echo "Name=Otwórz w nowym oknie" >> $(APPNAME).desktop
	@echo "Exec=$(BINDIR)/$(APPNAME) %f" >> $(APPNAME).desktop
	install -m 644 $(APPNAME).desktop $(DESTDIR)$(DATADIR)/applications/

install-icon:
	install -d $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps
	install -m 644 $(APPNAME).svg $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/

install-locale:
	install -d $(DESTDIR)$(LOCALEDIR)/pl/LC_MESSAGES
	install -d $(DESTDIR)$(LOCALEDIR)/en/LC_MESSAGES
	@echo "Uwaga: Pliki tłumaczeń nie zostały utworzone. Należy je dodać ręcznie do katalogu $(LOCALEDIR)/{pl,en}/LC_MESSAGES/"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(APPNAME)
	rm -f $(DESTDIR)$(DATADIR)/applications/$(APPNAME).desktop
	rm -f $(DESTDIR)$(DATADIR)/icons/hicolor/*/apps/$(APPNAME).*

clean:
	rm -f $(APPNAME) *.o *.desktop

.PHONY: all install install-desktop install-icon install-locale uninstall clean