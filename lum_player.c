#define GDK_WINDOWING_X11

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gst/tag/tag.h>
#include <gdk/gdkx.h>
#include <string.h>
#include <dirent.h>
#include <cairo.h>
#include <ctype.h>
#include <sys/stat.h>
#include <locale.h>
#include <libintl.h>

// Definicje dla obsługi tłumaczeń
#define _(String) gettext(String)
#define GETTEXT_PACKAGE "lum-player"
#define LOCALEDIR "/usr/share/locale"

// Globalne zmienne
GstElement *playbin;
GtkWidget *video_area;
GtkWidget *seek_scale;
GtkWidget *label_time;
GtkWidget *volume_button;
GtkWidget *window;
gboolean is_seeking = FALSE;
guint update_id = 0;
gchar *current_filename = NULL;
gchar *current_folder = NULL;
gdouble duration_seconds = 0.0;
gboolean is_fullscreen = FALSE;
gboolean is_audio_only = FALSE;
GtkWidget *main_controls = NULL;
GtkWidget *main_menu_bar = NULL;
GtkWidget *play_button = NULL;
GtkWidget *pause_button = NULL;
GtkWidget *stop_button = NULL;
GtkWidget *prev_button = NULL;
GtkWidget *next_button = NULL;
GtkWidget *fullscreen_button = NULL;
GtkWidget *snapshot_button = NULL;
GtkWidget *mute_button = NULL;
GdkPixbuf *album_art = NULL;
cairo_surface_t *album_art_surface = NULL;

// Zmienne do obsługi metadanych
gchar *current_title = NULL;
gchar *current_artist = NULL;
gchar *current_album = NULL;
gboolean has_embedded_art = FALSE;
guint check_embedded_art_id = 0;
GstSample *embedded_art_sample = NULL;

// Zmienne do obsługi autoukrywania kontrolek
gboolean controls_visible = TRUE;
guint hide_controls_id = 0;
guint mouse_motion_id = 0;
gint last_mouse_y = 0;

// Zmienne do obsługi listy plików w katalogu
GList *file_list = NULL;
GList *current_file_node = NULL;

// Struktura do przechowywania tłumaczeń
typedef struct {
    const char *pl;
    const char *en;
} Translation;

// Słownik tłumaczeń
static const Translation translations[] = {
    {"Plik", "File"},
    {"Otwórz", "Open"},
    {"Wyjście", "Exit"},
    {"Odtwarzanie", "Playback"},
    {"Odtwórz", "Play"},
    {"Pauza", "Pause"},
    {"Stop", "Stop"},
    {"Poprzedni", "Previous"},
    {"Następny", "Next"},
    {"Wycisz", "Mute"},
    {"Pełny ekran", "Fullscreen"},
    {"Zrzut ekranu", "Screenshot"},
    {"Pomoc", "Help"},
    {"O programie", "About"},
    {"Otwórz plik", "Open file"},
    {"Anuluj", "Cancel"},
    {"Pliki multimedialne", "Media files"},
    {"Pliki audio", "Audio files"},
    {"Pliki wideo", "Video files"},
    {"Wszystkie pliki", "All files"},
    {"Błąd GStreamer", "GStreamer error"},
    {"Lum Media Player", "Lum Media Player"},
    {"Odtwarzacz multimedialny", "Media player"},
    {"Normalny", "Normal"}
};

// Prototypy funkcji
static void toggle_fullscreen(GtkWidget *widget, gpointer data);
static void on_open_file(GtkWidget *widget, gpointer data);
static void on_quit(GtkWidget *widget, gpointer data);
static void on_about(GtkWidget *widget, gpointer data);
static void on_play(GtkWidget *widget, gpointer data);
static void on_pause(GtkWidget *widget, gpointer data);
static void on_stop(GtkWidget *widget, gpointer data);
static void on_prev(GtkWidget *widget, gpointer data);
static void on_next(GtkWidget *widget, gpointer data);
static void on_mute(GtkWidget *widget, gpointer data);
static void on_snapshot(GtkWidget *widget, gpointer data);
static gboolean is_media_file(const gchar *filename);
static void scan_directory_for_media(const gchar *dir_path, const gchar *current_file);
static gboolean find_and_load_album_art(const gchar *folder_path);
static gboolean on_video_draw(GtkWidget *widget, cairo_t *cr, gpointer data);
static void handle_tags(const GstTagList *tags);
static gboolean check_for_embedded_art(gpointer data);
static void clear_metadata(void);
static gboolean is_audio_file(const gchar *filename);
static void show_controls(void);
static void hide_controls(void);
static gboolean auto_hide_controls(gpointer data);
static gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer data);
static gboolean on_enter_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data);
static const char* tr(const char *text);
static void init_i18n(void);

// Inicjalizacja obsługi języków
static void init_i18n(void) {
    // Ustaw lokalizację zgodnie z ustawieniami systemowymi
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
}

// Funkcja do tłumaczenia tekstów
static const char* tr(const char *text) {
    // Najpierw spróbuj użyć gettext
    const char *translated = gettext(text);
    if (translated != text) {
        return translated;
    }
    
    // Jeśli gettext nie zadziałał, użyj naszego prostego systemu tłumaczeń
    const char *lang = setlocale(LC_MESSAGES, NULL);
    gboolean is_polish = (lang && (strstr(lang, "pl") || strstr(lang, "PL")));
    
    // Przeszukaj tablicę tłumaczeń
    for (int i = 0; i < sizeof(translations) / sizeof(translations[0]); i++) {
        if (strcmp(text, translations[i].pl) == 0) {
            return is_polish ? translations[i].pl : translations[i].en;
        } else if (strcmp(text, translations[i].en) == 0) {
            return is_polish ? translations[i].pl : translations[i].en;
        }
    }
    
    // Jeśli nie znaleziono tłumaczenia, zwróć oryginalny tekst
    return text;
}

// Ustaw tytuł okna
static void set_title(GtkWindow *window, const char *filename) {
    gchar *title;
    if (filename)
        title = g_strdup_printf("Lum Media Player - %s", filename);
    else
        title = g_strdup("Lum Media Player");
    gtk_window_set_title(window, title);
    g_free(title);
}

// Formatowanie czasu (00:00:00)
static gchar* format_time(gdouble value) {
    int t = (int)value;
    int hours = t / 3600;
    int minutes = (t / 60) % 60;
    int seconds = t % 60;
    
    if (hours > 0)
        return g_strdup_printf("%02d:%02d:%02d", hours, minutes, seconds);
    else
        return g_strdup_printf("%02d:%02d", minutes, seconds);
}

static gchar* seek_format(GtkScale *scale, gdouble value, gpointer user_data) {
    return format_time(value);
}

// Aktualizacja etykiety czasu
static void update_time_label() {
    gint64 pos = 0, len = 0;
    char buf[64];
    
    if (gst_element_query_position(playbin, GST_FORMAT_TIME, &pos) &&
        gst_element_query_duration(playbin, GST_FORMAT_TIME, &len) && len > 0) {
        
        int cur = (int)(pos / GST_SECOND);
        int dur = (int)(len / GST_SECOND);
        
        // Formatuj czas
        gchar *cur_str = format_time(cur);
        gchar *dur_str = format_time(dur);
        
        // Wyświetl czas w formacie "00:00 / 00:00"
        snprintf(buf, sizeof(buf), "%s / %s", cur_str, dur_str);
        
        // Ustaw tekst etykiety z większą czcionką i pogrubieniem
        PangoAttrList *attr_list = pango_attr_list_new();
        PangoAttribute *attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
        pango_attr_list_insert(attr_list, attr);
        
        attr = pango_attr_size_new(12 * PANGO_SCALE);
        pango_attr_list_insert(attr_list, attr);
        
        gtk_label_set_attributes(GTK_LABEL(label_time), attr_list);
        gtk_label_set_text(GTK_LABEL(label_time), buf);
        
        pango_attr_list_unref(attr_list);
        g_free(cur_str);
        g_free(dur_str);
    } else {
        // Gdy nie ma odtwarzania, wyświetl "--:-- / --:--"
        PangoAttrList *attr_list = pango_attr_list_new();
        PangoAttribute *attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
        pango_attr_list_insert(attr_list, attr);
        
        attr = pango_attr_size_new(12 * PANGO_SCALE);
        pango_attr_list_insert(attr_list, attr);
        
        gtk_label_set_attributes(GTK_LABEL(label_time), attr_list);
        gtk_label_set_text(GTK_LABEL(label_time), "--:-- / --:--");
        
        pango_attr_list_unref(attr_list);
    }
}

// Aktualizacja seekbara i czasu
static void update_seekbar() {
    if (!GST_IS_ELEMENT(playbin) || is_seeking) return;
    gint64 pos = 0, len = 0;
    if (gst_element_query_position(playbin, GST_FORMAT_TIME, &pos) &&
        gst_element_query_duration(playbin, GST_FORMAT_TIME, &len) && len > 0) {
        duration_seconds = (gdouble)len / GST_SECOND;
        gtk_range_set_range(GTK_RANGE(seek_scale), 0.0, duration_seconds);
        gtk_range_set_value(GTK_RANGE(seek_scale), (gdouble)pos / GST_SECOND);
    }
    update_time_label();
}

static gboolean refresh_seekbar(gpointer data) {
    update_seekbar();
    return TRUE;
}

// Renderowanie wideo w głównym oknie (video_area)
static void on_realize(GtkWidget *widget, gpointer data) {
    GdkWindow *gdk_window = gtk_widget_get_window(video_area);
    if (!gdk_window) return;
    
    // Upewnij się, że widget jest gotowy do renderowania
    gtk_widget_set_double_buffered(video_area, FALSE);
    
    // Pobierz XID okna
    Window xid = GDK_WINDOW_XID(gdk_window);
    
    // Ustaw XID jako uchwyt okna dla GStreamer
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(playbin), xid);
}

// Otwieranie pliku
static void on_open_file(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Otwórz plik",
        GTK_WINDOW(window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Anuluj", GTK_RESPONSE_CANCEL,
        "_Otwórz", GTK_RESPONSE_ACCEPT,
        NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Pliki multimedialne");
    gtk_file_filter_add_mime_type(filter, "video/*");
    gtk_file_filter_add_mime_type(filter, "audio/*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    // Dodaj filtr dla plików audio
    GtkFileFilter *audio_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(audio_filter, "Pliki audio");
    gtk_file_filter_add_mime_type(audio_filter, "audio/*");
    gtk_file_filter_add_pattern(audio_filter, "*.mp3");
    gtk_file_filter_add_pattern(audio_filter, "*.ogg");
    gtk_file_filter_add_pattern(audio_filter, "*.flac");
    gtk_file_filter_add_pattern(audio_filter, "*.wav");
    gtk_file_filter_add_pattern(audio_filter, "*.m4a");
    gtk_file_filter_add_pattern(audio_filter, "*.aac");
    gtk_file_filter_add_pattern(audio_filter, "*.opus");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), audio_filter);

    // Dodaj filtr dla plików wideo
    GtkFileFilter *video_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(video_filter, "Pliki wideo");
    gtk_file_filter_add_mime_type(video_filter, "video/*");
    gtk_file_filter_add_pattern(video_filter, "*.mp4");
    gtk_file_filter_add_pattern(video_filter, "*.mkv");
    gtk_file_filter_add_pattern(video_filter, "*.avi");
    gtk_file_filter_add_pattern(video_filter, "*.webm");
    gtk_file_filter_add_pattern(video_filter, "*.mov");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), video_filter);

    GtkFileFilter *all_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(all_filter, "Wszystkie pliki");
    gtk_file_filter_add_pattern(all_filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), all_filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gchar *uri = g_strdup_printf("file://%s", filename);
        gchar *basename = g_path_get_basename(filename);
        
        // Zapisz folder, w którym znajduje się plik
        if (current_folder) g_free(current_folder);
        current_folder = g_path_get_dirname(filename);

        // Wyczyść poprzednie metadane i okładki
        clear_metadata();

        // Odtwórz plik
        gst_element_set_state(playbin, GST_STATE_NULL);
        g_object_set(playbin, "uri", uri, NULL);

        if (current_filename) g_free(current_filename);
        current_filename = g_strdup(basename);
        set_title(GTK_WINDOW(window), current_filename);

        // Sprawdź, czy to plik audio i przygotuj metadane
        is_audio_only = is_audio_file(basename);
        
        // Jeśli to plik audio, spróbuj od razu załadować okładkę z folderu
        if (is_audio_only) {
            find_and_load_album_art(current_folder);
        }
        
        // Skanuj katalog w poszukiwaniu innych plików multimedialnych
        scan_directory_for_media(current_folder, current_filename);

        g_free(uri);
        g_free(filename);
        g_free(basename);
        
        gst_element_set_state(playbin, GST_STATE_PLAYING);
        
        // Aktualizuj przyciski
        gtk_widget_hide(play_button);
        gtk_widget_show(pause_button);
    }
    
    gtk_widget_destroy(dialog);
}

// Nie potrzebujemy już funkcji związanych z listą odtwarzania

// Sterowanie
static void on_play(GtkWidget *widget, gpointer data) {
    GstState state;
    gst_element_get_state(playbin, &state, NULL, GST_CLOCK_TIME_NONE);
    
    if (state != GST_STATE_PLAYING) {
        gst_element_set_state(playbin, GST_STATE_PLAYING);
        gtk_widget_hide(play_button);
        gtk_widget_show(pause_button);
    }
}

static void on_pause(GtkWidget *widget, gpointer data) {
    GstState state;
    gst_element_get_state(playbin, &state, NULL, GST_CLOCK_TIME_NONE);
    
    if (state == GST_STATE_PLAYING) {
        gst_element_set_state(playbin, GST_STATE_PAUSED);
        gtk_widget_hide(pause_button);
        gtk_widget_show(play_button);
    }
}

static void on_stop(GtkWidget *widget, gpointer data) {
    gst_element_set_state(playbin, GST_STATE_NULL);
    gtk_range_set_value(GTK_RANGE(seek_scale), 0.0);
    gtk_widget_hide(pause_button);
    gtk_widget_show(play_button);
}

static void on_prev(GtkWidget *widget, gpointer data) {
    // Sprawdź, czy mamy listę plików
    if (!file_list || !current_file_node) {
        // Jeśli nie ma listy plików, przewiń do początku bieżącego pliku
        gst_element_set_state(playbin, GST_STATE_PAUSED);
        gst_element_seek_simple(playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, 0);
        gst_element_set_state(playbin, GST_STATE_PLAYING);
        return;
    }
    
    // Przejdź do poprzedniego pliku na liście
    GList *prev_node = g_list_previous(current_file_node);
    if (!prev_node) {
        // Jeśli jesteśmy na początku listy, przejdź do końca (zapętlenie)
        prev_node = g_list_last(file_list);
    }
    
    if (prev_node) {
        // Odtwórz poprzedni plik
        const gchar *filename = (const gchar *)prev_node->data;
        gchar *uri = g_filename_to_uri(filename, NULL, NULL);
        if (uri) {
            // Pobierz nazwę pliku i folder
            gchar *basename = g_path_get_basename(filename);
            gchar *dirname = g_path_get_dirname(filename);
            
            // Wyczyść poprzednie metadane i okładki
            clear_metadata();
            
            // Odtwórz plik
            gst_element_set_state(playbin, GST_STATE_NULL);
            g_object_set(playbin, "uri", uri, NULL);
            
            if (current_filename) g_free(current_filename);
            current_filename = g_strdup(basename);
            
            if (current_folder) g_free(current_folder);
            current_folder = g_strdup(dirname);
            
            set_title(GTK_WINDOW(window), current_filename);
            
            // Sprawdź, czy to plik audio i przygotuj metadane
            is_audio_only = is_audio_file(basename);
            
            // Jeśli to plik audio, spróbuj od razu załadować okładkę z folderu
            if (is_audio_only) {
                find_and_load_album_art(current_folder);
            }
            
            // Rozpocznij odtwarzanie
            gst_element_set_state(playbin, GST_STATE_PLAYING);
            
            // Aktualizuj przyciski
            gtk_widget_hide(play_button);
            gtk_widget_show(pause_button);
            
            // Aktualizuj bieżący węzeł
            current_file_node = prev_node;
            
            // Zwolnij zasoby
            g_free(uri);
            g_free(basename);
            g_free(dirname);
        }
    }
}

static void on_next(GtkWidget *widget, gpointer data) {
    // Sprawdź, czy mamy listę plików
    if (!file_list || !current_file_node) {
        // Jeśli nie ma listy plików, zatrzymaj odtwarzanie
        gst_element_set_state(playbin, GST_STATE_NULL);
        gtk_widget_hide(pause_button);
        gtk_widget_show(play_button);
        return;
    }
    
    // Przejdź do następnego pliku na liście
    GList *next_node = g_list_next(current_file_node);
    if (!next_node) {
        // Jeśli jesteśmy na końcu listy, przejdź do początku (zapętlenie)
        next_node = file_list;
    }
    
    if (next_node) {
        // Odtwórz następny plik
        const gchar *filename = (const gchar *)next_node->data;
        gchar *uri = g_filename_to_uri(filename, NULL, NULL);
        if (uri) {
            // Pobierz nazwę pliku i folder
            gchar *basename = g_path_get_basename(filename);
            gchar *dirname = g_path_get_dirname(filename);
            
            // Wyczyść poprzednie metadane i okładki
            clear_metadata();
            
            // Odtwórz plik
            gst_element_set_state(playbin, GST_STATE_NULL);
            g_object_set(playbin, "uri", uri, NULL);
            
            if (current_filename) g_free(current_filename);
            current_filename = g_strdup(basename);
            
            if (current_folder) g_free(current_folder);
            current_folder = g_strdup(dirname);
            
            set_title(GTK_WINDOW(window), current_filename);
            
            // Sprawdź, czy to plik audio i przygotuj metadane
            is_audio_only = is_audio_file(basename);
            
            // Jeśli to plik audio, spróbuj od razu załadować okładkę z folderu
            if (is_audio_only) {
                find_and_load_album_art(current_folder);
            }
            
            // Rozpocznij odtwarzanie
            gst_element_set_state(playbin, GST_STATE_PLAYING);
            
            // Aktualizuj przyciski
            gtk_widget_hide(play_button);
            gtk_widget_show(pause_button);
            
            // Aktualizuj bieżący węzeł
            current_file_node = next_node;
            
            // Zwolnij zasoby
            g_free(uri);
            g_free(basename);
            g_free(dirname);
        }
    }
}

static void on_mute(GtkWidget *widget, gpointer data) {
    gboolean muted;
    g_object_get(playbin, "mute", &muted, NULL);
    g_object_set(playbin, "mute", !muted, NULL);
    
    // Aktualizuj ikonę przycisku głośności
    if (!muted) {
        // Jeśli wyciszamy, zapamiętaj aktualną głośność i ustaw suwak na 0
        gdouble current_volume;
        g_object_get(playbin, "volume", &current_volume, NULL);
        g_object_set_data(G_OBJECT(volume_button), "saved-volume", GINT_TO_POINTER((int)(current_volume * 100)));
        gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume_button), 0.0);
    } else {
        // Jeśli włączamy dźwięk, przywróć poprzednią głośność
        int saved_volume = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(volume_button), "saved-volume"));
        if (saved_volume > 0) {
            gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume_button), saved_volume / 100.0);
        } else {
            // Jeśli nie ma zapisanej głośności, ustaw domyślną
            gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume_button), 0.5);
        }
    }
}

static void on_snapshot(GtkWidget *widget, gpointer data) {
    // Implementacja zrzutu ekranu
    GstElement *video_sink = NULL;
    g_object_get(playbin, "video-sink", &video_sink, NULL);
    
    if (video_sink) {
        // Tutaj można by zaimplementować zrzut ekranu, ale to wymaga dodatkowej logiki
        // i zależy od używanego video sink
        g_object_unref(video_sink);
        
        // Tymczasowo wyświetlamy komunikat
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_INFO,
                                                 GTK_BUTTONS_OK,
                                                 "Zrzut ekranu został zapisany");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

static void on_volume_changed(GtkScaleButton *button, gdouble value, gpointer data) {
    // Ustaw głośność
    g_object_set(playbin, "volume", value, NULL);
    
    // Jeśli głośność jest ustawiona na 0, wycisz
    if (value == 0.0) {
        g_object_set(playbin, "mute", TRUE, NULL);
    } else {
        // Jeśli głośność jest większa niż 0, upewnij się, że dźwięk nie jest wyciszony
        gboolean muted;
        g_object_get(playbin, "mute", &muted, NULL);
        if (muted) {
            g_object_set(playbin, "mute", FALSE, NULL);
        }
    }
}

static void on_seek(GtkRange *r, gpointer d) {
    if (!GST_IS_ELEMENT(playbin)) return;
    is_seeking = TRUE;
    gdouble s = gtk_range_get_value(r);
    gint64 pos = (gint64)(s * GST_SECOND);
    gst_element_seek_simple(playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, pos);
    is_seeking = FALSE;
}

// Obsługa zdarzeń seekbara
static gboolean seek_press(GtkWidget *w, GdkEventButton *e, gpointer d) { is_seeking = TRUE; return FALSE; }
static gboolean seek_release(GtkWidget *w, GdkEventButton *e, gpointer d) { is_seeking = FALSE; on_seek(GTK_RANGE(w), NULL); return FALSE; }
static gboolean seek_key_press(GtkWidget *w, GdkEventKey *e, gpointer d) { is_seeking = TRUE; return FALSE; }
static gboolean seek_key_release(GtkWidget *w, GdkEventKey *e, gpointer d) { is_seeking = FALSE; on_seek(GTK_RANGE(w), NULL); return FALSE; }

// Sprawdza, czy plik jest plikiem audio na podstawie rozszerzenia
static gboolean is_audio_file(const gchar *filename) {
    if (!filename) return FALSE;
    
    const gchar *ext = strrchr(filename, '.');
    if (!ext) return FALSE;
    
    // Przejdź do małych liter dla łatwiejszego porównania
    gchar *ext_lower = g_ascii_strdown(ext, -1);
    
    // Lista rozszerzeń plików audio
    const gchar *audio_extensions[] = {
        ".mp3", ".ogg", ".oga", ".flac", ".wav", ".wma", ".aac", 
        ".m4a", ".opus", ".aiff", ".aif", ".ape", ".wv"
    };
    
    gboolean is_audio = FALSE;
    for (int i = 0; i < G_N_ELEMENTS(audio_extensions); i++) {
        if (strcmp(ext_lower, audio_extensions[i]) == 0) {
            is_audio = TRUE;
            break;
        }
    }
    
    g_free(ext_lower);
    return is_audio;
}

// Czyści metadane
static void clear_metadata(void) {
    if (current_title) {
        g_free(current_title);
        current_title = NULL;
    }
    
    if (current_artist) {
        g_free(current_artist);
        current_artist = NULL;
    }
    
    if (current_album) {
        g_free(current_album);
        current_album = NULL;
    }
    
    if (album_art) {
        g_object_unref(album_art);
        album_art = NULL;
    }
    
    if (album_art_surface) {
        cairo_surface_destroy(album_art_surface);
        album_art_surface = NULL;
    }
    
    if (embedded_art_sample) {
        gst_sample_unref(embedded_art_sample);
        embedded_art_sample = NULL;
    }
    
    has_embedded_art = FALSE;
    
    if (check_embedded_art_id) {
        g_source_remove(check_embedded_art_id);
        check_embedded_art_id = 0;
    }
}

// Obsługuje tagi metadanych z GStreamer
static void handle_tags(const GstTagList *tags) {
    gchar *title = NULL;
    gchar *artist = NULL;
    gchar *album = NULL;
    
    // Pobierz tytuł
    if (gst_tag_list_get_string(tags, GST_TAG_TITLE, &title)) {
        if (current_title) g_free(current_title);
        current_title = title;
    }
    
    // Pobierz artystę
    if (gst_tag_list_get_string(tags, GST_TAG_ARTIST, &artist)) {
        if (current_artist) g_free(current_artist);
        current_artist = artist;
    }
    
    // Pobierz album
    if (gst_tag_list_get_string(tags, GST_TAG_ALBUM, &album)) {
        if (current_album) g_free(current_album);
        current_album = album;
    }
    
    // Sprawdź, czy są tagi obrazu (okładki)
    if (gst_tag_list_get_sample(tags, GST_TAG_IMAGE, &embedded_art_sample) ||
        gst_tag_list_get_sample(tags, GST_TAG_PREVIEW_IMAGE, &embedded_art_sample)) {
        
        has_embedded_art = TRUE;
        
        // Zwolnij poprzednią okładkę, jeśli istnieje
        if (album_art) {
            g_object_unref(album_art);
            album_art = NULL;
        }
        
        if (album_art_surface) {
            cairo_surface_destroy(album_art_surface);
            album_art_surface = NULL;
        }
        
        // Konwertuj GstSample na GdkPixbuf
        GstBuffer *buffer = gst_sample_get_buffer(embedded_art_sample);
        GstMapInfo map;
        
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
            
            if (gdk_pixbuf_loader_write(loader, map.data, map.size, NULL)) {
                gdk_pixbuf_loader_close(loader, NULL);
                album_art = gdk_pixbuf_loader_get_pixbuf(loader);
                
                if (album_art) {
                    g_object_ref(album_art); // Zwiększ licznik referencji, aby przetrwał zniszczenie loadera
                }
            }
            
            g_object_unref(loader);
            gst_buffer_unmap(buffer, &map);
        }
        
        // Wymusza odświeżenie obszaru wideo
        if (album_art) {
            gtk_widget_queue_draw(video_area);
        }
    }
    
    // Aktualizuj tytuł okna, jeśli mamy metadane
    if (current_title && current_artist) {
        gchar *window_title = g_strdup_printf("%s - %s", current_title, current_artist);
        gtk_window_set_title(GTK_WINDOW(window), window_title);
        g_free(window_title);
    } else if (current_title) {
        gtk_window_set_title(GTK_WINDOW(window), current_title);
    } else if (current_filename) {
        set_title(GTK_WINDOW(window), current_filename);
    }
}

// Funkcja sprawdzająca, czy są dostępne metadane z okładką
static gboolean check_for_embedded_art(gpointer data) {
    if (!is_audio_only || !playbin) {
        check_embedded_art_id = 0;
        return G_SOURCE_REMOVE;
    }
    
    // Pobierz tagi
    GstTagList *tags = NULL;
    g_object_get(playbin, "current-audio-tags", &tags, NULL);
    
    if (tags) {
        handle_tags(tags);
        gst_tag_list_unref(tags);
        
        // Jeśli znaleziono okładkę, zatrzymaj timer
        if (has_embedded_art && album_art) {
            check_embedded_art_id = 0;
            return G_SOURCE_REMOVE;
        }
    }
    
    // Kontynuuj sprawdzanie, jeśli nie znaleziono okładki
    return G_SOURCE_CONTINUE;
}

// Funkcja wyszukująca i ładująca okładkę albumu z folderu
static gboolean find_and_load_album_art(const gchar *folder_path) {
    // Jeśli już mamy okładkę z metadanych, nie szukaj w folderze
    if (has_embedded_art && album_art) return TRUE;
    
    // Zwolnij poprzednią okładkę, jeśli istnieje
    if (album_art) {
        g_object_unref(album_art);
        album_art = NULL;
    }
    
    if (album_art_surface) {
        cairo_surface_destroy(album_art_surface);
        album_art_surface = NULL;
    }
    
    if (!folder_path) return FALSE;
    
    // Otwórz katalog
    DIR *dir = opendir(folder_path);
    if (!dir) return FALSE;
    
    // Nazwy plików, które mogą być okładkami
    const char *cover_names[] = {
        "cover.jpg", "cover.jpeg", "cover.png", 
        "folder.jpg", "folder.jpeg", "folder.png",
        "album.jpg", "album.jpeg", "album.png",
        "front.jpg", "front.jpeg", "front.png",
        "albumart.jpg", "albumart.jpeg", "albumart.png",
        "art.jpg", "art.jpeg", "art.png",
        "cd.jpg", "cd.jpeg", "cd.png",
        "thumb.jpg", "thumb.jpeg", "thumb.png",
        "artwork.jpg", "artwork.jpeg", "artwork.png"
    };
    
    // Liczba nazw do sprawdzenia
    const int num_names = sizeof(cover_names) / sizeof(cover_names[0]);
    
    // Sprawdź, czy któryś z plików istnieje
    struct dirent *entry;
    gchar *cover_path = NULL;
    
    while ((entry = readdir(dir)) != NULL) {
        // Sprawdź, czy nazwa pliku pasuje do którejś z typowych nazw okładek
        for (int i = 0; i < num_names; i++) {
            if (g_ascii_strcasecmp(entry->d_name, cover_names[i]) == 0) {
                cover_path = g_build_filename(folder_path, entry->d_name, NULL);
                break;
            }
        }
        
        // Jeśli znaleziono okładkę, przerwij pętlę
        if (cover_path) break;
        
        // Sprawdź, czy plik ma rozszerzenie obrazu
        const char *ext = strrchr(entry->d_name, '.');
        if (ext && (g_ascii_strcasecmp(ext, ".jpg") == 0 || 
                   g_ascii_strcasecmp(ext, ".jpeg") == 0 || 
                   g_ascii_strcasecmp(ext, ".png") == 0 ||
                   g_ascii_strcasecmp(ext, ".gif") == 0 ||
                   g_ascii_strcasecmp(ext, ".bmp") == 0)) {
            
            // Sprawdź, czy nazwa zawiera słowa sugerujące, że to okładka
            gchar *name_lower = g_ascii_strdown(entry->d_name, -1);
            if (strstr(name_lower, "cover") || 
                strstr(name_lower, "album") || 
                strstr(name_lower, "front") || 
                strstr(name_lower, "art") ||
                strstr(name_lower, "cd") ||
                strstr(name_lower, "folder") ||
                strstr(name_lower, "thumb") ||
                strstr(name_lower, "artwork")) {
                
                cover_path = g_build_filename(folder_path, entry->d_name, NULL);
                g_free(name_lower);
                break;
            }
            g_free(name_lower);
        }
    }
    
    closedir(dir);
    
    // Jeśli znaleziono okładkę, załaduj ją
    if (cover_path) {
        GError *error = NULL;
        album_art = gdk_pixbuf_new_from_file(cover_path, &error);
        
        if (!album_art && error) {
            g_warning("Nie można załadować okładki: %s", error->message);
            g_error_free(error);
        }
        
        g_free(cover_path);
        
        // Jeśli udało się załadować okładkę, odśwież obszar wideo
        if (album_art) {
            gtk_widget_queue_draw(video_area);
            return TRUE;
        }
    }
    
    // Jeśli nie znaleziono konkretnej okładki, spróbuj znaleźć dowolny obraz w folderze
    if (!album_art) {
        DIR *dir = opendir(folder_path);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                const char *ext = strrchr(entry->d_name, '.');
                if (ext && (g_ascii_strcasecmp(ext, ".jpg") == 0 || 
                           g_ascii_strcasecmp(ext, ".jpeg") == 0 || 
                           g_ascii_strcasecmp(ext, ".png") == 0)) {
                    
                    // Sprawdź rozmiar pliku - okładki zwykle mają sensowny rozmiar
                    gchar *img_path = g_build_filename(folder_path, entry->d_name, NULL);
                    struct stat st;
                    if (stat(img_path, &st) == 0) {
                        // Jeśli plik jest większy niż 10KB i mniejszy niż 5MB, to może być okładka
                        if (st.st_size > 10240 && st.st_size < 5242880) {
                            GError *error = NULL;
                            album_art = gdk_pixbuf_new_from_file(img_path, &error);
                            
                            if (album_art) {
                                // Sprawdź wymiary - okładki zwykle są kwadratowe lub prawie kwadratowe
                                int width = gdk_pixbuf_get_width(album_art);
                                int height = gdk_pixbuf_get_height(album_art);
                                double ratio = (double)width / height;
                                
                                // Jeśli proporcje są bliskie 1:1, to prawdopodobnie jest to okładka
                                if (ratio >= 0.8 && ratio <= 1.2) {
                                    g_free(img_path);
                                    closedir(dir);
                                    gtk_widget_queue_draw(video_area);
                                    return TRUE;
                                }
                                
                                // Jeśli to nie jest okładka, zwolnij zasoby
                                g_object_unref(album_art);
                                album_art = NULL;
                            }
                            
                            if (error) g_error_free(error);
                        }
                    }
                    g_free(img_path);
                }
            }
            closedir(dir);
        }
    }
    
    return (album_art != NULL);
}

// Funkcja rysująca okładkę albumu w obszarze wideo
static gboolean on_video_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
    // Jeśli nie odtwarzamy pliku audio lub nie mamy okładki, narysuj tylko tło
    if (!is_audio_only) return FALSE;
    
    // Pobierz wymiary obszaru rysowania
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    
    // Wypełnij tło gradientem
    cairo_pattern_t *pattern = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgb(pattern, 0, 0.1, 0.1, 0.15);
    cairo_pattern_add_color_stop_rgb(pattern, 1, 0.05, 0.05, 0.1);
    cairo_set_source(cr, pattern);
    cairo_paint(cr);
    cairo_pattern_destroy(pattern);
    
    // Jeśli nie mamy okładki, wyświetl informacje o utworze
    if (!album_art) {
        // Wyświetl informacje o utworze, jeśli są dostępne
        if (current_title || current_artist || current_album) {
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 24);
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            
            int y_pos = height / 3;
            
            if (current_title) {
                cairo_text_extents_t extents;
                cairo_text_extents(cr, current_title, &extents);
                cairo_move_to(cr, (width - extents.width) / 2, y_pos);
                cairo_show_text(cr, current_title);
                y_pos += 40;
            }
            
            if (current_artist) {
                cairo_set_font_size(cr, 18);
                cairo_text_extents_t extents;
                cairo_text_extents(cr, current_artist, &extents);
                cairo_move_to(cr, (width - extents.width) / 2, y_pos);
                cairo_show_text(cr, current_artist);
                y_pos += 30;
            }
            
            if (current_album) {
                cairo_set_font_size(cr, 16);
                cairo_text_extents_t extents;
                cairo_text_extents(cr, current_album, &extents);
                cairo_move_to(cr, (width - extents.width) / 2, y_pos);
                cairo_show_text(cr, current_album);
            }
        } else if (current_filename) {
            // Jeśli nie ma metadanych, wyświetl nazwę pliku
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 20);
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            
            cairo_text_extents_t extents;
            cairo_text_extents(cr, current_filename, &extents);
            cairo_move_to(cr, (width - extents.width) / 2, height / 2);
            cairo_show_text(cr, current_filename);
        }
        
        return TRUE;
    }
    
    // Oblicz wymiary okładki, zachowując proporcje
    int img_width = gdk_pixbuf_get_width(album_art);
    int img_height = gdk_pixbuf_get_height(album_art);
    
    double scale_x = (double)width * 0.8 / img_width;  // Użyj 80% szerokości
    double scale_y = (double)height * 0.8 / img_height; // Użyj 80% wysokości
    double scale = MIN(scale_x, scale_y);
    
    int new_width = img_width * scale;
    int new_height = img_height * scale;
    
    // Wyśrodkuj okładkę
    int x = (width - new_width) / 2;
    int y = (height - new_height) / 2;
    
    // Dodaj cień pod okładką
    cairo_save(cr);
    cairo_rectangle(cr, x + 10, y + 10, new_width, new_height);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    cairo_fill(cr);
    cairo_restore(cr);
    
    // Dodaj ramkę wokół okładki
    cairo_save(cr);
    cairo_rectangle(cr, x - 2, y - 2, new_width + 4, new_height + 4);
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_fill(cr);
    cairo_restore(cr);
    
    // Skaluj i rysuj okładkę
    cairo_save(cr);
    
    // Utwórz powierzchnię Cairo z pixbuf, jeśli jeszcze nie istnieje
    if (!album_art_surface) {
        album_art_surface = gdk_cairo_surface_create_from_pixbuf(album_art, 1, NULL);
    }
    
    // Rysuj okładkę
    cairo_rectangle(cr, x, y, new_width, new_height);
    cairo_clip(cr);
    
    cairo_translate(cr, x, y);
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, album_art_surface, 0, 0);
    cairo_paint(cr);
    
    cairo_restore(cr);
    
    // Wyświetl informacje o utworze pod okładką, jeśli są dostępne
    if (current_title || current_artist) {
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 16);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        
        int y_pos = y + new_height + 30;
        
        if (current_title) {
            cairo_text_extents_t extents;
            cairo_text_extents(cr, current_title, &extents);
            cairo_move_to(cr, (width - extents.width) / 2, y_pos);
            cairo_show_text(cr, current_title);
            y_pos += 25;
        }
        
        if (current_artist) {
            cairo_set_font_size(cr, 14);
            cairo_text_extents_t extents;
            cairo_text_extents(cr, current_artist, &extents);
            cairo_move_to(cr, (width - extents.width) / 2, y_pos);
            cairo_show_text(cr, current_artist);
        }
    }
    
    return TRUE;
}

// Funkcja do ekstrakcji metadanych została usunięta, ponieważ jej funkcjonalność
// jest teraz zintegrowana bezpośrednio w funkcjach otwierających pliki

// Obsługa komunikatów GStreamer
static gboolean on_bus(GstBus *bus, GstMessage *msg, gpointer data) {
    GtkWindow *window = GTK_WINDOW(data);
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            // Koniec odtwarzania - zatrzymaj i przewiń do początku
            gst_element_set_state(playbin, GST_STATE_NULL);
            gst_element_seek_simple(playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, 0);
            gtk_widget_hide(pause_button);
            gtk_widget_show(play_button);
            break;
        case GST_MESSAGE_ERROR: {
            GError *err = NULL;
            gchar *dbg = NULL;
            gst_message_parse_error(msg, &err, &dbg);
            GtkWidget *dialog = gtk_message_dialog_new(window, GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Błąd GStreamer: %s", err->message);
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            g_error_free(err);
            g_free(dbg);
            break;
        }
        case GST_MESSAGE_TAG: {
            // Obsługa tagów (metadanych)
            GstTagList *tags = NULL;
            gst_message_parse_tag(msg, &tags);
            
            if (tags) {
                handle_tags(tags);
                gst_tag_list_unref(tags);
            }
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            // Sprawdź, czy to zmiana stanu playbin
            GstObject *src = GST_MESSAGE_SRC(msg);
            if (GST_IS_ELEMENT(src) && src == GST_OBJECT(playbin)) {
                GstState old_state, new_state, pending;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending);
                
                // Jeśli przeszliśmy do stanu PLAYING, sprawdź typ strumienia
                if (new_state == GST_STATE_PLAYING) {
                    gint n_video = 0;
                    g_object_get(playbin, "n-video", &n_video, NULL);
                    
                    // Jeśli nie ma strumienia wideo, to jest to plik audio
                    is_audio_only = (n_video == 0);
                    
                    // Jeśli to plik audio, spróbuj załadować okładkę
                    if (is_audio_only) {
                        // Jeśli nie mamy jeszcze okładki, spróbuj załadować z folderu
                        if (!album_art && current_folder) {
                            find_and_load_album_art(current_folder);
                        }
                        
                        // Wymusza odświeżenie obszaru wideo
                        gtk_widget_queue_draw(video_area);
                        
                        // Ustaw timer do sprawdzania metadanych (okładki wbudowanej)
                        if (!check_embedded_art_id) {
                            check_embedded_art_id = g_timeout_add(500, check_for_embedded_art, NULL);
                        }
                    }
                }
            }
            break;
        }
        default:
            break;
    }
    return TRUE;
}

// Funkcje do obsługi ukrywania i pokazywania kontrolek
static void show_controls(void) {
    if (!controls_visible) {
        gtk_widget_show(main_menu_bar);
        gtk_widget_show(main_controls);
        controls_visible = TRUE;
    }
    
    // Anuluj istniejący timer ukrywania
    if (hide_controls_id) {
        g_source_remove(hide_controls_id);
        hide_controls_id = 0;
    }
    
    // Ustaw nowy timer ukrywania (tylko w trybie pełnoekranowym)
    if (is_fullscreen) {
        hide_controls_id = g_timeout_add(3000, auto_hide_controls, NULL);
    }
}

static void hide_controls(void) {
    if (controls_visible && is_fullscreen) {
        gtk_widget_hide(main_menu_bar);
        gtk_widget_hide(main_controls);
        controls_visible = FALSE;
    }
    
    // Anuluj timer ukrywania
    if (hide_controls_id) {
        g_source_remove(hide_controls_id);
        hide_controls_id = 0;
    }
}

static gboolean auto_hide_controls(gpointer data) {
    hide_controls();
    return G_SOURCE_REMOVE;
}

// Obsługa ruchu myszy
static gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    if (!is_fullscreen) return FALSE;
    
    // Pobierz wymiary obszaru wideo
    int height = gtk_widget_get_allocated_height(widget);
    
    // Pokaż kontrolki, gdy kursor jest blisko górnej lub dolnej krawędzi
    if (event->y < 50 || event->y > height - 50) {
        show_controls();
    } else {
        // Jeśli kursor jest w środkowej części ekranu, ukryj kontrolki po krótkim czasie
        if (controls_visible && !hide_controls_id) {
            hide_controls_id = g_timeout_add(1000, auto_hide_controls, NULL);
        }
    }
    
    // Zapisz pozycję Y myszy
    last_mouse_y = event->y;
    
    return FALSE;
}

// Obsługa wejścia kursora w obszar menu lub kontrolek
static gboolean on_enter_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    if (!is_fullscreen) return FALSE;
    
    // Sprawdź, czy to menu lub pasek kontrolny
    if (widget == main_menu_bar || widget == main_controls) {
        show_controls();
        
        // Anuluj timer ukrywania, gdy kursor jest nad kontrolkami
        if (hide_controls_id) {
            g_source_remove(hide_controls_id);
            hide_controls_id = 0;
        }
    }
    
    return FALSE;
}

// Przełączanie trybu pełnoekranowego
static void toggle_fullscreen(GtkWidget *widget, gpointer data) {
    is_fullscreen = !is_fullscreen;
    
    if (is_fullscreen) {
        // Przejdź do trybu pełnoekranowego
        gtk_window_fullscreen(GTK_WINDOW(window));
        gtk_button_set_label(GTK_BUTTON(fullscreen_button), "🔳");
        
        // Pokaż kontrolki na chwilę, a potem je ukryj
        show_controls();
        
        // Ustaw timer ukrywania kontrolek - ukryj po 2 sekundach
        if (!hide_controls_id) {
            hide_controls_id = g_timeout_add(2000, auto_hide_controls, NULL);
        }
    } else {
        // Wyjdź z trybu pełnoekranowego
        gtk_window_unfullscreen(GTK_WINDOW(window));
        gtk_button_set_label(GTK_BUTTON(fullscreen_button), "🔲");
        
        // Anuluj timer ukrywania
        if (hide_controls_id) {
            g_source_remove(hide_controls_id);
            hide_controls_id = 0;
        }
        
        // Upewnij się, że kontrolki są widoczne w trybie normalnym
        controls_visible = FALSE; // Wymuszamy aktualizację
        show_controls();
    }
}

// Obsługa klawiatury
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    switch (event->keyval) {
        case GDK_KEY_f:
        case GDK_KEY_F:
            toggle_fullscreen(NULL, NULL);
            return TRUE;
        case GDK_KEY_space:
            {
                GstState state;
                gst_element_get_state(playbin, &state, NULL, GST_CLOCK_TIME_NONE);
                if (state == GST_STATE_PLAYING)
                    on_pause(NULL, NULL);
                else
                    on_play(NULL, NULL);
            }
            return TRUE;
        case GDK_KEY_Escape:
            if (is_fullscreen) {
                toggle_fullscreen(NULL, NULL);
                return TRUE;
            }
            break;
        case GDK_KEY_m:
        case GDK_KEY_M:
            on_mute(NULL, NULL);
            return TRUE;
        case GDK_KEY_s:
        case GDK_KEY_S:
            on_stop(NULL, NULL);
            return TRUE;
        case GDK_KEY_Left:
            {
                gint64 pos = 0;
                if (gst_element_query_position(playbin, GST_FORMAT_TIME, &pos)) {
                    pos -= 10 * GST_SECOND; // Przewiń 10 sekund wstecz
                    if (pos < 0) pos = 0;
                    gst_element_seek_simple(playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, pos);
                }
            }
            return TRUE;
        case GDK_KEY_Right:
            {
                gint64 pos = 0, len = 0;
                if (gst_element_query_position(playbin, GST_FORMAT_TIME, &pos) &&
                    gst_element_query_duration(playbin, GST_FORMAT_TIME, &len)) {
                    pos += 10 * GST_SECOND; // Przewiń 10 sekund do przodu
                    if (pos > len) pos = len;
                    gst_element_seek_simple(playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, pos);
                }
            }
            return TRUE;
        case GDK_KEY_Up:
            {
                gdouble volume;
                g_object_get(playbin, "volume", &volume, NULL);
                volume += 0.1; // Zwiększ głośność o 10%
                if (volume > 1.0) volume = 1.0;
                g_object_set(playbin, "volume", volume, NULL);
                gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume_button), volume);
            }
            return TRUE;
        case GDK_KEY_Down:
            {
                gdouble volume;
                g_object_get(playbin, "volume", &volume, NULL);
                volume -= 0.1; // Zmniejsz głośność o 10%
                if (volume < 0.0) volume = 0.0;
                g_object_set(playbin, "volume", volume, NULL);
                gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume_button), volume);
            }
            return TRUE;
    }
    
    return FALSE;
}

// Funkcja tworząca menu
static GtkWidget* create_menu_bar() {
    GtkWidget *menu_bar = gtk_menu_bar_new();
    
    // Menu Plik
    GtkWidget *media_menu = gtk_menu_new();
    GtkWidget *media_item = gtk_menu_item_new_with_label(tr("Plik"));
    
    GtkWidget *open_file_item = gtk_menu_item_new_with_label(tr("Otwórz"));
    GtkWidget *separator1 = gtk_separator_menu_item_new();
    GtkWidget *quit_item = gtk_menu_item_new_with_label(tr("Wyjście"));
    
    gtk_menu_shell_append(GTK_MENU_SHELL(media_menu), open_file_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(media_menu), separator1);
    gtk_menu_shell_append(GTK_MENU_SHELL(media_menu), quit_item);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(media_item), media_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), media_item);
    
    // Menu Odtwarzanie
    GtkWidget *playback_menu = gtk_menu_new();
    GtkWidget *playback_item = gtk_menu_item_new_with_label(tr("Odtwarzanie"));
    
    GtkWidget *play_item = gtk_menu_item_new_with_label(tr("Odtwórz"));
    GtkWidget *pause_item = gtk_menu_item_new_with_label(tr("Pauza"));
    GtkWidget *stop_item = gtk_menu_item_new_with_label(tr("Stop"));
    GtkWidget *separator2 = gtk_separator_menu_item_new();
    GtkWidget *prev_item = gtk_menu_item_new_with_label(tr("Poprzedni"));
    GtkWidget *next_item = gtk_menu_item_new_with_label(tr("Następny"));
    
    gtk_menu_shell_append(GTK_MENU_SHELL(playback_menu), play_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(playback_menu), pause_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(playback_menu), stop_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(playback_menu), separator2);
    gtk_menu_shell_append(GTK_MENU_SHELL(playback_menu), prev_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(playback_menu), next_item);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(playback_item), playback_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), playback_item);
    
    // Menu Audio
    GtkWidget *audio_menu = gtk_menu_new();
    GtkWidget *audio_item = gtk_menu_item_new_with_label(tr("Audio"));
    
    GtkWidget *mute_item = gtk_menu_item_new_with_label(tr("Wycisz"));
    
    gtk_menu_shell_append(GTK_MENU_SHELL(audio_menu), mute_item);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(audio_item), audio_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), audio_item);
    
    // Menu Wideo
    GtkWidget *video_menu = gtk_menu_new();
    GtkWidget *video_item = gtk_menu_item_new_with_label(tr("Wideo"));
    
    GtkWidget *fullscreen_item = gtk_menu_item_new_with_label(tr("Pełny ekran"));
    GtkWidget *snapshot_item = gtk_menu_item_new_with_label(tr("Zrzut ekranu"));
    
    gtk_menu_shell_append(GTK_MENU_SHELL(video_menu), fullscreen_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(video_menu), snapshot_item);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(video_item), video_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), video_item);
    
    // Menu Pomoc
    GtkWidget *help_menu = gtk_menu_new();
    GtkWidget *help_item = gtk_menu_item_new_with_label("Pomoc");
    
    GtkWidget *about_item = gtk_menu_item_new_with_label("O programie");
    
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_item);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), help_item);
    
    // Podłączenie sygnałów
    g_signal_connect(G_OBJECT(open_file_item), "activate", G_CALLBACK(on_open_file), NULL);
    g_signal_connect(G_OBJECT(quit_item), "activate", G_CALLBACK(on_quit), NULL);
    
    g_signal_connect(G_OBJECT(play_item), "activate", G_CALLBACK(on_play), NULL);
    g_signal_connect(G_OBJECT(pause_item), "activate", G_CALLBACK(on_pause), NULL);
    g_signal_connect(G_OBJECT(stop_item), "activate", G_CALLBACK(on_stop), NULL);
    g_signal_connect(G_OBJECT(prev_item), "activate", G_CALLBACK(on_prev), NULL);
    g_signal_connect(G_OBJECT(next_item), "activate", G_CALLBACK(on_next), NULL);
    
    g_signal_connect(G_OBJECT(mute_item), "activate", G_CALLBACK(on_mute), NULL);
    
    g_signal_connect(G_OBJECT(fullscreen_item), "activate", G_CALLBACK(toggle_fullscreen), NULL);
    g_signal_connect(G_OBJECT(snapshot_item), "activate", G_CALLBACK(on_snapshot), NULL);
    
    g_signal_connect(G_OBJECT(about_item), "activate", G_CALLBACK(on_about), NULL);
    
    return menu_bar;
}

// Funkcja tworząca pasek narzędzi
static GtkWidget* create_toolbar() {
    // Główny kontener dla paska kontrolnego
    GtkWidget *control_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    // Pasek postępu (seekbar) - na górze paska kontrolnego
    GtkWidget *seekbar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(control_box), seekbar_box, FALSE, FALSE, 0);
    
    // Dodaj seekbar
    seek_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 1.0);
    gtk_range_set_value(GTK_RANGE(seek_scale), 0.0);
    gtk_scale_set_draw_value(GTK_SCALE(seek_scale), FALSE); // Bez wyświetlania wartości na suwaku
    gtk_widget_set_margin_start(seek_scale, 5);
    gtk_widget_set_margin_end(seek_scale, 5);
    
    // Ustaw wysokość paska postępu
    gtk_widget_set_size_request(seek_scale, -1, 15);
    
    // Dodaj styl CSS dla paska postępu
    GtkCssProvider *seekbar_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(seekbar_provider,
        "scale trough { min-height: 10px; }"
        "scale trough highlight { background-color: #f48b00; min-height: 10px; }", -1, NULL);
    
    GtkStyleContext *seekbar_context = gtk_widget_get_style_context(seek_scale);
    gtk_style_context_add_provider(seekbar_context, 
                                  GTK_STYLE_PROVIDER(seekbar_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    g_object_unref(seekbar_provider);
    
    // Podłącz sygnały
    g_signal_connect(seek_scale, "format-value", G_CALLBACK(seek_format), NULL);
    g_signal_connect(seek_scale, "button-press-event", G_CALLBACK(seek_press), NULL);
    g_signal_connect(seek_scale, "button-release-event", G_CALLBACK(seek_release), NULL);
    g_signal_connect(seek_scale, "key-press-event", G_CALLBACK(seek_key_press), NULL);
    g_signal_connect(seek_scale, "key-release-event", G_CALLBACK(seek_key_release), NULL);
    
    gtk_box_pack_start(GTK_BOX(seekbar_box), seek_scale, TRUE, TRUE, 0);
    
    // Pasek przycisków - na dole paska kontrolnego
    GtkWidget *buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(control_box), buttons_box, FALSE, FALSE, 0);
    
    // Przyciski odtwarzania
    play_button = gtk_button_new_with_label("▶");
    pause_button = gtk_button_new_with_label("⏸");
    stop_button = gtk_button_new_with_label("⏹");
    prev_button = gtk_button_new_with_label("⏮");
    next_button = gtk_button_new_with_label("⏭");
    
    // Przyciski dodatkowe
    fullscreen_button = gtk_button_new_with_label("🔲");
    snapshot_button = gtk_button_new_with_label("📷");
    
    // Przycisk głośności z obsługą wyciszania
    volume_button = gtk_volume_button_new();
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume_button), 0.5);
    // Dodaj obsługę kliknięcia na ikonę głośności (wyciszanie)
    GtkWidget *button = gtk_scale_button_get_popup(GTK_SCALE_BUTTON(volume_button));
    g_signal_connect(button, "button-press-event", G_CALLBACK(on_mute), NULL);
    
    // Etykieta czasu
    label_time = gtk_label_new("--:-- / --:--");
    gtk_widget_set_margin_start(label_time, 10);
    gtk_widget_set_margin_end(label_time, 10);
    
    // Ustaw styl etykiety czasu
    PangoAttrList *attr_list = pango_attr_list_new();
    PangoAttribute *attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    pango_attr_list_insert(attr_list, attr);
    
    attr = pango_attr_size_new(12 * PANGO_SCALE);
    pango_attr_list_insert(attr_list, attr);
    
    gtk_label_set_attributes(GTK_LABEL(label_time), attr_list);
    pango_attr_list_unref(attr_list);
    
    // Dodaj przyciski do paska
    gtk_box_pack_start(GTK_BOX(buttons_box), play_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons_box), pause_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons_box), stop_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons_box), prev_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons_box), next_button, FALSE, FALSE, 0);
    
    // Ukryj przycisk pauzy na początku
    gtk_widget_hide(pause_button);
    
    // Dodaj etykietę czasu po środku
    GtkWidget *spacer1 = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(buttons_box), spacer1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(buttons_box), label_time, FALSE, FALSE, 0);
    GtkWidget *spacer2 = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(buttons_box), spacer2, TRUE, TRUE, 0);
    
    // Dodaj przyciski kontrolne po prawej stronie
    gtk_box_pack_end(GTK_BOX(buttons_box), fullscreen_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(buttons_box), snapshot_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(buttons_box), volume_button, FALSE, FALSE, 0);
    
    // Stylizacja przycisków - większe i bardziej widoczne
    const gchar *button_css = 
        "button { padding: 5px; margin: 2px; }"
        "button label { font-size: 14px; }";
    
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, button_css, -1, NULL);
    
    GtkStyleContext *context;
    context = gtk_widget_get_style_context(play_button);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    context = gtk_widget_get_style_context(pause_button);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    context = gtk_widget_get_style_context(stop_button);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    context = gtk_widget_get_style_context(prev_button);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    context = gtk_widget_get_style_context(next_button);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    context = gtk_widget_get_style_context(fullscreen_button);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    context = gtk_widget_get_style_context(snapshot_button);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    context = gtk_widget_get_style_context(mute_button);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    g_object_unref(provider);
    
    // Podłącz sygnały
    g_signal_connect(G_OBJECT(play_button), "clicked", G_CALLBACK(on_play), NULL);
    g_signal_connect(G_OBJECT(pause_button), "clicked", G_CALLBACK(on_pause), NULL);
    g_signal_connect(G_OBJECT(stop_button), "clicked", G_CALLBACK(on_stop), NULL);
    g_signal_connect(G_OBJECT(prev_button), "clicked", G_CALLBACK(on_prev), NULL);
    g_signal_connect(G_OBJECT(next_button), "clicked", G_CALLBACK(on_next), NULL);
    g_signal_connect(G_OBJECT(fullscreen_button), "clicked", G_CALLBACK(toggle_fullscreen), NULL);
    g_signal_connect(G_OBJECT(snapshot_button), "clicked", G_CALLBACK(on_snapshot), NULL);
    g_signal_connect(G_OBJECT(volume_button), "value-changed", G_CALLBACK(on_volume_changed), NULL);
    
    return control_box;
}

// Nie potrzebujemy już funkcji tworzącej listę odtwarzania

// Funkcja zamykająca aplikację
static void on_quit(GtkWidget *widget, gpointer data) {
    // Zatrzymaj timery
    if (update_id) {
        g_source_remove(update_id);
        update_id = 0;
    }
    
    if (check_embedded_art_id) {
        g_source_remove(check_embedded_art_id);
        check_embedded_art_id = 0;
    }
    
    if (hide_controls_id) {
        g_source_remove(hide_controls_id);
        hide_controls_id = 0;
    }
    
    if (mouse_motion_id) {
        g_source_remove(mouse_motion_id);
        mouse_motion_id = 0;
    }
    
    // Wyczyść metadane i zwolnij zasoby
    clear_metadata();
    
    if (current_filename) {
        g_free(current_filename);
        current_filename = NULL;
    }
    
    if (current_folder) {
        g_free(current_folder);
        current_folder = NULL;
    }
    
    // Zatrzymaj odtwarzanie
    if (playbin) {
        gst_element_set_state(playbin, GST_STATE_NULL);
        gst_object_unref(playbin);
        playbin = NULL;
    }
    
    gtk_main_quit();
}

// Funkcja wyświetlająca okno "O programie"
static void on_about(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_about_dialog_new();
    
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "Lum Media Player");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.0");
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), "© 2024 Lum Team");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
                                tr("Odtwarzacz multimedialny"));
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

int main(int argc, char *argv[]) {
    // Inicjalizacja obsługi języków
    init_i18n();
    
    gtk_init(&argc, &argv);
    gst_init(&argc, &argv);
    
    // Zapisz ścieżkę do pliku, jeśli została podana jako argument
    gchar *file_to_open = NULL;
    if (argc > 1) {
        file_to_open = g_strdup(argv[1]);
        // Sprawdź, czy ścieżka jest poprawna
        if (!g_file_test(file_to_open, G_FILE_TEST_EXISTS)) {
            g_warning("Plik nie istnieje: %s", file_to_open);
            g_free(file_to_open);
            file_to_open = NULL;
        }
    }

    // Inicjalizacja GStreamer
    playbin = gst_element_factory_make("playbin", "playbin");
    if (!playbin) {
        g_error("Nie można utworzyć elementu playbin.");
    }

    // Użyj domyślnego video sink, który obsługuje video overlay
    // Nie używamy gtksink, ponieważ powoduje problemy z właściwością 'widget'

    // Ustawienie początkowej głośności
    g_object_set(playbin, "volume", 0.5, NULL);

    // Tworzenie głównego okna
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);
    set_title(GTK_WINDOW(window), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);

    // Główny kontener
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);

    // Dodaj menu na górze
    main_menu_bar = create_menu_bar();
    gtk_box_pack_start(GTK_BOX(main_vbox), main_menu_bar, FALSE, FALSE, 0);

    // Obszar wideo - główny element interfejsu, zajmuje całą przestrzeń
    video_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(video_area, 800, 480);
    
    // Upewnij się, że widget może otrzymywać zdarzenia myszy
    gtk_widget_set_events(video_area, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
                         GDK_POINTER_MOTION_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    
    // Dodaj obszar wideo bezpośrednio do głównego kontenera
    gtk_box_pack_start(GTK_BOX(main_vbox), video_area, TRUE, TRUE, 0);
    
    // Podłącz sygnał realize, który zostanie wywołany, gdy widget będzie gotowy do renderowania
    g_signal_connect(video_area, "realize", G_CALLBACK(on_realize), NULL);
    
    // Podłącz sygnał draw, aby obsłużyć rysowanie okładki albumu
    g_signal_connect(video_area, "draw", G_CALLBACK(on_video_draw), NULL);
    
    // Podłącz sygnały do obsługi zdarzeń myszy
    g_signal_connect(video_area, "motion-notify-event", G_CALLBACK(on_motion_notify), NULL);
    g_signal_connect(video_area, "enter-notify-event", G_CALLBACK(on_enter_notify), NULL);

    // Dodaj pasek kontrolny na dole
    main_controls = create_toolbar();
    gtk_box_pack_end(GTK_BOX(main_vbox), main_controls, FALSE, FALSE, 2);
    
    // Podłącz sygnały do obsługi zdarzeń myszy dla paska kontrolnego
    g_signal_connect(main_controls, "enter-notify-event", G_CALLBACK(on_enter_notify), NULL);
    
    // Podłącz sygnały do obsługi zdarzeń myszy dla menu
    g_signal_connect(main_menu_bar, "enter-notify-event", G_CALLBACK(on_enter_notify), NULL);

    // Nie używamy paska statusu, aby interfejs był czystszy

    // Obsługa komunikatów GStreamer
    GstBus *bus = gst_element_get_bus(playbin);
    gst_bus_add_watch(bus, (GstBusFunc)on_bus, window);
    gst_object_unref(bus);

    // Timer do aktualizacji seekbara
    update_id = g_timeout_add(200, refresh_seekbar, NULL);

    // Wyświetl wszystkie widgety
    gtk_widget_show_all(window);
    
    // Jeśli podano plik jako argument, otwórz go
    if (file_to_open) {
        // Utwórz URI z ścieżki pliku
        gchar *uri = g_filename_to_uri(file_to_open, NULL, NULL);
        if (uri) {
            // Pobierz nazwę pliku i folder
            gchar *basename = g_path_get_basename(file_to_open);
            gchar *dirname = g_path_get_dirname(file_to_open);
            
            // Wyczyść poprzednie metadane i okładki
            clear_metadata();
            
            // Odtwórz plik
            gst_element_set_state(playbin, GST_STATE_NULL);
            g_object_set(playbin, "uri", uri, NULL);
            
            if (current_filename) g_free(current_filename);
            current_filename = g_strdup(basename);
            
            if (current_folder) g_free(current_folder);
            current_folder = g_strdup(dirname);
            
            set_title(GTK_WINDOW(window), current_filename);
            
            // Sprawdź, czy to plik audio i przygotuj metadane
            is_audio_only = is_audio_file(basename);
            
            // Jeśli to plik audio, spróbuj od razu załadować okładkę z folderu
            if (is_audio_only) {
                find_and_load_album_art(current_folder);
            }
            
            // Rozpocznij odtwarzanie
            gst_element_set_state(playbin, GST_STATE_PLAYING);
            
            // Aktualizuj przyciski
            gtk_widget_hide(play_button);
            gtk_widget_show(pause_button);
            
            // Zwolnij zasoby
            g_free(uri);
            g_free(basename);
            g_free(dirname);
        }
        g_free(file_to_open);
    }

    // Uruchom główną pętlę GTK
    gtk_main();

    // Sprzątanie
    if (update_id) g_source_remove(update_id);
    gst_element_set_state(playbin, GST_STATE_NULL);
    gst_object_unref(playbin);
    if (current_filename) g_free(current_filename);

    return 0;
}

// Sprawdza, czy plik jest plikiem multimedialnym na podstawie rozszerzenia
static gboolean is_media_file(const gchar *filename) {
    if (!filename) return FALSE;
    
    const gchar *ext = strrchr(filename, '.');
    if (!ext) return FALSE;
    
    // Przejdź do małych liter dla łatwiejszego porównania
    gchar *ext_lower = g_ascii_strdown(ext, -1);
    
    // Lista rozszerzeń plików multimedialnych
    const gchar *media_extensions[] = {
        // Audio
        ".mp3", ".ogg", ".flac", ".wav", ".m4a", ".aac", ".wma",
        // Video
        ".mp4", ".mkv", ".avi", ".mov", ".wmv", ".flv", ".webm", ".mpg", ".mpeg", ".m4v", ".3gp",
        NULL
    };
    
    gboolean is_media = FALSE;
    for (int i = 0; media_extensions[i] != NULL; i++) {
        if (g_str_has_suffix(ext_lower, media_extensions[i])) {
            is_media = TRUE;
            break;
        }
    }
    
    g_free(ext_lower);
    return is_media;
}

// Skanuje katalog w poszukiwaniu plików multimedialnych
static void scan_directory_for_media(const gchar *dir_path, const gchar *current_file) {
    // Wyczyść poprzednią listę plików
    if (file_list) {
        g_list_free_full(file_list, g_free);
        file_list = NULL;
        current_file_node = NULL;
    }
    
    DIR *dir = opendir(dir_path);
    if (!dir) return;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Pomijamy katalogi i ukryte pliki
        if (entry->d_name[0] == '.') continue;
        
        // Sprawdź, czy to plik multimedialny
        if (is_media_file(entry->d_name)) {
            // Dodaj pełną ścieżkę do listy
            gchar *full_path = g_build_filename(dir_path, entry->d_name, NULL);
            file_list = g_list_append(file_list, full_path);
            
            // Jeśli to aktualnie odtwarzany plik, zapamiętaj jego pozycję
            if (current_file && strcmp(entry->d_name, current_file) == 0) {
                current_file_node = g_list_last(file_list);
            }
        }
    }
    
    closedir(dir);
    
    // Sortuj listę plików alfabetycznie
    file_list = g_list_sort(file_list, (GCompareFunc)g_strcmp0);
    
    // Jeśli nie znaleziono aktualnego pliku, ustaw na pierwszy
    if (!current_file_node && file_list) {
        current_file_node = file_list;
    }
}