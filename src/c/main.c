#include <pebble.h>

#define WATCH_VERSION "2.1.1"

// Temporary settings variable: set to true to use Timer window, false for OneShot.
static bool s_use_timer_window = false;

// ---------- EXPLICIT MESSAGE KEYS ----------
#define KEY_REQUEST_CONFIG         0
#define KEY_CONFIG_DATA            1
#define KEY_INVERTED               2
#define KEY_WALLPAPER              3
#define KEY_CLOCK_MODE             4
#define KEY_LEADING_ZEROS          5
#define KEY_SHOW_AMPM              6
#define KEY_UI_COLOR               7
#define KEY_FLICK_WINDOW           8

#define KEY_HOURLY_VIBRATION          21
#define KEY_BT_DISCONNECT_VIBRATION   22
#define KEY_HOURLY_CHIME              23
#define KEY_SILENCE_QUIET_TIME        24

#define KEY_ACTIVE_WINDOW          25
#define KEY_SHOW_WINDOW            26
#define KEY_DATE_FORMAT            27
#define KEY_DATE_SEPARATOR         28
#define KEY_MONTH_FORMAT          29

// Custom image transfer keys
#define KEY_IMAGE_REQUEST          10
#define KEY_IMAGE_BEGIN            11
#define KEY_IMAGE_WIDTH            12
#define KEY_IMAGE_HEIGHT           13
#define KEY_IMAGE_LENGTH           14
#define KEY_IMAGE_CHECKSUM         15
#define KEY_IMAGE_CHUNK            16
#define KEY_IMAGE_OFFSET           17
#define KEY_IMAGE_END              18
#define KEY_IMAGE_DESIRED_CHECKSUM 19
#define KEY_IMAGE_PALETTE          20

// ---------- PERSISTENCE KEYS ----------
#define PERSIST_INVERTED       100
#define PERSIST_WALLPAPER      101
#define PERSIST_CLOCK_MODE     102
#define PERSIST_LEADING_ZEROS_MODE  204
#define PERSIST_SHOW_AMPM_MODE      205
#define PERSIST_UI_COLOR       107
#define PERSIST_RAINBOW_MODE   106
#define PERSIST_FLICK_WINDOW   206

#define PERSIST_HOURLY_VIBRATION          208
#define PERSIST_BT_DISCONNECT_VIBRATION   209
#define PERSIST_HOURLY_CHIME              210
#define PERSIST_SILENCE_QUIET_TIME        211

#define PERSIST_ACTIVE_WINDOW      212
#define PERSIST_SHOW_WINDOW        213
#define PERSIST_DATE_FORMAT        214
#define PERSIST_DATE_SEPARATOR     215
#define PERSIST_MONTH_FORMAT      216

// Custom photo persistence (single slot)
#define PERSIST_PHOTO_META      901
#define PERSIST_PHOTO_DATA      1000

// ---------- CONSTANTS ----------
#define WALLPAPER_CUSTOM       8
#define PHOTO_CHUNK_SIZE       256
#define PHOTO_MAGIC            0x544D5743  // "TWMC"
#define PHOTO_VERSION          1

// Fixed duration for "On Flick" mode
#define FLICK_ONESHOT_DURATION_MS 10000

#ifdef PBL_COLOR
  #define CUSTOM_BITMAP_FORMAT   GBitmapFormat4BitPalette
  #define CUSTOM_PALETTE_SIZE    16
#else
  #define CUSTOM_BITMAP_FORMAT   GBitmapFormat1BitPalette
  #define CUSTOM_PALETTE_SIZE    2
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

// ---------- GLOBALS ----------
static bool s_inverted = false;
static int s_wallpaper_value = 1;
static int s_clock_mode = 0;
static int s_leading_zeros_mode = 2;
static int s_show_ampm_mode = 2;
static char s_ui_color[16] = "9664ff";
#ifdef PBL_COLOR
static bool s_rainbow_active = false;
static bool s_rainbow_power_saving = false;
static AppTimer *s_rainbow_timer = NULL;
static uint8_t s_rainbow_power_hue_index = 0;
#endif

static GColor s_dark_color;
static GColor s_accent_color;
static GColor s_variant_color;   // For punctuation flashing
static GColor s_text_color;
static GColor s_background_color;
static uint32_t s_taskbar_icon_res_id;
static uint32_t s_desktop_res_id;
static uint32_t s_oneshot_window_icon_res_id;
static uint32_t s_timer_window_icon_res_id;
static uint32_t s_close_button_res_id;
static uint32_t s_minimize_button_res_id;
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
static uint32_t s_maximize_button_res_id;
#endif

static Window *s_main_window;
static Layer *s_overlay_layer;
static Layer *s_oneshot_window_layer;
static BitmapLayer *s_wallpaper_layer;
static GBitmap *s_wallpaper_bitmap;
static GBitmap *s_taskbar_icon_bitmap;
static GBitmap *s_desktop_bitmap;
static GBitmap *s_oneshot_window_icon_bitmap;
static GBitmap *s_timer_window_icon_bitmap;
static GBitmap *s_oneshot_window_content_bitmap;
static GBitmap *s_close_button_bitmap;
static GBitmap *s_minimize_button_bitmap;
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
static GBitmap *s_maximize_button_bitmap;
#endif
static GFont s_text_font;
static char s_time_buffer[32];

// Fade animation globals
static Animation *s_oneshot_fade_animation = NULL;
static GBitmap *s_oneshot_content_fade_bitmap = NULL;
static GColor s_oneshot_original_palette[16];
static uint8_t s_oneshot_palette_count = 0;
static bool s_oneshot_fade_finished = false;

// Custom wallpaper state
static GBitmap *s_custom_wallpaper_bitmap = NULL;
static bool s_custom_wallpaper_valid = false;
static bool s_custom_wallpaper_is_low_depth = false;
static bool s_want_full_custom_image = false;

// Custom image transfer state
static bool s_transfer_active = false;
static uint8_t *s_transfer_pixel_data = NULL;
static uint32_t s_transfer_pixel_length = 0;
static uint32_t s_transfer_received = 0;
static uint16_t s_transfer_checksum = 0;
static uint16_t s_transfer_running_checksum = 0;
static uint8_t s_transfer_palette[CUSTOM_PALETTE_SIZE];
static GBitmap *s_transfer_bitmap = NULL;
static AppTimer *s_transfer_timer = NULL;
static AppTimer *s_request_timer = NULL;
static uint8_t s_request_attempts = 0;

// Flick globals
static int s_flick_window = 1;
static bool s_flick_visible = false;
static AppTimer *s_flick_timer = NULL;

static int  s_hourly_vibration = 0;
static int  s_bt_disconnect_vibration = 0;
static bool s_hourly_chime = false;
static int  s_silence_quiet_time = 3;

static int s_last_hour = -1;

// Timer window animation (for punctuation flashing and ms updates)
static AppTimer *s_timer_window_flash_timer = NULL;
static AppTimer *s_timer_window_ms_timer = NULL;
static bool s_timer_punct_primary = true;

static int s_active_window = 0;      // 0=OneShot, 1=Timer
static int s_show_window = 1;        // 0=Never, 1=On Flick, 2=Always
static int s_date_format = 0;        // 0=MM.DD.YYYY, 1=DD.MM.YYYY, 2=YYYY.MM.DD
static int s_date_separator = 0;     // 0='.', 1='/', 2='-', 3=' '
static int s_month_format = 0;     // 0=Numeric (12), 1=Abbreviated (DEC)

// Forward declarations
static void schedule_image_request(void);
static void update_wallpaper(void);
static bool needs_custom_image(void);
static void update_time(void);
static void overlay_update_proc(Layer *layer, GContext *ctx);
static void oneshot_window_update_proc(Layer *layer, GContext *ctx);
static void accel_tap_handler(AccelAxisType axis, int32_t direction);
static void draw_oneshot_window(GContext *ctx);
static void draw_timer_window(GContext *ctx);
static void start_timer_window_anim(void);
static void stop_timer_window_anim(void);

typedef struct __attribute__((__packed__)) {
  uint32_t magic;
  uint16_t version;
  uint16_t width;
  uint16_t height;
  uint32_t length;
  uint16_t checksum;
  uint16_t palette_length;
} CustomPhotoMetadata;

#ifdef PBL_COLOR
static GColor hex_to_gcolor(const char *hex) {
    uint32_t val = 0;
    for (int i = 0; i < 6; i++) {
        char c = hex[i];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
    }
    return GColorFromHEX(val);
}

static GColor get_variant_from_hex(const char *hex) {
    uint32_t val = 0;
    for (int i = 0; i < 6; i++) {
        char c = hex[i];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
    }
    uint8_t r = (val >> 16) & 0xFF;
    uint8_t g = (val >> 8) & 0xFF;
    uint8_t b = val & 0xFF;

    uint8_t vr = (uint8_t)((r * 60) / 100);
    uint8_t vg = (uint8_t)((g * 60) / 100);
    uint8_t vb = (uint8_t)((b * 60) / 100);

    return GColorFromRGB(vr, vg, vb);
}
#endif

static uint32_t get_wallpaper_resource(int value) {
    switch (value) {
        case 1: return RESOURCE_ID_THE_WORLD_MACHINE;
        case 2: return RESOURCE_ID_BARRENS_CRATERS;
        case 3: return RESOURCE_ID_GLEN_SHORELINE;
        case 4: return RESOURCE_ID_REFUGE_CITYSCAPE;
        case 5: return RESOURCE_ID_MESSIAH;
        case 6: return RESOURCE_ID_MY_BURDEN_IS_LIGHT;
        case 7: return RESOURCE_ID_ASTEROID;
        default: return 0;
    }
}

static void update_colors_and_resources(void) {
#ifdef PBL_COLOR
    s_dark_color = GColorFromRGB(1,1,1);
    if (!s_rainbow_active) {
        s_accent_color = hex_to_gcolor(s_ui_color);
    }
    s_text_color = GColorFromRGB(1,1,1);
    s_background_color = GColorClear;

    // Compute variant color directly from the hex string (same as HTML)
    s_variant_color = get_variant_from_hex(s_ui_color);

    if (s_use_timer_window) {
        s_taskbar_icon_res_id = RESOURCE_ID_TIMER;
    } else {
        s_taskbar_icon_res_id = RESOURCE_ID_LIGHTBULB;
    }

    s_desktop_res_id = RESOURCE_ID_SHOW_DESKTOP;
    s_oneshot_window_icon_res_id = RESOURCE_ID_ONESHOT_WINDOW_ICON;
    s_timer_window_icon_res_id = RESOURCE_ID_TIMER_WINDOW_ICON;
    s_close_button_res_id = RESOURCE_ID_CLOSE_BUTTON;
    s_minimize_button_res_id = RESOURCE_ID_MINIMIZE_BUTTON;
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    s_maximize_button_res_id = RESOURCE_ID_MAXIMIZE_BUTTON;
#endif
#else
    if (s_inverted) {
        s_dark_color = GColorWhite;
        s_accent_color = GColorBlack;
        s_text_color = GColorWhite;
        s_background_color = GColorClear;
        // On BW, keep punctuation same as primary to avoid blinking
        s_variant_color = s_accent_color;

        if (s_use_timer_window) s_taskbar_icon_res_id = RESOURCE_ID_TIMER_INVERTED;
        else s_taskbar_icon_res_id = RESOURCE_ID_LIGHTBULB_INVERTED;
        s_desktop_res_id = RESOURCE_ID_SHOW_DESKTOP_INVERTED;
        s_oneshot_window_icon_res_id = RESOURCE_ID_ONESHOT_WINDOW_ICON_INVERTED;
        s_timer_window_icon_res_id = RESOURCE_ID_TIMER_WINDOW_ICON_INVERTED;
        s_close_button_res_id = RESOURCE_ID_CLOSE_BUTTON_INVERTED;
#ifdef RESOURCE_ID_MINIMIZE_BUTTON_INVERTED
        s_minimize_button_res_id = RESOURCE_ID_MINIMIZE_BUTTON_INVERTED;
#else
        s_minimize_button_res_id = RESOURCE_ID_MINIMIZE_BUTTON;
#endif
    } else {
        s_dark_color = GColorBlack;
        s_accent_color = GColorWhite;
        s_text_color = GColorBlack;
        s_background_color = GColorClear;
        // On BW, keep punctuation same as primary to avoid blinking
        s_variant_color = s_accent_color;

        if (s_use_timer_window) s_taskbar_icon_res_id = RESOURCE_ID_TIMER;
        else s_taskbar_icon_res_id = RESOURCE_ID_LIGHTBULB;
        s_desktop_res_id = RESOURCE_ID_SHOW_DESKTOP;
        s_oneshot_window_icon_res_id = RESOURCE_ID_ONESHOT_WINDOW_ICON;
        s_timer_window_icon_res_id = RESOURCE_ID_TIMER_WINDOW_ICON;
        s_close_button_res_id = RESOURCE_ID_CLOSE_BUTTON;
        s_minimize_button_res_id = RESOURCE_ID_MINIMIZE_BUTTON;
    }
#endif
}

static void load_settings(void) {
    if (persist_exists(PERSIST_INVERTED))
        s_inverted = persist_read_bool(PERSIST_INVERTED);
    if (persist_exists(PERSIST_WALLPAPER))
        s_wallpaper_value = persist_read_int(PERSIST_WALLPAPER);
    if (persist_exists(PERSIST_CLOCK_MODE))
        s_clock_mode = persist_read_int(PERSIST_CLOCK_MODE);

    if (persist_exists(104)) {
        bool old = persist_read_bool(104);
        persist_write_int(PERSIST_LEADING_ZEROS_MODE, old ? 1 : 0);
        persist_delete(104);
    }
    if (persist_exists(PERSIST_LEADING_ZEROS_MODE))
        s_leading_zeros_mode = persist_read_int(PERSIST_LEADING_ZEROS_MODE);
    else
        s_leading_zeros_mode = 2;

    if (persist_exists(105)) {
        bool old = persist_read_bool(105);
        persist_write_int(PERSIST_SHOW_AMPM_MODE, old ? 1 : 0);
        persist_delete(105);
    }
    if (persist_exists(PERSIST_SHOW_AMPM_MODE))
        s_show_ampm_mode = persist_read_int(PERSIST_SHOW_AMPM_MODE);
    else
        s_show_ampm_mode = 2;

    if (persist_exists(PERSIST_UI_COLOR))
        persist_read_string(PERSIST_UI_COLOR, s_ui_color, sizeof(s_ui_color));

#ifdef PBL_COLOR
    if (persist_exists(PERSIST_RAINBOW_MODE)) {
        int mode = persist_read_int(PERSIST_RAINBOW_MODE);
        if (mode == 1) {
            s_rainbow_active = true;
            s_rainbow_power_saving = false;
            strncpy(s_ui_color, "rainbow", sizeof(s_ui_color)-1);
            s_ui_color[sizeof(s_ui_color)-1] = '\0';
        } else if (mode == 2) {
            s_rainbow_active = true;
            s_rainbow_power_saving = true;
            strncpy(s_ui_color, "rainbow_power", sizeof(s_ui_color)-1);
            s_ui_color[sizeof(s_ui_color)-1] = '\0';
        }
    }
#endif

    // If old flick setting exists and new show setting has never been set,
    // migrate it to preserve user preference.
    if (persist_exists(PERSIST_FLICK_WINDOW) && !persist_exists(PERSIST_SHOW_WINDOW)) {
        s_show_window = persist_read_int(PERSIST_FLICK_WINDOW);
    }

    if (persist_exists(PERSIST_FLICK_WINDOW))
        s_flick_window = persist_read_int(PERSIST_FLICK_WINDOW);
    else
        s_flick_window = 1;

    if (persist_exists(PERSIST_HOURLY_VIBRATION))
        s_hourly_vibration = persist_read_int(PERSIST_HOURLY_VIBRATION);
    else
        s_hourly_vibration = 0;

    if (persist_exists(PERSIST_BT_DISCONNECT_VIBRATION))
        s_bt_disconnect_vibration = persist_read_int(PERSIST_BT_DISCONNECT_VIBRATION);
    else
        s_bt_disconnect_vibration = 0;

    if (persist_exists(PERSIST_HOURLY_CHIME))
        s_hourly_chime = persist_read_bool(PERSIST_HOURLY_CHIME);
    else
        s_hourly_chime = false;

    if (persist_exists(PERSIST_SILENCE_QUIET_TIME))
        s_silence_quiet_time = persist_read_int(PERSIST_SILENCE_QUIET_TIME);
    else
        s_silence_quiet_time = 3;
  
    if (persist_exists(PERSIST_ACTIVE_WINDOW))
        s_active_window = persist_read_int(PERSIST_ACTIVE_WINDOW);
    else
        s_active_window = 0;
    
    if (persist_exists(PERSIST_SHOW_WINDOW))
        s_show_window = persist_read_int(PERSIST_SHOW_WINDOW);
    else
        s_show_window = 1;   // On Flick
    
    if (persist_exists(PERSIST_DATE_FORMAT))
        s_date_format = persist_read_int(PERSIST_DATE_FORMAT);
    else
        s_date_format = 0;
    
    if (persist_exists(PERSIST_DATE_SEPARATOR))
        s_date_separator = persist_read_int(PERSIST_DATE_SEPARATOR);
    else
        s_date_separator = 0;
    
    if (persist_exists(PERSIST_MONTH_FORMAT))
        s_month_format = persist_read_int(PERSIST_MONTH_FORMAT);
    else
        s_month_format = 0;
    
    // Update derived flag
    s_use_timer_window = (s_active_window == 1);
}

static void save_settings(void) {
    persist_write_bool(PERSIST_INVERTED, s_inverted);
    persist_write_int(PERSIST_WALLPAPER, s_wallpaper_value);
    persist_write_int(PERSIST_CLOCK_MODE, s_clock_mode);
    persist_write_int(PERSIST_LEADING_ZEROS_MODE, s_leading_zeros_mode);
    persist_write_int(PERSIST_SHOW_AMPM_MODE, s_show_ampm_mode);
    persist_write_string(PERSIST_UI_COLOR, s_ui_color);

#ifdef PBL_COLOR
    if (strcmp(s_ui_color, "rainbow") == 0) {
        persist_write_int(PERSIST_RAINBOW_MODE, 1);
    } else if (strcmp(s_ui_color, "rainbow_power") == 0) {
        persist_write_int(PERSIST_RAINBOW_MODE, 2);
    } else {
        persist_delete(PERSIST_RAINBOW_MODE);
    }
#endif

    persist_write_int(PERSIST_FLICK_WINDOW, s_flick_window);
    persist_write_int(PERSIST_HOURLY_VIBRATION, s_hourly_vibration);
    persist_write_int(PERSIST_BT_DISCONNECT_VIBRATION, s_bt_disconnect_vibration);
    persist_write_bool(PERSIST_HOURLY_CHIME, s_hourly_chime);
    persist_write_int(PERSIST_SILENCE_QUIET_TIME, s_silence_quiet_time);
  
    persist_write_int(PERSIST_ACTIVE_WINDOW, s_active_window);
    persist_write_int(PERSIST_SHOW_WINDOW, s_show_window);
    persist_write_int(PERSIST_DATE_FORMAT, s_date_format);
    persist_write_int(PERSIST_DATE_SEPARATOR, s_date_separator);
    persist_write_int(PERSIST_MONTH_FORMAT, s_month_format);
}

static void perform_vibration(int mode) {
    switch (mode) {
        case 1: vibes_short_pulse(); break;
        case 2: vibes_long_pulse(); break;
        case 3: vibes_double_pulse(); break;
        default: break;
    }
}

// ---------- PCM SOUND PLAYER (Emery/Flint only) ----------
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_FLINT)

static uint8_t *s_active_wav = NULL;

static void wav_finished_callback(SpeakerFinishReason reason, void *ctx) {
    free(ctx);
    s_active_wav = NULL;
}

static bool parse_wav_header(const uint8_t *data, size_t size,
                              uint32_t *data_offset, uint32_t *data_size,
                              uint16_t *bits_per_sample, uint32_t *sample_rate) {
    if (size < 44) return false;
    if (*(uint32_t*)data != 0x46464952 || *(uint32_t*)(data + 8) != 0x45564157)
        return false;

    size_t offset = 12;
    *data_offset = 0;
    *data_size = 0;

    for (int chunk_idx = 0; chunk_idx < 100; chunk_idx++) {
        if (offset + 8 > size) break;

        uint32_t chunk_id = *(uint32_t*)(data + offset);
        uint32_t chunk_size = *(uint32_t*)(data + offset + 4);

        if (chunk_id == 0x20746d66) { // "fmt "
            if (offset + 8 + 16 > size) return false;
            uint16_t audio_format = *(uint16_t*)(data + offset + 8);
            uint16_t num_channels   = *(uint16_t*)(data + offset + 10);
            *sample_rate            = *(uint32_t*)(data + offset + 12);
            *bits_per_sample        = *(uint16_t*)(data + offset + 22);

            if (audio_format != 1) return false;
            if (num_channels != 1) return false;
            if (*bits_per_sample != 8 && *bits_per_sample != 16) return false;
        } else if (chunk_id == 0x61746164) { // "data"
            if (offset + 8 + chunk_size > size) return false;
            *data_offset = offset + 8;
            *data_size = chunk_size;
            return true;
        }

        offset += 8 + ((chunk_size + 1) & ~1);
    }

    return (*data_offset != 0 && *data_size != 0);
}

static void play_wav_resource(uint32_t resource_id) {
    if (s_active_wav != NULL) {
        speaker_set_finish_callback(NULL, NULL);
        speaker_stop();
        free(s_active_wav);
        s_active_wav = NULL;
    }

    ResHandle rh = resource_get_handle(resource_id);
    size_t res_size = resource_size(rh);
    if (res_size == 0 || res_size > 65536) return;

    uint8_t *wav_data = malloc(res_size);
    if (!wav_data) return;

    resource_load_byte_range(rh, 0, wav_data, res_size);

    uint32_t data_offset = 0, data_size = 0;
    uint16_t bits_per_sample = 0;
    uint32_t sample_rate = 0;

    if (!parse_wav_header(wav_data, res_size, &data_offset, &data_size,
                          &bits_per_sample, &sample_rate)) {
        free(wav_data);
        return;
    }

    if (data_offset + data_size > res_size) {
        free(wav_data);
        return;
    }

    if (data_size > 16384) {
        data_size = 16384;
    }

    if (bits_per_sample == 8) {
        uint8_t *samples = wav_data + data_offset;
        for (uint32_t i = 0; i < data_size; i++) {
            samples[i] ^= 0x80;
        }
    }

    SpeakerPcmFormat format;
    if (sample_rate == 8000) {
        format = (bits_per_sample == 16) ? SpeakerPcmFormat_8kHz_16bit : SpeakerPcmFormat_8kHz_8bit;
    } else if (sample_rate == 16000) {
        format = (bits_per_sample == 16) ? SpeakerPcmFormat_16kHz_16bit : SpeakerPcmFormat_16kHz_8bit;
    } else {
        free(wav_data);
        return;
    }

    SpeakerSample sample = {
        .data = wav_data + data_offset,
        .num_bytes = data_size,
        .format = format,
        .base_midi_note = 60,
        .loop = false
    };

    SpeakerNote note = {
        .midi_note = 60,
        .waveform = 0,
        .duration_ms = 30000,
        .velocity = 127
    };

    SpeakerTrack track = {
        .notes = &note,
        .num_notes = 1,
        .sample = &sample
    };

    speaker_set_finish_callback(wav_finished_callback, wav_data);
    if (speaker_play_tracks(&track, 1, 80)) {
        s_active_wav = wav_data;
    } else {
        free(wav_data);
        speaker_set_finish_callback(NULL, NULL);
    }
}

#endif // EMERY / FLINT

static void perform_hourly_chime(void) {
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_FLINT)
    play_wav_resource(RESOURCE_ID_TWM_STARTUP_SOUND);
#else
    // No sound on other platforms
#endif
}

static bool should_silence_vibration(void) {
    if (!quiet_time_is_active()) return false;
    return (s_silence_quiet_time == 1 || s_silence_quiet_time == 3);
}

static bool should_silence_sound(void) {
    if (!quiet_time_is_active()) return false;
    return (s_silence_quiet_time == 2 || s_silence_quiet_time == 3);
}

// ---------- TIME ----------
static void update_time(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    bool use_24h;
    if (s_clock_mode == 0)
        use_24h = clock_is_24h_style();
    else if (s_clock_mode == 2)
        use_24h = true;
    else
        use_24h = false;

    bool effective_leading_zeros;
    if (s_leading_zeros_mode == 0)
        effective_leading_zeros = false;
    else if (s_leading_zeros_mode == 1)
        effective_leading_zeros = true;
    else
        effective_leading_zeros = use_24h;

    bool effective_show_ampm;
    if (s_show_ampm_mode == 0)
        effective_show_ampm = false;
    else if (s_show_ampm_mode == 1)
        effective_show_ampm = true;
    else
        effective_show_ampm = !use_24h;

    if (use_24h) {
        if (effective_leading_zeros)
            strftime(s_time_buffer, sizeof(s_time_buffer), "%H:%M", t);
        else {
            strftime(s_time_buffer, sizeof(s_time_buffer), "%k:%M", t);
            if (s_time_buffer[0] == ' ')
                memmove(s_time_buffer, s_time_buffer+1, strlen(s_time_buffer));
        }
        if (effective_show_ampm) {
            strcat(s_time_buffer, " ");
            strcat(s_time_buffer, (t->tm_hour >= 12) ? "PM" : "AM");
        }
    } else {
        if (effective_show_ampm)
            strftime(s_time_buffer, sizeof(s_time_buffer), "%I:%M %p", t);
        else
            strftime(s_time_buffer, sizeof(s_time_buffer), "%I:%M", t);
        if (!effective_leading_zeros && s_time_buffer[0] == '0')
            memmove(s_time_buffer, s_time_buffer+1, strlen(s_time_buffer));
    }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time();
    if (s_overlay_layer) {
        layer_mark_dirty(s_overlay_layer);
    }

    if (s_oneshot_window_layer && s_use_timer_window) {
        layer_mark_dirty(s_oneshot_window_layer);
    }

    if (units_changed & HOUR_UNIT) {
        if (s_last_hour == -1) {
            s_last_hour = tick_time->tm_hour;
        } else if (tick_time->tm_hour != s_last_hour) {
            s_last_hour = tick_time->tm_hour;
            if (s_hourly_vibration != 0 && !should_silence_vibration()) {
                perform_vibration(s_hourly_vibration);
            }
            if (s_hourly_chime && !should_silence_sound()) {
                perform_hourly_chime();
            }
        }
    }
}

// ---------- CUSTOM IMAGE HELPERS ----------
static uint16_t image_checksum(const uint8_t *data, uint32_t length) {
    uint16_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum = (uint16_t)(sum + data[i]);
    }
    return sum;
}

static void destroy_custom_image(void) {
    if (s_custom_wallpaper_bitmap) {
        if (s_wallpaper_layer) {
            bitmap_layer_set_bitmap(s_wallpaper_layer, NULL);
            layer_set_hidden(bitmap_layer_get_layer(s_wallpaper_layer), true);
        }
        gbitmap_destroy(s_custom_wallpaper_bitmap);
        s_custom_wallpaper_bitmap = NULL;
    }
    s_custom_wallpaper_valid = false;
    s_custom_wallpaper_is_low_depth = false;
    s_want_full_custom_image = false;
}

static void cancel_transfer_timer(void) {
    if (s_transfer_timer) {
        app_timer_cancel(s_transfer_timer);
        s_transfer_timer = NULL;
    }
}

static void cancel_transfer(void) {
    cancel_transfer_timer();
    if (s_transfer_pixel_data) {
        free(s_transfer_pixel_data);
        s_transfer_pixel_data = NULL;
    }
    if (s_transfer_bitmap) {
        gbitmap_destroy(s_transfer_bitmap);
        s_transfer_bitmap = NULL;
    }
    s_transfer_active = false;
    s_transfer_pixel_length = 0;
    s_transfer_received = 0;
    s_transfer_checksum = 0;
    s_transfer_running_checksum = 0;
}

static void transfer_timeout_callback(void *context) {
    s_transfer_timer = NULL;
    if (s_transfer_active) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image transfer timed out");
        cancel_transfer();
        if (needs_custom_image()) {
            s_request_attempts = 0;
            schedule_image_request();
        }
    }
}

static bool refresh_transfer_timeout(void) {
    cancel_transfer_timer();
    if (!s_transfer_active) return true;
    s_transfer_timer = app_timer_register(10000, transfer_timeout_callback, NULL);
    return s_transfer_timer != NULL;
}

static bool needs_custom_image(void) {
    return s_wallpaper_value == WALLPAPER_CUSTOM &&
           (!s_custom_wallpaper_valid ||
            (s_custom_wallpaper_is_low_depth && s_want_full_custom_image));
}

static void request_image_callback(void *context) {
    s_request_timer = NULL;
    if (!needs_custom_image() || s_transfer_active) return;
    if (!connection_service_peek_pebble_app_connection()) return;

    DictionaryIterator *out;
    AppMessageResult result = app_message_outbox_begin(&out);
    if (result == APP_MSG_OK) {
        dict_write_uint8(out, KEY_IMAGE_REQUEST, 1);
        result = app_message_outbox_send();
    }
    s_request_attempts++;
    if (s_request_attempts < 3) {
        schedule_image_request();
    }
}

static void schedule_image_request(void) {
    if (!needs_custom_image() || s_request_timer || s_request_attempts >= 3) return;
    s_request_timer = app_timer_register(1500, request_image_callback, NULL);
}

// ---------- PERSISTENCE SIZE HELPERS ----------
static uint32_t full_persist_storage_size(void) {
#ifdef PBL_COLOR
    return (uint32_t)((PBL_DISPLAY_WIDTH + 1) / 2) * PBL_DISPLAY_HEIGHT + CUSTOM_PALETTE_SIZE;
#else
    return (uint32_t)((PBL_DISPLAY_WIDTH + 7) / 8) * PBL_DISPLAY_HEIGHT + CUSTOM_PALETTE_SIZE;
#endif
}

static bool size_fits(uint32_t size) {
    const uint32_t margin = sizeof(CustomPhotoMetadata) + 256;
    return (size + margin) <= persist_get_max_size();
}

static bool full_persist_possible(void) {
    return size_fits(full_persist_storage_size());
}

static void delete_photo_storage(void) {
    for (int i = 0; i < 64; i++) {
        persist_delete(PERSIST_PHOTO_DATA + i);
    }
    persist_delete(PERSIST_PHOTO_META);
}

// Persist full-res if possible, otherwise half-res (color only)
static bool persist_custom_image(const uint8_t *pixel_data, uint32_t pixel_len,
                                 const uint8_t *palette, uint16_t palette_len) {
    uint8_t temp[PHOTO_CHUNK_SIZE];
    uint16_t width, height;
    uint32_t total_len;

    if (full_persist_possible()) {
        width = PBL_DISPLAY_WIDTH;
        height = PBL_DISPLAY_HEIGHT;
        total_len = pixel_len + palette_len;
        delete_photo_storage();

        uint16_t checksum = image_checksum(pixel_data, pixel_len);
        checksum = (uint16_t)(checksum + image_checksum(palette, palette_len));

        uint32_t offset = 0;
        while (offset < total_len) {
            uint16_t chunk_len = (uint16_t)MIN(PHOTO_CHUNK_SIZE, total_len - offset);
            const uint8_t *src = NULL;

            if (offset >= pixel_len) {
                src = palette + (offset - pixel_len);
            } else if (offset + chunk_len <= pixel_len) {
                src = pixel_data + offset;
            } else {
                uint32_t pixel_part = pixel_len - offset;
                uint32_t palette_part = chunk_len - pixel_part;
                memcpy(temp, pixel_data + offset, pixel_part);
                memcpy(temp + pixel_part, palette, palette_part);
                src = temp;
            }

            if (persist_write_data(PERSIST_PHOTO_DATA + offset / PHOTO_CHUNK_SIZE,
                                   src, chunk_len) != chunk_len) {
                delete_photo_storage();
                return false;
            }
            offset += chunk_len;
        }

        CustomPhotoMetadata meta = {
            .magic = PHOTO_MAGIC,
            .version = PHOTO_VERSION,
            .width = width,
            .height = height,
            .length = total_len,
            .checksum = checksum,
            .palette_length = palette_len
        };

        if (persist_write_data(PERSIST_PHOTO_META, &meta, sizeof(meta)) != (int)sizeof(meta)) {
            delete_photo_storage();
            return false;
        }

        APP_LOG(APP_LOG_LEVEL_INFO, "Custom image persisted full-res");
        return true;
    }

#ifdef PBL_COLOR
    // Half-res fallback
    uint16_t half_w = PBL_DISPLAY_WIDTH / 2;
    uint16_t half_h = PBL_DISPLAY_HEIGHT / 2;
    uint16_t half_row_bytes = (half_w + 1) / 2;
    uint32_t half_pixel_len = (uint32_t)half_row_bytes * half_h;
    uint8_t *half_pixels = malloc(half_pixel_len);

    if (!half_pixels) {
        return false;
    }

    uint16_t full_row_bytes = (PBL_DISPLAY_WIDTH + 1) / 2;
    for (int y = 0; y < half_h; ++y) {
        int src_y = y * 2;
        for (int x = 0; x < half_w; ++x) {
            int src_x = x * 2;
            int src_byte = src_y * full_row_bytes + (src_x / 2);
            uint8_t src_pair = pixel_data[src_byte];
            uint8_t index = (src_x & 1) ? (src_pair & 0x0F) : (src_pair >> 4);
            int dst_byte = y * half_row_bytes + (x / 2);
            if ((x & 1) == 0) {
                half_pixels[dst_byte] = index << 4;
            } else {
                half_pixels[dst_byte] |= index;
            }
        }
    }

    total_len = half_pixel_len + palette_len;
    uint16_t checksum = image_checksum(half_pixels, half_pixel_len);
    checksum = (uint16_t)(checksum + image_checksum(palette, palette_len));

    delete_photo_storage();

    uint32_t offset = 0;
    while (offset < total_len) {
        uint16_t chunk_len = (uint16_t)MIN(PHOTO_CHUNK_SIZE, total_len - offset);
        const uint8_t *src = NULL;

        if (offset >= half_pixel_len) {
            src = palette + (offset - half_pixel_len);
        } else if (offset + chunk_len <= half_pixel_len) {
            src = half_pixels + offset;
        } else {
            uint32_t pixel_part = half_pixel_len - offset;
            uint32_t palette_part = chunk_len - pixel_part;
            memcpy(temp, half_pixels + offset, pixel_part);
            memcpy(temp + pixel_part, palette, palette_part);
            src = temp;
        }

        if (persist_write_data(PERSIST_PHOTO_DATA + offset / PHOTO_CHUNK_SIZE,
                               src, chunk_len) != chunk_len) {
            free(half_pixels);
            delete_photo_storage();
            return false;
        }
        offset += chunk_len;
    }

    free(half_pixels);

    CustomPhotoMetadata meta = {
        .magic = PHOTO_MAGIC,
        .version = PHOTO_VERSION,
        .width = half_w,
        .height = half_h,
        .length = total_len,
        .checksum = checksum,
        .palette_length = palette_len
    };

    if (persist_write_data(PERSIST_PHOTO_META, &meta, sizeof(meta)) != (int)sizeof(meta)) {
        delete_photo_storage();
        return false;
    }

    APP_LOG(APP_LOG_LEVEL_INFO, "Custom image persisted half-res");
    return true;
#else
    return false;
#endif
}

static bool load_persisted_custom_image(void) {
    CustomPhotoMetadata meta;
    if (persist_read_data(PERSIST_PHOTO_META, &meta, sizeof(meta)) != (int)sizeof(meta) ||
        meta.magic != PHOTO_MAGIC || meta.version != PHOTO_VERSION ||
        meta.palette_length != CUSTOM_PALETTE_SIZE) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image metadata invalid");
        return false;
    }

    bool is_full = (meta.width == PBL_DISPLAY_WIDTH && meta.height == PBL_DISPLAY_HEIGHT);
    bool is_half = false;
#ifdef PBL_COLOR
    is_half = (meta.width == PBL_DISPLAY_WIDTH / 2 && meta.height == PBL_DISPLAY_HEIGHT / 2);
#endif
    if (!is_full && !is_half) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image dimensions not recognized: %dx%d",
                meta.width, meta.height);
        return false;
    }

    uint32_t total_len = meta.length;
    if (total_len <= meta.palette_length) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image length too small");
        return false;
    }

    uint8_t *combined = malloc(total_len);
    if (!combined) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image malloc failed");
        return false;
    }

    uint32_t offset = 0;
    bool read_ok = true;
    while (offset < total_len) {
        uint16_t chunk_len = (uint16_t)MIN(PHOTO_CHUNK_SIZE, total_len - offset);
        if (persist_read_data(PERSIST_PHOTO_DATA + offset / PHOTO_CHUNK_SIZE,
                              combined + offset, chunk_len) != chunk_len) {
            APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image chunk read failed at %lu", (unsigned long)offset);
            read_ok = false;
            break;
        }
        offset += chunk_len;
    }

    if (!read_ok) {
        free(combined);
        return false;
    }

    bool checksum_ok = (image_checksum(combined, total_len) == meta.checksum);
    if (!checksum_ok) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image checksum mismatch, using anyway");
    }

    uint32_t persist_pixel_len = total_len - meta.palette_length;
    uint8_t *persist_pixels = combined;
    uint8_t *persist_palette = combined + persist_pixel_len;

    GBitmap *full_bitmap = gbitmap_create_blank(GSize(PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT),
                                                CUSTOM_BITMAP_FORMAT);
    if (!full_bitmap) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image bitmap creation failed");
        free(combined);
        return false;
    }

    if (is_full) {
        memcpy(gbitmap_get_data(full_bitmap), persist_pixels, persist_pixel_len);
        memcpy(gbitmap_get_palette(full_bitmap), persist_palette, CUSTOM_PALETTE_SIZE);
    } else {
#ifdef PBL_COLOR
        uint16_t full_row_bytes = gbitmap_get_bytes_per_row(full_bitmap);
        uint8_t *full_data = gbitmap_get_data(full_bitmap);
        uint16_t half_row_bytes = (meta.width + 1) / 2;
        for (int y = 0; y < PBL_DISPLAY_HEIGHT; ++y) {
            int src_y = y / 2;
            for (int x = 0; x < PBL_DISPLAY_WIDTH; ++x) {
                int src_x = x / 2;
                int src_byte = src_y * half_row_bytes + (src_x / 2);
                uint8_t src_pair = persist_pixels[src_byte];
                uint8_t index = (src_x & 1) ? (src_pair & 0x0F) : (src_pair >> 4);
                int dst_byte = y * full_row_bytes + (x / 2);
                if ((x & 1) == 0) {
                    full_data[dst_byte] = index << 4;
                } else {
                    full_data[dst_byte] |= index;
                }
            }
        }
        memcpy(gbitmap_get_palette(full_bitmap), persist_palette, CUSTOM_PALETTE_SIZE);
#else
        gbitmap_destroy(full_bitmap);
        free(combined);
        return false;
#endif
    }

    GBitmap *old_custom = s_custom_wallpaper_bitmap;
    s_custom_wallpaper_bitmap = full_bitmap;
    s_custom_wallpaper_valid = true;
    s_custom_wallpaper_is_low_depth = is_half;
    s_want_full_custom_image = is_half;

    if (s_wallpaper_layer) {
        bitmap_layer_set_bitmap(s_wallpaper_layer, s_custom_wallpaper_bitmap);
        layer_set_hidden(bitmap_layer_get_layer(s_wallpaper_layer), false);
        if (s_main_window) window_set_background_color(s_main_window, GColorClear);
    }

    if (old_custom) {
        gbitmap_destroy(old_custom);
    }

    free(combined);
    APP_LOG(APP_LOG_LEVEL_INFO, "Custom image loaded: %s, checksum %s",
            is_half ? "half-res" : "full-res",
            checksum_ok ? "OK" : "FAILED");
    return true;
}

static void begin_custom_image_transfer(uint16_t width, uint16_t height,
                                        uint32_t length, uint16_t checksum,
                                        const uint8_t *palette) {
    if (width != PBL_DISPLAY_WIDTH || height != PBL_DISPLAY_HEIGHT) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image wrong dimensions");
        return;
    }

    GBitmap *bitmap = gbitmap_create_blank(GSize(width, height), CUSTOM_BITMAP_FORMAT);
    if (!bitmap) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Cannot create custom bitmap");
        return;
    }

    uint16_t row_bytes = gbitmap_get_bytes_per_row(bitmap);
    uint32_t pixel_length = (uint32_t)row_bytes * height;
    if (length != pixel_length) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Custom image length mismatch");
        gbitmap_destroy(bitmap);
        return;
    }

    cancel_transfer();

    s_transfer_pixel_data = malloc(length);
    if (!s_transfer_pixel_data) {
        if (s_custom_wallpaper_bitmap) {
            destroy_custom_image();
        }
        s_transfer_pixel_data = malloc(length);
        if (!s_transfer_pixel_data) {
            gbitmap_destroy(bitmap);
            APP_LOG(APP_LOG_LEVEL_ERROR, "Cannot allocate custom image buffer");
            return;
        }
    }

    memcpy(s_transfer_palette, palette, CUSTOM_PALETTE_SIZE);
    s_transfer_bitmap = bitmap;
    s_transfer_pixel_length = length;
    s_transfer_received = 0;
    s_transfer_checksum = checksum;
    s_transfer_running_checksum = 0;
    s_transfer_active = true;

    if (!refresh_transfer_timeout()) {
        cancel_transfer();
    }
}

static void fail_transfer(const char *reason) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image transfer failed: %s", reason);
    cancel_transfer();
    if (s_wallpaper_value == WALLPAPER_CUSTOM) {
        load_persisted_custom_image();
        update_wallpaper();
    }
    if (needs_custom_image()) {
        schedule_image_request();
    }
}

static void finish_custom_image_transfer(uint32_t length, uint16_t checksum) {
    if (!s_transfer_active) return;
    if (length != s_transfer_pixel_length ||
        checksum != s_transfer_checksum ||
        s_transfer_received != s_transfer_pixel_length ||
        s_transfer_running_checksum != s_transfer_checksum) {
        fail_transfer("validation");
        return;
    }

    memcpy(gbitmap_get_data(s_transfer_bitmap), s_transfer_pixel_data, s_transfer_pixel_length);
    memcpy(gbitmap_get_palette(s_transfer_bitmap), s_transfer_palette, CUSTOM_PALETTE_SIZE);

   (void)persist_custom_image(s_transfer_pixel_data, s_transfer_pixel_length,
                                          s_transfer_palette, CUSTOM_PALETTE_SIZE);

    GBitmap *old_custom = s_custom_wallpaper_bitmap;

    s_custom_wallpaper_bitmap = s_transfer_bitmap;
    s_custom_wallpaper_valid = true;
    s_custom_wallpaper_is_low_depth = false;
    s_want_full_custom_image = false;
    s_transfer_bitmap = NULL;

    update_wallpaper();

    if (old_custom) {
        gbitmap_destroy(old_custom);
    }

    s_transfer_active = false;
    cancel_transfer_timer();
    free(s_transfer_pixel_data);
    s_transfer_pixel_data = NULL;

    APP_LOG(APP_LOG_LEVEL_INFO, "Custom image ready");
}

static void update_wallpaper(void) {
    if (!s_wallpaper_layer) return;

    if (s_wallpaper_bitmap) {
        gbitmap_destroy(s_wallpaper_bitmap);
        s_wallpaper_bitmap = NULL;
    }

    if (s_wallpaper_value == 0) {
        layer_set_hidden(bitmap_layer_get_layer(s_wallpaper_layer), true);
        if (s_main_window) window_set_background_color(s_main_window, GColorBlack);
        return;
    }

    if (s_wallpaper_value == WALLPAPER_CUSTOM) {
        if (s_custom_wallpaper_valid && s_custom_wallpaper_bitmap) {
            bitmap_layer_set_bitmap(s_wallpaper_layer, s_custom_wallpaper_bitmap);
            layer_set_hidden(bitmap_layer_get_layer(s_wallpaper_layer), false);
            if (s_main_window) window_set_background_color(s_main_window, GColorClear);
        } else {
            layer_set_hidden(bitmap_layer_get_layer(s_wallpaper_layer), true);
            if (s_main_window) window_set_background_color(s_main_window, GColorBlack);
            schedule_image_request();
        }
        return;
    }

    uint32_t res = get_wallpaper_resource(s_wallpaper_value);
    if (!res) {
        layer_set_hidden(bitmap_layer_get_layer(s_wallpaper_layer), true);
        if (s_main_window) window_set_background_color(s_main_window, GColorBlack);
        return;
    }
    s_wallpaper_bitmap = gbitmap_create_with_resource(res);
    if (!s_wallpaper_bitmap) {
        layer_set_hidden(bitmap_layer_get_layer(s_wallpaper_layer), true);
        if (s_main_window) window_set_background_color(s_main_window, GColorBlack);
        return;
    }
    bitmap_layer_set_bitmap(s_wallpaper_layer, s_wallpaper_bitmap);
    layer_set_hidden(bitmap_layer_get_layer(s_wallpaper_layer), false);
    if (s_main_window) window_set_background_color(s_main_window, GColorClear);
}

// ---------- RAINBOW HELPERS (color only) ----------
#ifdef PBL_COLOR

#define RAINBOW_SMOOTH_STEP_MS 50
#define RAINBOW_POWER_STEP_MS 1000

static GColor hue_to_gcolor(uint8_t hue) {
    uint8_t sector = hue / 43;
    uint8_t rem = (hue % 43) * 6;
    uint8_t p, q, t;
    switch (sector) {
        case 0: p = 255; q = rem; t = 0; break;
        case 1: p = 255 - rem; q = 255; t = 0; break;
        case 2: p = 0; q = 255; t = rem; break;
        case 3: p = 0; q = 255 - rem; t = 255; break;
        case 4: p = rem; q = 0; t = 255; break;
        case 5: p = 255; q = 0; t = 255 - rem; break;
        default: p = 255; q = 0; t = 0; break;
    }
    return GColorFromRGB(p, q, t);
}

static void update_rainbow_colors(void) {
    static float smooth_hue = 0.0f;

    if (s_rainbow_power_saving) {
        uint8_t hue = s_rainbow_power_hue_index * (256 / 6);
        s_accent_color = hue_to_gcolor(hue);
        s_rainbow_power_hue_index = (s_rainbow_power_hue_index + 1) % 6;
    } else {
        smooth_hue += (256.0f / (5000 / RAINBOW_SMOOTH_STEP_MS));
        if (smooth_hue >= 256.0f) smooth_hue -= 256.0f;
        uint8_t hue = (uint8_t)smooth_hue;
        s_accent_color = hue_to_gcolor(hue);
    }

    if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
    if (s_oneshot_window_layer) layer_mark_dirty(s_oneshot_window_layer);
}

static void rainbow_timer_callback(void *context) {
    update_rainbow_colors();
    uint32_t next_interval = s_rainbow_power_saving ? RAINBOW_POWER_STEP_MS : RAINBOW_SMOOTH_STEP_MS;
    s_rainbow_timer = app_timer_register(next_interval, rainbow_timer_callback, NULL);
}

static void start_rainbow_timer(void) {
    if (s_rainbow_timer) {
        app_timer_cancel(s_rainbow_timer);
        s_rainbow_timer = NULL;
    }
    if (s_rainbow_active) {
        update_rainbow_colors();
        uint32_t interval = s_rainbow_power_saving ? RAINBOW_POWER_STEP_MS : RAINBOW_SMOOTH_STEP_MS;
        s_rainbow_timer = app_timer_register(interval, rainbow_timer_callback, NULL);
    }
}

static void stop_rainbow_timer(void) {
    if (s_rainbow_timer) {
        app_timer_cancel(s_rainbow_timer);
        s_rainbow_timer = NULL;
    }
}

#endif // PBL_COLOR

// ---------- APPLY INVERTED ----------
static void apply_inverted(bool inverted) {
    s_inverted = inverted;
    save_settings();
    update_colors_and_resources();
    if (s_main_window)
        window_set_background_color(s_main_window, s_background_color);

    if (s_taskbar_icon_bitmap) {
        gbitmap_destroy(s_taskbar_icon_bitmap);
        s_taskbar_icon_bitmap = NULL;
    }
    s_taskbar_icon_bitmap = gbitmap_create_with_resource(s_taskbar_icon_res_id);

    if (s_desktop_bitmap) {
        gbitmap_destroy(s_desktop_bitmap);
        s_desktop_bitmap = NULL;
    }
    s_desktop_bitmap = gbitmap_create_with_resource(s_desktop_res_id);

    if (s_oneshot_window_icon_bitmap) {
        gbitmap_destroy(s_oneshot_window_icon_bitmap);
        s_oneshot_window_icon_bitmap = NULL;
    }
    s_oneshot_window_icon_bitmap = gbitmap_create_with_resource(s_oneshot_window_icon_res_id);

    if (s_timer_window_icon_bitmap) {
        gbitmap_destroy(s_timer_window_icon_bitmap);
        s_timer_window_icon_bitmap = NULL;
    }
    s_timer_window_icon_bitmap = gbitmap_create_with_resource(s_timer_window_icon_res_id);

    if (s_close_button_bitmap) {
        gbitmap_destroy(s_close_button_bitmap);
        s_close_button_bitmap = NULL;
    }
    s_close_button_bitmap = gbitmap_create_with_resource(s_close_button_res_id);

    if (s_minimize_button_bitmap) {
        gbitmap_destroy(s_minimize_button_bitmap);
        s_minimize_button_bitmap = NULL;
    }
    s_minimize_button_bitmap = gbitmap_create_with_resource(s_minimize_button_res_id);

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    if (s_maximize_button_bitmap) {
        gbitmap_destroy(s_maximize_button_bitmap);
        s_maximize_button_bitmap = NULL;
    }
    s_maximize_button_bitmap = gbitmap_create_with_resource(s_maximize_button_res_id);
#endif

    update_wallpaper();
    update_time();
    if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
    if (s_oneshot_window_layer) layer_mark_dirty(s_oneshot_window_layer);
}

#ifdef PBL_COLOR
static void apply_ui_color(const char *hex) {
    if (strcmp(hex, "rainbow") == 0) {
        strncpy(s_ui_color, "rainbow", sizeof(s_ui_color)-1);
        s_ui_color[sizeof(s_ui_color)-1] = '\0';
        s_rainbow_active = true;
        s_rainbow_power_saving = false;
        start_rainbow_timer();
    } else if (strcmp(hex, "rainbow_power") == 0) {
        strncpy(s_ui_color, "rainbow_power", sizeof(s_ui_color)-1);
        s_ui_color[sizeof(s_ui_color)-1] = '\0';
        s_rainbow_active = true;
        s_rainbow_power_saving = true;
        s_rainbow_power_hue_index = 0;
        start_rainbow_timer();
    } else {
        if (hex[0] == '#') hex++;
        strncpy(s_ui_color, hex, sizeof(s_ui_color)-1);
        s_ui_color[sizeof(s_ui_color)-1] = '\0';
        s_rainbow_active = false;
        stop_rainbow_timer();
        s_accent_color = hex_to_gcolor(hex);

        // Recompute variant color directly from the hex string
        s_variant_color = get_variant_from_hex(hex);

        if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
        if (s_oneshot_window_layer) layer_mark_dirty(s_oneshot_window_layer);
    }
    save_settings();
}
#endif

// ---------- LAYOUT CONSTANTS ----------
#define LINE_HEIGHT 28
#define SEPARATOR_HEIGHT 2
#define RECTANGLE_HEIGHT 24
#define RECTANGLE_OFFSET_Y 2

#define ANCHOR_WIDTH 26
#define ANCHOR_HEIGHT 9
#define ANCHOR_OFFSET_X 2
#define ANCHOR_OFFSET_Y 2
#define TEXT_STRING "OneShot"
#define TEXT_OFFSET_X -2
#define TEXT_OFFSET_Y 2
#define TIME_TEXT_RIGHT_OFFSET 24
#define TIME_TEXT_BOTTOM_OFFSET 10
#define TIME_TEXT_OFFSET_X 0
#define TIME_TEXT_OFFSET_Y -1
#define ANCHOR_4X6_WIDTH 4
#define ANCHOR_4X6_HEIGHT 6
#define ANCHOR_4X6_RIGHT_OFFSET 0
#define ANCHOR_4X6_BOTTOM_OFFSET 0
#define ANCHOR_4X6_OFFSET_X 0
#define ANCHOR_4X6_OFFSET_Y 0
#define RECT_16X16_OFFSET_FROM_ANCHOR_X -16
#define RECT_16X16_OFFSET_FROM_ANCHOR_Y -16
#define LIGHTBULB_OFFSET_Y 4

#if !defined(PBL_PLATFORM_CHALK) && !defined(PBL_PLATFORM_GABBRO)
  #define RECTANGLE_OFFSET_X 2
  #if defined(PBL_PLATFORM_EMERY)
    #define RECTANGLE_WIDTH 90
  #else
    #define RECTANGLE_WIDTH 68
  #endif
  #define LIGHTBULB_OFFSET_X 4
  #define CHALK_SPECIAL_LAYOUT 0
#endif

#if defined(PBL_PLATFORM_GABBRO)
  #define RECTANGLE_OFFSET_X 2
  #define RECTANGLE_WIDTH 90
  #define LIGHTBULB_OFFSET_X 4
  #define CHALK_SPECIAL_LAYOUT 1
#endif

#if defined(PBL_PLATFORM_CHALK)
  #define RECTANGLE_OFFSET_X 0
  #define LIGHTBULB_OFFSET_X 4
  #define CHALK_SPECIAL_LAYOUT 1
#endif

// ---------- ONESHOT WINDOW CONSTANTS ----------
#define ONESHOT_WIN_BORDER           2
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  #define ONESHOT_WIN_CONTENT_W        160
  #define ONESHOT_WIN_CONTENT_H        120
#else
  #define ONESHOT_WIN_CONTENT_W        96
  #define ONESHOT_WIN_CONTENT_H        72
#endif
#define ONESHOT_WIN_ICON_SIZE        16
#define ONESHOT_WIN_ICON_LEFT_MARGIN 2
#define ONESHOT_WIN_ICON_TOP_MARGIN  2
#define ONESHOT_WIN_ICON_BOTTOM_MARGIN 2
#define ONESHOT_WIN_CLOSE_RIGHT_MARGIN 2
#define ONESHOT_WIN_DIVIDER_H        4
#define ONESHOT_WIN_TOPBAR_H         (ONESHOT_WIN_ICON_SIZE + ONESHOT_WIN_ICON_TOP_MARGIN + ONESHOT_WIN_ICON_BOTTOM_MARGIN)
#define ONESHOT_WIN_TOTAL_W          (ONESHOT_WIN_BORDER * 2 + ONESHOT_WIN_CONTENT_W)
#define ONESHOT_WIN_TOTAL_H          (ONESHOT_WIN_BORDER * 2 + ONESHOT_WIN_TOPBAR_H + ONESHOT_WIN_DIVIDER_H + ONESHOT_WIN_CONTENT_H)

// ---------- TIMER WINDOW CONSTANTS ----------
#define TIMER_WIN_BORDER             2
#define TIMER_WIN_ICON_SIZE          16
#define TIMER_WIN_ICON_TOP_MARGIN    2
#define TIMER_WIN_ICON_BOTTOM_MARGIN 2
#define TIMER_WIN_ICON_LEFT_MARGIN   2
#define TIMER_WIN_TITLE_GAP          4
#define TIMER_WIN_CLOSE_RIGHT_MARGIN 2
#define TIMER_WIN_DIVIDER_H          4
#define TIMER_WIN_TOPBAR_H           (TIMER_WIN_ICON_SIZE + TIMER_WIN_ICON_TOP_MARGIN + TIMER_WIN_ICON_BOTTOM_MARGIN)
#define TIMER_WIN_CONTENT_W          93
#define TIMER_WIN_CONTENT_PADDING    2
#define TIMER_WIN_SYSTIME_TOP        6
#define TIMER_WIN_SYSTIME_LABEL_H    12
#define TIMER_WIN_SYSTIME_RECT_GAP   3
#define TIMER_WIN_TIME_RECT_H        20
#define TIMER_WIN_TIME_TOP_PAD       4
#define TIMER_WIN_TIME_BOTTOM_PAD    3
#define TIMER_WIN_SYSTIME_TO_DATE_GAP 4
#define TIMER_WIN_DATE_LABEL_H       12
#define TIMER_WIN_DATE_RECT_GAP      5
#define TIMER_WIN_DATE_RECT_BOTTOM_GAP 3
#define TIMER_WIN_TIME_LEFT_PAD      2
#define TIMER_WIN_TIME_RIGHT_PAD     2
#define TIMER_WIN_LABEL_LEFT_PAD     4

// Fixed positions from the right edge of the System Time rectangle
#define TIMER_WIN_HOUR_3DIGIT_OFFSET   71   // HHH
#define TIMER_WIN_HOUR_2DIGIT_OFFSET   65   // HH
#define TIMER_WIN_HOUR_1DIGIT_OFFSET   59   // H
#define TIMER_WIN_COLON1_OFFSET        53   // first ':'
#define TIMER_WIN_MIN_OFFSET           50   // MM
#define TIMER_WIN_COLON2_OFFSET        38   // second ':'
#define TIMER_WIN_SEC_OFFSET           35   // SS
#define TIMER_WIN_MS_OFFSET            23   // .ddd (unchanged)

// Debug: set to true to replace all timer digits with zeros (for layout testing)
static bool s_timer_debug_zero = false;

// ---------- FADE ANIMATION HELPERS ----------
static GBitmap *copy_bitmap_for_fade(GBitmap *src) {
    if (!src) return NULL;
    GBitmapFormat format = gbitmap_get_format(src);
    GSize size = gbitmap_get_bounds(src).size;
    GBitmap *copy = gbitmap_create_blank(size, format);
    if (!copy) return NULL;
    uint16_t bytes_per_row = gbitmap_get_bytes_per_row(src);
    uint8_t *src_data = gbitmap_get_data(src);
    uint8_t *dst_data = gbitmap_get_data(copy);
    for (int y = 0; y < size.h; y++) {
        memcpy(dst_data + y * bytes_per_row,
               src_data + y * bytes_per_row,
               bytes_per_row);
    }
    if (format == GBitmapFormat1BitPalette || format == GBitmapFormat4BitPalette) {
        GColor *src_pal = gbitmap_get_palette(src);
        GColor *dst_pal = gbitmap_get_palette(copy);
        s_oneshot_palette_count = (format == GBitmapFormat1BitPalette) ? 2 : 16;
        memcpy(dst_pal, src_pal, s_oneshot_palette_count * sizeof(GColor));
        memcpy(s_oneshot_original_palette, src_pal, s_oneshot_palette_count * sizeof(GColor));
    } else {
        s_oneshot_palette_count = 0;
    }
    return copy;
}

static void oneshot_fade_update(Animation *animation, const AnimationProgress progress) {
    if (!s_oneshot_content_fade_bitmap || s_oneshot_palette_count == 0) return;
    GColor *palette = gbitmap_get_palette(s_oneshot_content_fade_bitmap);
    for (int i = 0; i < s_oneshot_palette_count; i++) {
        GColor target = s_oneshot_original_palette[i];
        uint8_t fr = (target.r * progress) / ANIMATION_NORMALIZED_MAX;
        uint8_t fg = (target.g * progress) / ANIMATION_NORMALIZED_MAX;
        uint8_t fb = (target.b * progress) / ANIMATION_NORMALIZED_MAX;
        palette[i].r = fr;
        palette[i].g = fg;
        palette[i].b = fb;
        palette[i].a = target.a;
    }
    if (s_oneshot_window_layer) layer_mark_dirty(s_oneshot_window_layer);
}

static void oneshot_fade_setup(Animation *animation) {
    oneshot_fade_update(animation, 0);
}

static void oneshot_fade_teardown(Animation *animation) { }

static const AnimationImplementation s_oneshot_fade_impl = {
    .setup = oneshot_fade_setup,
    .update = oneshot_fade_update,
    .teardown = oneshot_fade_teardown
};

static void oneshot_fade_stopped(Animation *animation, bool finished, void *context) {
    if (finished) {
        animation_destroy(animation);
        s_oneshot_fade_animation = NULL;
        s_oneshot_fade_finished = true;
    }
}

static const AnimationHandlers s_oneshot_fade_handlers = {
    .stopped = oneshot_fade_stopped
};

static void start_oneshot_fade(void) {
    if (!s_oneshot_content_fade_bitmap || s_oneshot_palette_count == 0) return;
    if (s_oneshot_fade_animation) {
        animation_unschedule(s_oneshot_fade_animation);
        animation_destroy(s_oneshot_fade_animation);
        s_oneshot_fade_animation = NULL;
    }
    oneshot_fade_update(NULL, 0);
    s_oneshot_fade_animation = animation_create();
    animation_set_duration(s_oneshot_fade_animation, 1000);
    animation_set_curve(s_oneshot_fade_animation, AnimationCurveEaseOut);
    animation_set_implementation(s_oneshot_fade_animation, &s_oneshot_fade_impl);
    animation_set_handlers(s_oneshot_fade_animation, s_oneshot_fade_handlers, NULL);
    s_oneshot_fade_finished = false;
    animation_schedule(s_oneshot_fade_animation);
}

// ---------- TIMER WINDOW ANIMATION ----------
static void timer_window_ms_callback(void *ctx) {
    if (s_oneshot_window_layer) layer_mark_dirty(s_oneshot_window_layer);
    s_timer_window_ms_timer = app_timer_register(100, timer_window_ms_callback, NULL);
}

static void timer_window_flash_callback(void *ctx) {
    s_timer_punct_primary = !s_timer_punct_primary;
    if (s_oneshot_window_layer) layer_mark_dirty(s_oneshot_window_layer);
    s_timer_window_flash_timer = app_timer_register(333, timer_window_flash_callback, NULL);
}

static void start_timer_window_anim(void) {
    stop_timer_window_anim();
    s_timer_punct_primary = true;
    s_timer_window_ms_timer = app_timer_register(100, timer_window_ms_callback, NULL);
    s_timer_window_flash_timer = app_timer_register(333, timer_window_flash_callback, NULL);
}

static void stop_timer_window_anim(void) {
    if (s_timer_window_ms_timer) {
        app_timer_cancel(s_timer_window_ms_timer);
        s_timer_window_ms_timer = NULL;
    }
    if (s_timer_window_flash_timer) {
        app_timer_cancel(s_timer_window_flash_timer);
        s_timer_window_flash_timer = NULL;
    }
}

// ---------- UI UPDATE PROCS ----------
static void overlay_update_proc(Layer *layer, GContext *ctx) {
#if defined(PBL_PLATFORM_GABBRO)
    GRect bounds = layer_get_bounds(layer);
#else
    GRect bounds = layer_get_unobstructed_bounds(layer);
#endif
    int w = bounds.size.w, h = bounds.size.h;
    graphics_context_set_compositing_mode(ctx, GCompOpSet);

    int colored_y = h - LINE_HEIGHT;
    int separator_y = colored_y - SEPARATOR_HEIGHT;

    graphics_context_set_fill_color(ctx, s_dark_color);
    graphics_fill_rect(ctx, GRect(0, colored_y, w, LINE_HEIGHT), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(0, separator_y, w, SEPARATOR_HEIGHT), 0, GCornerNone);

    update_time();
    GSize time_size = graphics_text_layout_get_content_size(s_time_buffer, s_text_font,
                                                            GRect(0,0,200,200),
                                                            GTextOverflowModeWordWrap,
                                                            GTextAlignmentLeft);

#if CHALK_SPECIAL_LAYOUT
    int clock_y = h - time_size.h - TIME_TEXT_BOTTOM_OFFSET;
    int clock_x = (w - time_size.w) / 2;
    graphics_context_set_text_color(ctx, s_accent_color);
    graphics_draw_text(ctx, s_time_buffer, s_text_font,
                       GRect(clock_x, clock_y, time_size.w, time_size.h),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
#else
    if (s_flick_visible) {
        int rx = RECTANGLE_OFFSET_X;
        int ry = h - RECTANGLE_OFFSET_Y - RECTANGLE_HEIGHT;
        graphics_context_set_fill_color(ctx, s_accent_color);
        graphics_fill_rect(ctx, GRect(rx, ry, RECTANGLE_WIDTH, RECTANGLE_HEIGHT), 0, GCornerNone);

        if (s_taskbar_icon_bitmap) {
            GRect ib = gbitmap_get_bounds(s_taskbar_icon_bitmap);
            graphics_draw_bitmap_in_rect(ctx, s_taskbar_icon_bitmap,
                                         GRect(rx + LIGHTBULB_OFFSET_X,
                                               ry + LIGHTBULB_OFFSET_Y,
                                               ib.size.w, ib.size.h));
        }

        const char *taskbar_text = s_use_timer_window ? "Timer" : "OneShot";
        int ax = ANCHOR_OFFSET_X;
        int ay = h - ANCHOR_OFFSET_Y - ANCHOR_HEIGHT;
        int ar = ax + ANCHOR_WIDTH;
        int at = ay;
        GSize ts = graphics_text_layout_get_content_size(taskbar_text, s_text_font,
                                                          GRect(0,0,200,200),
                                                          GTextOverflowModeWordWrap,
                                                          GTextAlignmentLeft);
        int tx = ar + TEXT_OFFSET_X;
        int ty = at + TEXT_OFFSET_Y - ts.h;
        graphics_context_set_text_color(ctx, s_text_color);
        graphics_draw_text(ctx, taskbar_text, s_text_font,
                           GRect(tx, ty, ts.w, ts.h),
                           GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    }

    int base_x = w - time_size.w - TIME_TEXT_RIGHT_OFFSET;
    int base_y = h - time_size.h - TIME_TEXT_BOTTOM_OFFSET;
    graphics_context_set_text_color(ctx, s_accent_color);
    graphics_draw_text(ctx, s_time_buffer, s_text_font,
                       GRect(base_x + TIME_TEXT_OFFSET_X,
                             base_y + TIME_TEXT_OFFSET_Y,
                             time_size.w, time_size.h),
                       GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);

    int a4x = w - ANCHOR_4X6_WIDTH - ANCHOR_4X6_RIGHT_OFFSET + ANCHOR_4X6_OFFSET_X;
    int a4y = h - ANCHOR_4X6_HEIGHT - ANCHOR_4X6_BOTTOM_OFFSET + ANCHOR_4X6_OFFSET_Y;
    int rx_16 = a4x + RECT_16X16_OFFSET_FROM_ANCHOR_X;
    int ry_16 = a4y + RECT_16X16_OFFSET_FROM_ANCHOR_Y;

    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(rx_16, ry_16, 16, 16), 0, GCornerNone);
    if (s_desktop_bitmap) {
        graphics_draw_bitmap_in_rect(ctx, s_desktop_bitmap,
                                     GRect(rx_16, ry_16, 16, 16));
    }
#endif
}

// ---------- WINDOW DRAWING FUNCTIONS ----------
static int draw_text_segment(GContext *ctx, const char *text, int x, int y, GColor color) {
    GSize size = graphics_text_layout_get_content_size(text, s_text_font,
                                                       GRect(0,0,100,50),
                                                       GTextOverflowModeWordWrap,
                                                       GTextAlignmentLeft);
    if (size.w < 1) size.w = 1;
    graphics_context_set_text_color(ctx, color);
    graphics_draw_text(ctx, text, s_text_font,
                       GRect(x, y, size.w, size.h),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    return size.w;
}

static int text_width(const char *text) {
    GSize size = graphics_text_layout_get_content_size(text, s_text_font,
                                                       GRect(0,0,100,50),
                                                       GTextOverflowModeWordWrap,
                                                       GTextAlignmentLeft);
    return (size.w < 1) ? 1 : size.w;
}

static void draw_oneshot_window(GContext *ctx) {
    GRect bounds = layer_get_bounds(s_oneshot_window_layer);
    int screen_w = bounds.size.w, screen_h = bounds.size.h;
    int taskbar_total_height = LINE_HEIGHT + SEPARATOR_HEIGHT;
    int available_h = screen_h - taskbar_total_height;

    int win_x = (screen_w - ONESHOT_WIN_TOTAL_W) / 2;
    int win_y = (available_h - ONESHOT_WIN_TOTAL_H) / 2;

    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(win_x, win_y, ONESHOT_WIN_TOTAL_W, ONESHOT_WIN_TOTAL_H), 0, GCornerNone);

    GColor win_bg = GColorBlack;
#ifndef PBL_COLOR
    if (s_inverted) win_bg = GColorWhite;
#endif
    graphics_context_set_fill_color(ctx, win_bg);
    graphics_fill_rect(ctx, GRect(win_x + ONESHOT_WIN_BORDER, win_y + ONESHOT_WIN_BORDER,
                                  ONESHOT_WIN_TOTAL_W - 2*ONESHOT_WIN_BORDER,
                                  ONESHOT_WIN_TOTAL_H - 2*ONESHOT_WIN_BORDER), 0, GCornerNone);

    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    int icon_x = win_x + ONESHOT_WIN_BORDER + ONESHOT_WIN_ICON_LEFT_MARGIN;
    int icon_y = win_y + ONESHOT_WIN_BORDER + ONESHOT_WIN_ICON_TOP_MARGIN;
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(icon_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE), 0, GCornerNone);
    if (s_oneshot_window_icon_bitmap)
        graphics_draw_bitmap_in_rect(ctx, s_oneshot_window_icon_bitmap,
                                     GRect(icon_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE));

    int text_x = icon_x + ONESHOT_WIN_ICON_SIZE + 4;
    int text_y = win_y + ONESHOT_WIN_BORDER + 5;
    GSize title_size = graphics_text_layout_get_content_size("OneShot", s_text_font,
                                                             GRect(0,0,200,200),
                                                             GTextOverflowModeWordWrap,
                                                             GTextAlignmentLeft);
    graphics_context_set_text_color(ctx, s_accent_color);
    graphics_draw_text(ctx, "OneShot", s_text_font,
                       GRect(text_x, text_y, title_size.w, title_size.h),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    int close_x = win_x + ONESHOT_WIN_BORDER + ONESHOT_WIN_CONTENT_W - 2 - 16;
    int max_x = close_x - 16 - 2;
    int minimize_x = max_x - 16 - 2;
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(minimize_x, icon_y, 16, 16), 0, GCornerNone);
    if (s_minimize_button_bitmap) graphics_draw_bitmap_in_rect(ctx, s_minimize_button_bitmap, GRect(minimize_x, icon_y, 16, 16));
    graphics_fill_rect(ctx, GRect(max_x, icon_y, 16, 16), 0, GCornerNone);
    if (s_maximize_button_bitmap) graphics_draw_bitmap_in_rect(ctx, s_maximize_button_bitmap, GRect(max_x, icon_y, 16, 16));
    graphics_fill_rect(ctx, GRect(close_x, icon_y, 16, 16), 0, GCornerNone);
    if (s_close_button_bitmap) graphics_draw_bitmap_in_rect(ctx, s_close_button_bitmap, GRect(close_x, icon_y, 16, 16));
#else
    int close_x = win_x + ONESHOT_WIN_BORDER + ONESHOT_WIN_CONTENT_W - 2 - 16;
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(close_x, icon_y, 16, 16), 0, GCornerNone);
    if (s_close_button_bitmap) graphics_draw_bitmap_in_rect(ctx, s_close_button_bitmap, GRect(close_x, icon_y, 16, 16));
#endif

    int divider_y = win_y + ONESHOT_WIN_BORDER + ONESHOT_WIN_TOPBAR_H;
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(win_x + ONESHOT_WIN_BORDER, divider_y,
                                  ONESHOT_WIN_TOTAL_W - 2*ONESHOT_WIN_BORDER, 4), 0, GCornerNone);

    int content_y = divider_y + 4;
    GBitmap *content_bmp = s_oneshot_content_fade_bitmap ? s_oneshot_content_fade_bitmap : s_oneshot_window_content_bitmap;
    if (content_bmp) {
        graphics_draw_bitmap_in_rect(ctx, content_bmp,
                                     GRect(win_x + ONESHOT_WIN_BORDER, content_y,
                                           ONESHOT_WIN_CONTENT_W, ONESHOT_WIN_CONTENT_H));
    }
}

static void draw_timer_window(GContext *ctx) {
    GRect bounds = layer_get_bounds(s_oneshot_window_layer);
    int screen_w = bounds.size.w, screen_h = bounds.size.h;
    int taskbar_total_height = LINE_HEIGHT + SEPARATOR_HEIGHT;
    int available_h = screen_h - taskbar_total_height;

    int win_content_h = TIMER_WIN_TOPBAR_H + TIMER_WIN_DIVIDER_H +
                        TIMER_WIN_SYSTIME_TOP + TIMER_WIN_SYSTIME_LABEL_H +
                        TIMER_WIN_SYSTIME_RECT_GAP + TIMER_WIN_TIME_RECT_H +
                        TIMER_WIN_SYSTIME_TO_DATE_GAP + TIMER_WIN_DATE_LABEL_H +
                        TIMER_WIN_DATE_RECT_GAP + TIMER_WIN_TIME_RECT_H +
                        TIMER_WIN_DATE_RECT_BOTTOM_GAP;
    int win_total_w = TIMER_WIN_BORDER * 2 + TIMER_WIN_CONTENT_W;
    int win_total_h = TIMER_WIN_BORDER * 2 + win_content_h;
    int win_x = (screen_w - win_total_w) / 2;
    int win_y = (available_h - win_total_h) / 2;

    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(win_x, win_y, win_total_w, win_total_h), 0, GCornerNone);

    GColor win_bg = GColorBlack;
#ifndef PBL_COLOR
    if (s_inverted) win_bg = GColorWhite;
#endif
    graphics_context_set_fill_color(ctx, win_bg);
    graphics_fill_rect(ctx, GRect(win_x + TIMER_WIN_BORDER, win_y + TIMER_WIN_BORDER,
                                  TIMER_WIN_CONTENT_W, win_content_h), 0, GCornerNone);

    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    int icon_x = win_x + TIMER_WIN_BORDER + 2;
    int icon_y = win_y + TIMER_WIN_BORDER + 2;
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(icon_x, icon_y, 16, 16), 0, GCornerNone);
    if (s_timer_window_icon_bitmap)
        graphics_draw_bitmap_in_rect(ctx, s_timer_window_icon_bitmap, GRect(icon_x, icon_y, 16, 16));

    int title_x = icon_x + 16 + 4;
    int title_y = win_y + TIMER_WIN_BORDER + 5;
    GSize title_size = graphics_text_layout_get_content_size("Timer", s_text_font,
                                                             GRect(0,0,200,200),
                                                             GTextOverflowModeWordWrap,
                                                             GTextAlignmentLeft);
    graphics_context_set_text_color(ctx, s_accent_color);
    graphics_draw_text(ctx, "Timer", s_text_font,
                       GRect(title_x, title_y, title_size.w, title_size.h),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

    int close_x = win_x + TIMER_WIN_BORDER + TIMER_WIN_CONTENT_W - 2 - 16;
    int minimize_x = close_x - 16 - 2;   // minimize left of close
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(minimize_x, icon_y, 16, 16), 0, GCornerNone);
    if (s_minimize_button_bitmap) graphics_draw_bitmap_in_rect(ctx, s_minimize_button_bitmap, GRect(minimize_x, icon_y, 16, 16));
    graphics_fill_rect(ctx, GRect(close_x, icon_y, 16, 16), 0, GCornerNone);
    if (s_close_button_bitmap) graphics_draw_bitmap_in_rect(ctx, s_close_button_bitmap, GRect(close_x, icon_y, 16, 16));

    int divider_y = win_y + TIMER_WIN_BORDER + TIMER_WIN_TOPBAR_H;
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(win_x + TIMER_WIN_BORDER, divider_y, TIMER_WIN_CONTENT_W, 4), 0, GCornerNone);

    int content_top = divider_y + 4;

    // System Time label
    int systime_label_y = content_top + 6;
    graphics_context_set_text_color(ctx, s_accent_color);
    graphics_draw_text(ctx, "System Time", s_text_font,
                       GRect(win_x + TIMER_WIN_BORDER + 4, systime_label_y,
                             TIMER_WIN_CONTENT_W - 4, 12),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

    // System Time rectangle
    int systime_rect_y = systime_label_y + 12 + 3;
    int rect_x = win_x + TIMER_WIN_BORDER + 2;
    int rect_w = TIMER_WIN_CONTENT_W - 4;
    graphics_context_set_stroke_color(ctx, s_accent_color);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_rect(ctx, GRect(rect_x, systime_rect_y, rect_w, 20));

    // Correct time extraction
    time_t secs;
    uint16_t ms;
    time_ms(&secs, &ms);
    struct tm *t = localtime(&secs);

    // Determine time format (must be before date so we can use leading_zeros)
    bool use_24h = (s_clock_mode == 0) ? clock_is_24h_style() : (s_clock_mode == 2);
    bool leading_zeros = (s_leading_zeros_mode == 0) ? false :
                         (s_leading_zeros_mode == 1) ? true : use_24h;

    // Build month component
    char month_component[4];
    if (s_month_format == 0) {
        if (leading_zeros) {
            snprintf(month_component, sizeof(month_component), "%02d", t->tm_mon + 1);
        } else {
            snprintf(month_component, sizeof(month_component), "%d", t->tm_mon + 1);
        }
    } else {
        static const char *months[] = {"JAN","FEB","MAR","APR","MAY","JUN",
                                       "JUL","AUG","SEP","OCT","NOV","DEC"};
        snprintf(month_component, sizeof(month_component), "%s", months[t->tm_mon]);
    }

    // Build day component respecting leading zeros
    char day_component[3];
    if (leading_zeros) {
        snprintf(day_component, sizeof(day_component), "%02d", t->tm_mday);
    } else {
        snprintf(day_component, sizeof(day_component), "%d", t->tm_mday);
    }

    char date_str[16];  // increased for safety
    char sep = (s_date_separator == 0) ? '.' :
               (s_date_separator == 1) ? '/' :
               (s_date_separator == 2) ? '-' : ' ';

    if (s_date_format == 0)      // MM.DD.YYYY
        snprintf(date_str, sizeof(date_str), "%s%c%s%c%04d",
                 month_component, sep, day_component, sep, t->tm_year + 1900);
    else if (s_date_format == 1) // DD.MM.YYYY
        snprintf(date_str, sizeof(date_str), "%s%c%s%c%04d",
                 day_component, sep, month_component, sep, t->tm_year + 1900);
    else                          // YYYY.MM.DD
        snprintf(date_str, sizeof(date_str), "%04d%c%s%c%s",
                 t->tm_year + 1900, sep, month_component, sep, day_component);

    int hour = t->tm_hour;
    if (!use_24h) {
        hour = hour % 12;
        if (hour == 0) hour = 12;
    }

    char hour_str[4];
    if (leading_zeros) {
        snprintf(hour_str, sizeof(hour_str), "%03d", hour);
    } else {
        snprintf(hour_str, sizeof(hour_str), "%d", hour);
    }

    char min_str[3], sec_str[3], ms_str[4];
    snprintf(min_str, sizeof(min_str), "%02d", t->tm_min);
    snprintf(sec_str, sizeof(sec_str), "%02d", t->tm_sec);
    snprintf(ms_str, sizeof(ms_str), "%03d", ms);

    if (s_timer_debug_zero) {
        for (int i = 0; hour_str[i] != '\0'; i++) hour_str[i] = '0';
        for (int i = 0; min_str[i] != '\0'; i++) min_str[i] = '0';
        for (int i = 0; sec_str[i] != '\0'; i++) sec_str[i] = '0';
        for (int i = 0; ms_str[i] != '\0'; i++) ms_str[i] = '0';
    }

    int hour_digits = strlen(hour_str);

    int hour_offset;
    if (hour_digits == 3)      hour_offset = TIMER_WIN_HOUR_3DIGIT_OFFSET;
    else if (hour_digits == 2) hour_offset = TIMER_WIN_HOUR_2DIGIT_OFFSET;
    else                       hour_offset = TIMER_WIN_HOUR_1DIGIT_OFFSET;

    int colon1_offset = TIMER_WIN_COLON1_OFFSET;
    int min_offset    = TIMER_WIN_MIN_OFFSET;
    int colon2_offset = TIMER_WIN_COLON2_OFFSET;
    int sec_offset    = TIMER_WIN_SEC_OFFSET;
    int ms_offset     = TIMER_WIN_MS_OFFSET;

    int hour_x   = rect_x + rect_w - hour_offset;
    int colon1_x = rect_x + rect_w - colon1_offset;
    int min_x    = rect_x + rect_w - min_offset;
    int colon2_x = rect_x + rect_w - colon2_offset;
    int sec_x    = rect_x + rect_w - sec_offset;
    int ms_x     = rect_x + rect_w - ms_offset;

    int y_text = systime_rect_y + 4;

    GColor primary = s_accent_color;
    GColor punct_color = s_timer_punct_primary ? s_accent_color : s_variant_color;

    draw_text_segment(ctx, hour_str, hour_x, y_text, primary);
    draw_text_segment(ctx, ":", colon1_x, y_text, punct_color);
    draw_text_segment(ctx, min_str, min_x, y_text, primary);
    draw_text_segment(ctx, ":", colon2_x, y_text, punct_color);
    draw_text_segment(ctx, sec_str, sec_x, y_text, primary);
    draw_text_segment(ctx, ".", ms_x, y_text, punct_color);
    draw_text_segment(ctx, ms_str, ms_x + text_width("."), y_text, primary);

    // Current Date label
    int date_label_y = systime_rect_y + 20 + 4;
    graphics_context_set_text_color(ctx, s_accent_color);
    graphics_draw_text(ctx, "Current Date", s_text_font,
                       GRect(win_x + TIMER_WIN_BORDER + 4, date_label_y,
                             TIMER_WIN_CONTENT_W - 4, 12),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

    // Current Date rectangle
    int date_rect_y = date_label_y + 12 + 5;
    graphics_context_set_stroke_color(ctx, s_accent_color);
    graphics_draw_rect(ctx, GRect(rect_x, date_rect_y, rect_w, 20));

    // Draw actual date string, right-aligned
    graphics_context_set_text_color(ctx, s_accent_color);
    graphics_draw_text(ctx, date_str, s_text_font,
                       GRect(rect_x + 2, date_rect_y + 4,
                             rect_w - 4, 20 - 4 - 3),
                       GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
}

static void oneshot_window_update_proc(Layer *layer, GContext *ctx) {
    if (s_use_timer_window) {
        draw_timer_window(ctx);
    } else {
        draw_oneshot_window(ctx);
    }
}

// ---------- FLICK & ACCEL ----------
static void flick_timer_callback(void *context) {
    s_flick_timer = NULL;
    s_flick_visible = false;
    stop_timer_window_anim();
    if (s_oneshot_fade_animation) {
        animation_unschedule(s_oneshot_fade_animation);
        animation_destroy(s_oneshot_fade_animation);
        s_oneshot_fade_animation = NULL;
    }
    if (s_oneshot_window_layer) layer_set_hidden(s_oneshot_window_layer, true);
    if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
    if (s_show_window == 0) {
        if (s_flick_timer) { app_timer_cancel(s_flick_timer); s_flick_timer = NULL; }
        stop_timer_window_anim();
        if (s_oneshot_window_layer) layer_set_hidden(s_oneshot_window_layer, true);
        if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
        return;
    }
    if (s_show_window == 2) {
        if (s_flick_timer) { app_timer_cancel(s_flick_timer); s_flick_timer = NULL; }
        if (s_oneshot_window_layer) {
            layer_set_hidden(s_oneshot_window_layer, false);
            layer_mark_dirty(s_oneshot_window_layer);
        }
        if (s_use_timer_window) start_timer_window_anim();
        else start_oneshot_fade();
        if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
        return;
    }
    // On Flick
    s_flick_visible = true;
    if (s_flick_timer) { app_timer_cancel(s_flick_timer); s_flick_timer = NULL; }
    s_flick_timer = app_timer_register(FLICK_ONESHOT_DURATION_MS, flick_timer_callback, NULL);
    if (s_oneshot_window_layer) {
        layer_set_hidden(s_oneshot_window_layer, false);
        layer_mark_dirty(s_oneshot_window_layer);
    }
    if (s_use_timer_window) start_timer_window_anim();
    else start_oneshot_fade();
    if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
}

// ---------- MAIN WINDOW ----------
static void main_window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);

    s_wallpaper_layer = bitmap_layer_create(bounds);
    bitmap_layer_set_compositing_mode(s_wallpaper_layer, GCompOpSet);
    layer_add_child(root, bitmap_layer_get_layer(s_wallpaper_layer));
    update_wallpaper();

    s_oneshot_window_layer = layer_create(bounds);
    layer_set_update_proc(s_oneshot_window_layer, oneshot_window_update_proc);
    layer_add_child(root, s_oneshot_window_layer);

    if (s_show_window == 2) {
        s_flick_visible = true;
        layer_set_hidden(s_oneshot_window_layer, false);
        if (s_use_timer_window) start_timer_window_anim();
        else start_oneshot_fade();
    } else {
        layer_set_hidden(s_oneshot_window_layer, true);
    }

    s_overlay_layer = layer_create(bounds);
    layer_set_update_proc(s_overlay_layer, overlay_update_proc);
    layer_add_child(root, s_overlay_layer);

    s_text_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_VOLTER_9));
    if (!s_text_font) s_text_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

    s_taskbar_icon_bitmap = gbitmap_create_with_resource(s_taskbar_icon_res_id);
    s_desktop_bitmap = gbitmap_create_with_resource(s_desktop_res_id);
    s_oneshot_window_icon_bitmap = gbitmap_create_with_resource(s_oneshot_window_icon_res_id);
    s_timer_window_icon_bitmap = gbitmap_create_with_resource(s_timer_window_icon_res_id);
    s_oneshot_window_content_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ONESHOT_WINDOW);
    s_close_button_bitmap = gbitmap_create_with_resource(s_close_button_res_id);
    s_minimize_button_bitmap = gbitmap_create_with_resource(s_minimize_button_res_id);
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    s_maximize_button_bitmap = gbitmap_create_with_resource(s_maximize_button_res_id);
#endif

    s_oneshot_content_fade_bitmap = copy_bitmap_for_fade(s_oneshot_window_content_bitmap);

    update_time();
    layer_mark_dirty(s_overlay_layer);
    layer_mark_dirty(s_oneshot_window_layer);

    if (s_wallpaper_value == WALLPAPER_CUSTOM && !s_custom_wallpaper_valid) {
        schedule_image_request();
    } else if (s_wallpaper_value == WALLPAPER_CUSTOM && s_want_full_custom_image) {
        s_request_attempts = 0;
        schedule_image_request();
    }
}

static void main_window_unload(Window *window) {
    stop_timer_window_anim();
    if (s_oneshot_fade_animation) {
        animation_unschedule(s_oneshot_fade_animation);
        animation_destroy(s_oneshot_fade_animation);
        s_oneshot_fade_animation = NULL;
    }
    if (s_oneshot_content_fade_bitmap) {
        gbitmap_destroy(s_oneshot_content_fade_bitmap);
        s_oneshot_content_fade_bitmap = NULL;
    }

    if (s_wallpaper_bitmap) gbitmap_destroy(s_wallpaper_bitmap);
    if (s_wallpaper_layer) bitmap_layer_destroy(s_wallpaper_layer);
    layer_destroy(s_oneshot_window_layer);
    layer_destroy(s_overlay_layer);
    if (s_text_font) fonts_unload_custom_font(s_text_font);
    if (s_taskbar_icon_bitmap) gbitmap_destroy(s_taskbar_icon_bitmap);
    if (s_desktop_bitmap) gbitmap_destroy(s_desktop_bitmap);
    if (s_oneshot_window_icon_bitmap) gbitmap_destroy(s_oneshot_window_icon_bitmap);
    if (s_timer_window_icon_bitmap) gbitmap_destroy(s_timer_window_icon_bitmap);
    if (s_oneshot_window_content_bitmap) gbitmap_destroy(s_oneshot_window_content_bitmap);
    if (s_close_button_bitmap) gbitmap_destroy(s_close_button_bitmap);
    if (s_minimize_button_bitmap) gbitmap_destroy(s_minimize_button_bitmap);
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    if (s_maximize_button_bitmap) gbitmap_destroy(s_maximize_button_bitmap);
#endif
    cancel_transfer();
    if (s_flick_timer) {
        app_timer_cancel(s_flick_timer);
        s_flick_timer = NULL;
    }

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_FLINT)
    if (s_active_wav != NULL) {
        speaker_set_finish_callback(NULL, NULL);
        speaker_stop();
        free(s_active_wav);
        s_active_wav = NULL;
    }
#endif
}

// ---------- INBOX ----------
static void inbox_received_callback(DictionaryIterator *iter, void *context) {
    Tuple *t = dict_find(iter, KEY_REQUEST_CONFIG);
    if (t) {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"ClockMode\":%d,\"LeadingZeros\":%d,\"ShowAMPM\":%d,"
            "\"Wallpaper\":%d,\"Inverted\":%s,\"UI_Color\":\"%s\","
            "\"FlickWindow\":%d,\"Version\":\"%s\","
            "\"HourlyVibration\":%d,\"BTDisconnectVibration\":%d,\"HourlyChime\":%d,"
            "\"SilenceQuietTime\":%d,"
            "\"ActiveWindow\":%d,\"ShowWindow\":%d,\"DateFormat\":%d,\"DateSeparator\":%d,"
            "\"MonthFormat\":%d}",
            s_clock_mode, s_leading_zeros_mode, s_show_ampm_mode,
            s_wallpaper_value, s_inverted ? "true" : "false", s_ui_color,
            s_flick_window, WATCH_VERSION, s_hourly_vibration,
            s_bt_disconnect_vibration, s_hourly_chime ? 1 : 0, s_silence_quiet_time,
            s_active_window, s_show_window, s_date_format, s_date_separator,
            s_month_format);
        DictionaryIterator *out;
        app_message_outbox_begin(&out);
        dict_write_cstring(out, KEY_CONFIG_DATA, json);
        app_message_outbox_send();
        return;
    }

    if ((t = dict_find(iter, KEY_INVERTED))) {
        bool val = (t->value->int32 == 1);
        if (val != s_inverted) { s_inverted = val; save_settings(); apply_inverted(val); }
    }
    if ((t = dict_find(iter, KEY_WALLPAPER))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_wallpaper_value) {
            s_wallpaper_value = val;
            save_settings();
            if (val == WALLPAPER_CUSTOM && !s_custom_wallpaper_valid) s_request_attempts = 0;
            update_wallpaper();
        }
    }
    if ((t = dict_find(iter, KEY_CLOCK_MODE))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_clock_mode) { s_clock_mode = val; save_settings(); update_time(); }
    }
    if ((t = dict_find(iter, KEY_LEADING_ZEROS))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_leading_zeros_mode) { s_leading_zeros_mode = val; save_settings(); update_time(); }
    }
    if ((t = dict_find(iter, KEY_SHOW_AMPM))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_show_ampm_mode) { s_show_ampm_mode = val; save_settings(); update_time(); }
    }
    if ((t = dict_find(iter, KEY_UI_COLOR))) {
#ifdef PBL_COLOR
        apply_ui_color(t->value->cstring);
#endif
    }
    if ((t = dict_find(iter, KEY_FLICK_WINDOW))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_flick_window) {
            s_flick_window = val; save_settings();
            if (val == 0) { s_flick_visible = false; stop_timer_window_anim(); if (s_flick_timer) { app_timer_cancel(s_flick_timer); s_flick_timer = NULL; } if (s_oneshot_window_layer) layer_set_hidden(s_oneshot_window_layer, true); if (s_overlay_layer) layer_mark_dirty(s_overlay_layer); }
            else if (val == 2) { s_flick_visible = true; if (s_flick_timer) { app_timer_cancel(s_flick_timer); s_flick_timer = NULL; } if (s_oneshot_window_layer) { layer_set_hidden(s_oneshot_window_layer, false); if (s_use_timer_window) start_timer_window_anim(); else start_oneshot_fade(); } if (s_overlay_layer) layer_mark_dirty(s_overlay_layer); }
            else { s_flick_visible = false; if (s_flick_timer) { app_timer_cancel(s_flick_timer); s_flick_timer = NULL; } if (s_oneshot_window_layer) layer_set_hidden(s_oneshot_window_layer, true); if (s_overlay_layer) layer_mark_dirty(s_overlay_layer); }
        }
    }
    if ((t = dict_find(iter, KEY_ACTIVE_WINDOW))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        s_active_window = val;
        s_use_timer_window = (s_active_window == 1);
        save_settings();
        update_colors_and_resources();
        // Recreate bitmaps that depend on s_use_timer_window
        if (s_taskbar_icon_bitmap) {
            gbitmap_destroy(s_taskbar_icon_bitmap);
            s_taskbar_icon_bitmap = NULL;
        }
        s_taskbar_icon_bitmap = gbitmap_create_with_resource(s_taskbar_icon_res_id);
        if (s_main_window) {
            layer_mark_dirty(s_overlay_layer);
            layer_mark_dirty(s_oneshot_window_layer);
        }
    }
    
    if ((t = dict_find(iter, KEY_SHOW_WINDOW))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        s_show_window = val;
        save_settings();
        // Apply visibility regardless of previous value
        if (val == 0) { // Never
            s_flick_visible = false;
            stop_timer_window_anim();
            if (s_flick_timer) { app_timer_cancel(s_flick_timer); s_flick_timer = NULL; }
            if (s_oneshot_window_layer) layer_set_hidden(s_oneshot_window_layer, true);
            if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
        } else if (val == 2) { // Always
            s_flick_visible = true;
            if (s_flick_timer) { app_timer_cancel(s_flick_timer); s_flick_timer = NULL; }
            if (s_oneshot_window_layer) {
                layer_set_hidden(s_oneshot_window_layer, false);
                if (s_use_timer_window) start_timer_window_anim();
                else start_oneshot_fade();
            }
            if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
        } else { // On Flick (1)
            s_flick_visible = false;
            if (s_flick_timer) { app_timer_cancel(s_flick_timer); s_flick_timer = NULL; }
            if (s_oneshot_window_layer) layer_set_hidden(s_oneshot_window_layer, true);
            if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
        }
    }
    if ((t = dict_find(iter, KEY_DATE_FORMAT))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_date_format) {
            s_date_format = val;
            save_settings();
            if (s_oneshot_window_layer) layer_mark_dirty(s_oneshot_window_layer);
        }
    }
    if ((t = dict_find(iter, KEY_DATE_SEPARATOR))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_date_separator) {
            s_date_separator = val;
            save_settings();
            if (s_oneshot_window_layer) layer_mark_dirty(s_oneshot_window_layer);
        }
    }
    if ((t = dict_find(iter, KEY_MONTH_FORMAT))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_month_format) {
            s_month_format = val;
            save_settings();
            if (s_oneshot_window_layer) layer_mark_dirty(s_oneshot_window_layer);
        }
    }
    if ((t = dict_find(iter, KEY_HOURLY_VIBRATION))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_hourly_vibration) { s_hourly_vibration = val; save_settings(); }
    }
    if ((t = dict_find(iter, KEY_BT_DISCONNECT_VIBRATION))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_bt_disconnect_vibration) { s_bt_disconnect_vibration = val; save_settings(); }
    }
    if ((t = dict_find(iter, KEY_HOURLY_CHIME))) {
        bool val = (t->value->int32 == 1);
        if (val != s_hourly_chime) { s_hourly_chime = val; save_settings(); }
    }
    if ((t = dict_find(iter, KEY_SILENCE_QUIET_TIME))) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_silence_quiet_time) { s_silence_quiet_time = val; save_settings(); }
    }

    if ((t = dict_find(iter, KEY_IMAGE_DESIRED_CHECKSUM)) && s_wallpaper_value == WALLPAPER_CUSTOM) {
        uint16_t checksum = (uint16_t)t->value->int32;
        if (s_transfer_active && s_transfer_checksum != checksum) cancel_transfer();
        if (!s_custom_wallpaper_valid) { s_request_attempts = 0; schedule_image_request(); }
    }
    Tuple *begin = dict_find(iter, KEY_IMAGE_BEGIN);
    if (begin && begin->value->int32 == 1) {
        Tuple *w = dict_find(iter, KEY_IMAGE_WIDTH);
        Tuple *h = dict_find(iter, KEY_IMAGE_HEIGHT);
        Tuple *len = dict_find(iter, KEY_IMAGE_LENGTH);
        Tuple *chk = dict_find(iter, KEY_IMAGE_CHECKSUM);
        Tuple *palette = dict_find(iter, KEY_IMAGE_PALETTE);
    
        if (!w || !h || !len || !chk || !palette || palette->length != CUSTOM_PALETTE_SIZE) {
            return;
        }
    
        uint16_t width = (uint16_t)w->value->int32;
        uint16_t height = (uint16_t)h->value->int32;
        uint32_t length = (uint32_t)len->value->int32;
        uint16_t checksum = (uint16_t)chk->value->int32;
    
        begin_custom_image_transfer(width, height, length, checksum, palette->value->data);
    }
    Tuple *chunk = dict_find(iter, KEY_IMAGE_CHUNK);
    if (chunk) {
        if (!s_transfer_active) return;
        uint32_t offset = (uint32_t)dict_find(iter, KEY_IMAGE_OFFSET)->value->int32;
        if (offset + chunk->length > s_transfer_pixel_length) { fail_transfer("chunk order"); return; }
        memcpy(s_transfer_pixel_data + offset, chunk->value->data, chunk->length);
        for (uint16_t i = 0; i < chunk->length; i++)
            s_transfer_running_checksum += s_transfer_pixel_data[offset + i];
        s_transfer_received += chunk->length;
        refresh_transfer_timeout();
    }
    Tuple *end = dict_find(iter, KEY_IMAGE_END);
    if (end && end->value->int32 == 1) {
        uint32_t length = (uint32_t)dict_find(iter, KEY_IMAGE_LENGTH)->value->int32;
        uint16_t checksum = (uint16_t)dict_find(iter, KEY_IMAGE_CHECKSUM)->value->int32;
        finish_custom_image_transfer(length, checksum);
    }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
    if (s_transfer_active) fail_transfer("inbox dropped");
}

static void outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context) {
    if (needs_custom_image() && !s_transfer_active) schedule_image_request();
}

static void connection_handler(bool connected) {
    if (!connected) {
        if (s_bt_disconnect_vibration && !should_silence_vibration()) perform_vibration(s_bt_disconnect_vibration);
        if (s_transfer_active) fail_transfer("phone disconnected");
    } else if (connected && needs_custom_image() && !s_transfer_active) {
        s_request_attempts = 0;
        schedule_image_request();
    }
}

// ---------- INIT / DEINIT ----------
static void init(void) {
    load_settings();
    update_colors_and_resources();
    if (s_wallpaper_value == WALLPAPER_CUSTOM) {
        if (!load_persisted_custom_image()) APP_LOG(APP_LOG_LEVEL_INFO, "No persisted custom image");
    }

    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    window_stack_push(s_main_window, true);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    s_last_hour = t->tm_hour;

    tick_timer_service_subscribe(SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT, tick_handler);
    connection_service_subscribe((ConnectionHandlers) { .pebble_app_connection_handler = connection_handler });
    accel_tap_service_subscribe(accel_tap_handler);

    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_register_outbox_failed(outbox_failed_handler);
    app_message_open(1024, 1024);

#ifdef PBL_COLOR
    if (s_rainbow_active) start_rainbow_timer();
#endif
}

static void deinit(void) {
#ifdef PBL_COLOR
    stop_rainbow_timer();
#endif
    tick_timer_service_unsubscribe();
    connection_service_unsubscribe();
    accel_tap_service_unsubscribe();
    app_message_deregister_callbacks();
    window_destroy(s_main_window);
    destroy_custom_image();
    if (s_flick_timer) app_timer_cancel(s_flick_timer);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}