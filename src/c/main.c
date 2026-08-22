#include <pebble.h>

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
static int s_leading_zeros_mode = 2;   // 0=OFF, 1=ON, 2=Auto
static int s_show_ampm_mode = 2;       // 0=OFF, 1=ON, 2=Auto
static char s_ui_color[16] = "aa55ff";
#ifdef PBL_COLOR
static bool s_rainbow_active = false;
static bool s_rainbow_power_saving = false;
static AppTimer *s_rainbow_timer = NULL;
static uint8_t s_rainbow_power_hue_index = 0;
#endif

static GColor s_dark_color;
static GColor s_accent_color;
static GColor s_text_color;
static GColor s_background_color;
static uint32_t s_lightbulb_res_id;
static uint32_t s_desktop_res_id;
static uint32_t s_oneshot_window_icon_res_id;
static uint32_t s_close_button_res_id;
#ifdef PBL_PLATFORM_EMERY
static uint32_t s_minimize_button_res_id;
static uint32_t s_maximize_button_res_id;
#endif

static Window *s_main_window;
static Layer *s_overlay_layer;
static Layer *s_oneshot_window_layer;
static BitmapLayer *s_wallpaper_layer;
static GBitmap *s_wallpaper_bitmap;
static GBitmap *s_lightbulb_bitmap;
static GBitmap *s_desktop_bitmap;
static GBitmap *s_oneshot_window_icon_bitmap;
static GBitmap *s_oneshot_window_content_bitmap;
static GBitmap *s_close_button_bitmap;
#ifdef PBL_PLATFORM_EMERY
static GBitmap *s_minimize_button_bitmap;
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
static int s_flick_window = 1;        // 0=Never, 1=On Flick, 2=Always
static bool s_flick_visible = false;  // current visibility state of the flick window
static AppTimer *s_flick_timer = NULL;

// Forward declarations
static void schedule_image_request(void);
static void update_wallpaper(void);
static bool needs_custom_image(void);
static void update_time(void);
static void overlay_update_proc(Layer *layer, GContext *ctx);
static void oneshot_window_update_proc(Layer *layer, GContext *ctx);
static void accel_tap_handler(AccelAxisType axis, int32_t direction);

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
    s_lightbulb_res_id = RESOURCE_ID_LIGHTBULB;
    s_desktop_res_id = RESOURCE_ID_SHOW_DESKTOP;
    s_oneshot_window_icon_res_id = RESOURCE_ID_ONESHOT_WINDOW_ICON;
    s_close_button_res_id = RESOURCE_ID_CLOSE_BUTTON;
#ifdef PBL_PLATFORM_EMERY
    s_minimize_button_res_id = RESOURCE_ID_MINIMIZE_BUTTON;
    s_maximize_button_res_id = RESOURCE_ID_MAXIMIZE_BUTTON;
#endif
#else
    if (s_inverted) {
        s_dark_color = GColorWhite;
        s_accent_color = GColorBlack;
        s_text_color = GColorWhite;
        s_background_color = GColorClear;
        s_lightbulb_res_id = RESOURCE_ID_LIGHTBULB_INVERTED;
        s_desktop_res_id = RESOURCE_ID_SHOW_DESKTOP_INVERTED;
        s_oneshot_window_icon_res_id = RESOURCE_ID_ONESHOT_WINDOW_ICON_INVERTED;
#ifdef RESOURCE_ID_CLOSE_BUTTON_INVERTED
        s_close_button_res_id = RESOURCE_ID_CLOSE_BUTTON_INVERTED;
#else
        s_close_button_res_id = RESOURCE_ID_CLOSE_BUTTON;
#endif
    } else {
        s_dark_color = GColorBlack;
        s_accent_color = GColorWhite;
        s_text_color = GColorBlack;
        s_background_color = GColorClear;
        s_lightbulb_res_id = RESOURCE_ID_LIGHTBULB;
        s_desktop_res_id = RESOURCE_ID_SHOW_DESKTOP;
        s_oneshot_window_icon_res_id = RESOURCE_ID_ONESHOT_WINDOW_ICON;
        s_close_button_res_id = RESOURCE_ID_CLOSE_BUTTON;
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

    // Migrate old Leading Zeros boolean (key 104) to new mode
    if (persist_exists(104)) {
        bool old = persist_read_bool(104);
        int newMode = old ? 1 : 0;
        persist_write_int(PERSIST_LEADING_ZEROS_MODE, newMode);
        persist_delete(104);
    }
    if (persist_exists(PERSIST_LEADING_ZEROS_MODE))
        s_leading_zeros_mode = persist_read_int(PERSIST_LEADING_ZEROS_MODE);
    else
        s_leading_zeros_mode = 2; // default Auto

    // Migrate old Show AM/PM boolean (key 105) to new mode
    if (persist_exists(105)) {
        bool old = persist_read_bool(105);
        int newMode = old ? 1 : 0;
        persist_write_int(PERSIST_SHOW_AMPM_MODE, newMode);
        persist_delete(105);
    }
    if (persist_exists(PERSIST_SHOW_AMPM_MODE))
        s_show_ampm_mode = persist_read_int(PERSIST_SHOW_AMPM_MODE);
    else
        s_show_ampm_mode = 2; // default Auto

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

    if (persist_exists(PERSIST_FLICK_WINDOW))
        s_flick_window = persist_read_int(PERSIST_FLICK_WINDOW);
    else
        s_flick_window = 1; // default On Flick
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

    // Effective leading zeros
    bool effective_leading_zeros;
    if (s_leading_zeros_mode == 0)      // OFF
        effective_leading_zeros = false;
    else if (s_leading_zeros_mode == 1) // ON
        effective_leading_zeros = true;
    else                                // Auto
        effective_leading_zeros = use_24h;   // ON for 24h, OFF for 12h

    // Effective AM/PM display
    bool effective_show_ampm;
    if (s_show_ampm_mode == 0)          // OFF
        effective_show_ampm = false;
    else if (s_show_ampm_mode == 1)     // ON
        effective_show_ampm = true;
    else                                // Auto
        effective_show_ampm = !use_24h; // ON for 12h, OFF for 24h

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
    uint8_t persist_palette[CUSTOM_PALETTE_SIZE];
    uint32_t persist_pixel_len;
    uint16_t persist_width, persist_height;
    uint8_t *persist_pixels = NULL;

    bool use_full = full_persist_possible();

    if (use_full) {
        persist_width = PBL_DISPLAY_WIDTH;
        persist_height = PBL_DISPLAY_HEIGHT;
        persist_pixel_len = pixel_len;
        persist_pixels = malloc(persist_pixel_len);
        if (!persist_pixels) return false;
        memcpy(persist_pixels, pixel_data, pixel_len);
        memcpy(persist_palette, palette, CUSTOM_PALETTE_SIZE);
    } else {
#ifdef PBL_COLOR
        persist_width = PBL_DISPLAY_WIDTH / 2;
        persist_height = PBL_DISPLAY_HEIGHT / 2;
        uint16_t half_row_bytes = (persist_width + 1) / 2;
        persist_pixel_len = (uint32_t)half_row_bytes * persist_height;
        persist_pixels = malloc(persist_pixel_len);
        if (!persist_pixels) return false;

        uint16_t full_row_bytes = (PBL_DISPLAY_WIDTH + 1) / 2;
        for (int y = 0; y < persist_height; ++y) {
            for (int x = 0; x < persist_width; ++x) {
                int src_x = x * 2;
                int src_y = y * 2;
                int src_byte = src_y * full_row_bytes + (src_x / 2);
                uint8_t src_pair = pixel_data[src_byte];
                uint8_t index = (src_x & 1) ? (src_pair & 0x0F) : (src_pair >> 4);
                int dst_byte = y * half_row_bytes + (x / 2);
                if ((x & 1) == 0) {
                    persist_pixels[dst_byte] = index << 4;
                } else {
                    persist_pixels[dst_byte] |= index;
                }
            }
        }
        memcpy(persist_palette, palette, CUSTOM_PALETTE_SIZE);
#else
        return false;
#endif
    }

    uint32_t total_len = persist_pixel_len + CUSTOM_PALETTE_SIZE;
    if (!size_fits(total_len)) {
        free(persist_pixels);
        APP_LOG(APP_LOG_LEVEL_WARNING, "Even half-res doesn't fit");
        return false;
    }

    delete_photo_storage();

    uint8_t *combined = malloc(total_len);
    if (!combined) {
        free(persist_pixels);
        return false;
    }
    memcpy(combined, persist_pixels, persist_pixel_len);
    memcpy(combined + persist_pixel_len, persist_palette, CUSTOM_PALETTE_SIZE);
    free(persist_pixels);

    uint16_t combined_checksum = image_checksum(combined, total_len);

    uint32_t offset = 0;
    while (offset < total_len) {
        uint16_t chunk_len = (uint16_t)MIN(PHOTO_CHUNK_SIZE, total_len - offset);
        int result = persist_write_data(PERSIST_PHOTO_DATA + offset / PHOTO_CHUNK_SIZE,
                                        combined + offset, chunk_len);
        if (result != chunk_len) {
            APP_LOG(APP_LOG_LEVEL_WARNING, "Photo persist chunk failed at %lu", (unsigned long)offset);
            delete_photo_storage();
            free(combined);
            return false;
        }
        offset += chunk_len;
    }

    CustomPhotoMetadata meta = {
        .magic = PHOTO_MAGIC,
        .version = PHOTO_VERSION,
        .width = persist_width,
        .height = persist_height,
        .length = total_len,
        .checksum = combined_checksum,
        .palette_length = CUSTOM_PALETTE_SIZE
    };
    if (persist_write_data(PERSIST_PHOTO_META, &meta, sizeof(meta)) != (int)sizeof(meta)) {
        delete_photo_storage();
        free(combined);
        return false;
    }

    free(combined);
    return true;
}

static bool load_persisted_custom_image(void) {
    CustomPhotoMetadata meta;
    if (persist_read_data(PERSIST_PHOTO_META, &meta, sizeof(meta)) != (int)sizeof(meta) ||
        meta.magic != PHOTO_MAGIC || meta.version != PHOTO_VERSION ||
        meta.palette_length != CUSTOM_PALETTE_SIZE) {
        return false;
    }

    bool is_full = (meta.width == PBL_DISPLAY_WIDTH && meta.height == PBL_DISPLAY_HEIGHT);
    bool is_half = false;
#ifdef PBL_COLOR
    is_half = (meta.width == PBL_DISPLAY_WIDTH / 2 && meta.height == PBL_DISPLAY_HEIGHT / 2);
#endif
    if (!is_full && !is_half) return false;

    uint32_t total_len = meta.length;
    if (total_len <= meta.palette_length) return false;

    uint8_t *combined = malloc(total_len);
    if (!combined) return false;

    uint32_t offset = 0;
    while (offset < total_len) {
        uint16_t chunk_len = (uint16_t)MIN(PHOTO_CHUNK_SIZE, total_len - offset);
        if (persist_read_data(PERSIST_PHOTO_DATA + offset / PHOTO_CHUNK_SIZE,
                              combined + offset, chunk_len) != chunk_len) {
            free(combined);
            return false;
        }
        offset += chunk_len;
    }

    if (image_checksum(combined, total_len) != meta.checksum) {
        free(combined);
        return false;
    }

    uint32_t persist_pixel_len = total_len - meta.palette_length;
    uint8_t *persist_pixels = combined;
    uint8_t *persist_palette = combined + persist_pixel_len;

    GBitmap *full_bitmap = gbitmap_create_blank(GSize(PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT),
                                                CUSTOM_BITMAP_FORMAT);
    if (!full_bitmap) {
        free(combined);
        return false;
    }

    if (is_full) {
        memcpy(gbitmap_get_data(full_bitmap), persist_pixels, persist_pixel_len);
        memcpy(gbitmap_get_palette(full_bitmap), persist_palette, CUSTOM_PALETTE_SIZE);
        destroy_custom_image();
        s_custom_wallpaper_bitmap = full_bitmap;
        s_custom_wallpaper_valid = true;
        s_custom_wallpaper_is_low_depth = false;
        s_want_full_custom_image = false;
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
        destroy_custom_image();
        s_custom_wallpaper_bitmap = full_bitmap;
        s_custom_wallpaper_valid = true;
        s_custom_wallpaper_is_low_depth = true;
        s_want_full_custom_image = true;
#else
        gbitmap_destroy(full_bitmap);
        free(combined);
        return false;
#endif
    }

    free(combined);
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
        APP_LOG(APP_LOG_LEVEL_ERROR, "Custom image length mismatch: %lu vs %lu",
                (unsigned long)length, (unsigned long)pixel_length);
        gbitmap_destroy(bitmap);
        return;
    }

    cancel_transfer();

    s_transfer_pixel_data = malloc(length);
    if (!s_transfer_pixel_data) {
        gbitmap_destroy(bitmap);
        APP_LOG(APP_LOG_LEVEL_ERROR, "Cannot allocate custom image buffer");
        return;
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

    bool persisted = persist_custom_image(s_transfer_pixel_data, s_transfer_pixel_length,
                                          s_transfer_palette, CUSTOM_PALETTE_SIZE);
    if (persisted) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Custom image persisted");
    } else {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image persistence failed");
    }

    destroy_custom_image();
    s_custom_wallpaper_bitmap = s_transfer_bitmap;
    s_custom_wallpaper_valid = true;
    s_custom_wallpaper_is_low_depth = false;
    s_want_full_custom_image = false;
    s_transfer_bitmap = NULL;
    s_transfer_active = false;
    cancel_transfer_timer();

    free(s_transfer_pixel_data);
    s_transfer_pixel_data = NULL;

    APP_LOG(APP_LOG_LEVEL_INFO, "Custom image ready");
    update_wallpaper();
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

    if (s_lightbulb_bitmap) {
        gbitmap_destroy(s_lightbulb_bitmap);
        s_lightbulb_bitmap = NULL;
    }
    s_lightbulb_bitmap = gbitmap_create_with_resource(s_lightbulb_res_id);

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

    if (s_close_button_bitmap) {
        gbitmap_destroy(s_close_button_bitmap);
        s_close_button_bitmap = NULL;
    }
    s_close_button_bitmap = gbitmap_create_with_resource(s_close_button_res_id);

#ifdef PBL_PLATFORM_EMERY
    if (s_minimize_button_bitmap) {
        gbitmap_destroy(s_minimize_button_bitmap);
        s_minimize_button_bitmap = NULL;
    }
    s_minimize_button_bitmap = gbitmap_create_with_resource(s_minimize_button_res_id);

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
        if (hex[0] == '#') hex++;   // Strip leading '#'

        strncpy(s_ui_color, hex, sizeof(s_ui_color)-1);
        s_ui_color[sizeof(s_ui_color)-1] = '\0';
        s_rainbow_active = false;
        stop_rainbow_timer();
        s_accent_color = hex_to_gcolor(hex);
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
#define RECTANGLE_OFFSET_X 2
#define RECTANGLE_OFFSET_Y 2
#ifdef PBL_PLATFORM_EMERY
    #define RECTANGLE_WIDTH 90
#else
    #define RECTANGLE_WIDTH 68
#endif
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
#define LIGHTBULB_OFFSET_X 4
#define LIGHTBULB_OFFSET_Y 4

// ---------- ONESHOT WINDOW CONSTANTS ----------
#define ONESHOT_WIN_BORDER           2
#ifdef PBL_PLATFORM_EMERY
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

    if (s_oneshot_window_layer) {
        layer_mark_dirty(s_oneshot_window_layer);
    }
}

static void oneshot_fade_setup(Animation *animation) {
    oneshot_fade_update(animation, 0);
}

static void oneshot_fade_teardown(Animation *animation) {
    // Nothing to do
}

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

    // Reset palette to black before scheduling
    oneshot_fade_update(NULL, 0);

    s_oneshot_fade_animation = animation_create();
    animation_set_duration(s_oneshot_fade_animation, 1000);
    animation_set_curve(s_oneshot_fade_animation, AnimationCurveEaseOut);
    animation_set_implementation(s_oneshot_fade_animation, &s_oneshot_fade_impl);
    animation_set_handlers(s_oneshot_fade_animation, s_oneshot_fade_handlers, NULL);
    s_oneshot_fade_finished = false;
    animation_schedule(s_oneshot_fade_animation);
}

static void flick_timer_callback(void *context) {
    s_flick_timer = NULL;
    s_flick_visible = false;
    if (s_oneshot_fade_animation) {
        animation_unschedule(s_oneshot_fade_animation);
        animation_destroy(s_oneshot_fade_animation);
        s_oneshot_fade_animation = NULL;
    }
    if (s_oneshot_window_layer) {
        layer_set_hidden(s_oneshot_window_layer, true);
        layer_mark_dirty(s_oneshot_window_layer);
    }
    // Mark overlay dirty to update taskbar button visibility
    if (s_overlay_layer) {
        layer_mark_dirty(s_overlay_layer);
    }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
    if (s_flick_window == 0) {
        // Never: hide and cancel any timer
        if (s_flick_timer) {
            app_timer_cancel(s_flick_timer);
            s_flick_timer = NULL;
        }
        if (s_oneshot_window_layer) {
            layer_set_hidden(s_oneshot_window_layer, true);
        }
        if (s_overlay_layer) {
            layer_mark_dirty(s_overlay_layer);
        }
        return;
    }

    if (s_flick_window == 2) {
        // Always: show permanently, cancel any timer
        if (s_flick_timer) {
            app_timer_cancel(s_flick_timer);
            s_flick_timer = NULL;
        }
        if (s_oneshot_window_layer) {
            layer_set_hidden(s_oneshot_window_layer, false);
            layer_mark_dirty(s_oneshot_window_layer);
        }
        if (s_overlay_layer) {
            layer_mark_dirty(s_overlay_layer);
        }
        return;
    }

    // s_flick_window == 1: On Flick with 10-second timeout
    s_flick_visible = true;

    // Cancel existing timer, if any
    if (s_flick_timer) {
        app_timer_cancel(s_flick_timer);
        s_flick_timer = NULL;
    }

    // Start / restart 10-second hide timer
    s_flick_timer = app_timer_register(FLICK_ONESHOT_DURATION_MS, flick_timer_callback, NULL);

    if (s_oneshot_window_layer) {
        layer_set_hidden(s_oneshot_window_layer, false);
        layer_mark_dirty(s_oneshot_window_layer);
    }
    if (s_overlay_layer) {
        layer_mark_dirty(s_overlay_layer);
    }

    start_oneshot_fade();
}

// ---------- UI UPDATE PROCS ----------
static void overlay_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_unobstructed_bounds(layer);
    int w = bounds.size.w;
    int h = bounds.size.h;

    graphics_context_set_compositing_mode(ctx, GCompOpSet);

    int colored_y = h - LINE_HEIGHT;
    int separator_y = colored_y - SEPARATOR_HEIGHT;

    graphics_context_set_fill_color(ctx, s_dark_color);
    graphics_fill_rect(ctx, GRect(0, colored_y, w, LINE_HEIGHT), 0, GCornerNone);

    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(0, separator_y, w, SEPARATOR_HEIGHT), 0, GCornerNone);

    // Only draw the OneShot taskbar button and text when the window is visible
    if (s_flick_visible) {
        int rx = RECTANGLE_OFFSET_X;
        int ry = h - RECTANGLE_OFFSET_Y - RECTANGLE_HEIGHT;
        graphics_context_set_fill_color(ctx, s_accent_color);
        graphics_fill_rect(ctx, GRect(rx, ry, RECTANGLE_WIDTH, RECTANGLE_HEIGHT), 0, GCornerNone);

        if (s_lightbulb_bitmap) {
            GRect ib = gbitmap_get_bounds(s_lightbulb_bitmap);
            graphics_draw_bitmap_in_rect(ctx, s_lightbulb_bitmap,
                                         GRect(rx + LIGHTBULB_OFFSET_X,
                                               ry + LIGHTBULB_OFFSET_Y,
                                               ib.size.w, ib.size.h));
        }

        int ax = ANCHOR_OFFSET_X;
        int ay = h - ANCHOR_OFFSET_Y - ANCHOR_HEIGHT;
        int ar = ax + ANCHOR_WIDTH;
        int at = ay;
        GSize ts = graphics_text_layout_get_content_size(TEXT_STRING, s_text_font,
                                                          GRect(0,0,200,200),
                                                          GTextOverflowModeWordWrap,
                                                          GTextAlignmentLeft);
        int tx = ar + TEXT_OFFSET_X;
        int ty = at + TEXT_OFFSET_Y - ts.h;
        graphics_context_set_text_color(ctx, s_text_color);
        graphics_draw_text(ctx, TEXT_STRING, s_text_font,
                           GRect(tx, ty, ts.w, ts.h),
                           GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    }

    // Time and desktop icon are always drawn
    update_time();
    GSize time_size = graphics_text_layout_get_content_size(s_time_buffer, s_text_font,
                                                            GRect(0,0,200,200),
                                                            GTextOverflowModeWordWrap,
                                                            GTextAlignmentLeft);
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
}

static void oneshot_window_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    int screen_w = bounds.size.w;
    int screen_h = bounds.size.h;

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
    graphics_fill_rect(ctx, GRect(win_x + ONESHOT_WIN_BORDER,
                                  win_y + ONESHOT_WIN_BORDER,
                                  ONESHOT_WIN_TOTAL_W - 2 * ONESHOT_WIN_BORDER,
                                  ONESHOT_WIN_TOTAL_H - 2 * ONESHOT_WIN_BORDER), 0, GCornerNone);

    graphics_context_set_compositing_mode(ctx, GCompOpSet);

    int icon_x = win_x + ONESHOT_WIN_BORDER + ONESHOT_WIN_ICON_LEFT_MARGIN;
    int icon_y = win_y + ONESHOT_WIN_BORDER + ONESHOT_WIN_ICON_TOP_MARGIN;
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(icon_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE), 0, GCornerNone);

    if (s_oneshot_window_icon_bitmap) {
        graphics_draw_bitmap_in_rect(ctx, s_oneshot_window_icon_bitmap,
                                     GRect(icon_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE));
    }

    int text_gap = 4;
    int text_x = icon_x + ONESHOT_WIN_ICON_SIZE + text_gap;
    int text_y = win_y + ONESHOT_WIN_BORDER + 5;
    GSize title_size = graphics_text_layout_get_content_size("OneShot", s_text_font,
                                                              GRect(0,0,200,200),
                                                              GTextOverflowModeWordWrap,
                                                              GTextAlignmentLeft);
    graphics_context_set_text_color(ctx, s_accent_color);
    graphics_draw_text(ctx, "OneShot", s_text_font,
                       GRect(text_x, text_y, title_size.w, title_size.h),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

#ifdef PBL_PLATFORM_EMERY
    int close_x = win_x + ONESHOT_WIN_BORDER + ONESHOT_WIN_CONTENT_W
                  - ONESHOT_WIN_CLOSE_RIGHT_MARGIN - ONESHOT_WIN_ICON_SIZE;
    int maximize_x = close_x - ONESHOT_WIN_ICON_SIZE - 2;
    int minimize_x = maximize_x - ONESHOT_WIN_ICON_SIZE - 2;

    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(minimize_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE), 0, GCornerNone);
    if (s_minimize_button_bitmap) {
        graphics_draw_bitmap_in_rect(ctx, s_minimize_button_bitmap,
                                     GRect(minimize_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE));
    }

    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(maximize_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE), 0, GCornerNone);
    if (s_maximize_button_bitmap) {
        graphics_draw_bitmap_in_rect(ctx, s_maximize_button_bitmap,
                                     GRect(maximize_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE));
    }

    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(close_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE), 0, GCornerNone);
    if (s_close_button_bitmap) {
        graphics_draw_bitmap_in_rect(ctx, s_close_button_bitmap,
                                     GRect(close_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE));
    }
#else
    int close_x = win_x + ONESHOT_WIN_BORDER + ONESHOT_WIN_CONTENT_W
                  - ONESHOT_WIN_CLOSE_RIGHT_MARGIN - ONESHOT_WIN_ICON_SIZE;
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(close_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE), 0, GCornerNone);
    if (s_close_button_bitmap) {
        graphics_draw_bitmap_in_rect(ctx, s_close_button_bitmap,
                                     GRect(close_x, icon_y, ONESHOT_WIN_ICON_SIZE, ONESHOT_WIN_ICON_SIZE));
    }
#endif

    int divider_y = win_y + ONESHOT_WIN_BORDER + ONESHOT_WIN_TOPBAR_H;
    graphics_context_set_fill_color(ctx, s_accent_color);
    graphics_fill_rect(ctx, GRect(win_x + ONESHOT_WIN_BORDER, divider_y,
                                  ONESHOT_WIN_TOTAL_W - 2 * ONESHOT_WIN_BORDER,
                                  ONESHOT_WIN_DIVIDER_H), 0, GCornerNone);

    int content_y = divider_y + ONESHOT_WIN_DIVIDER_H;
    GBitmap *content_bmp = s_oneshot_content_fade_bitmap
                           ? s_oneshot_content_fade_bitmap
                           : s_oneshot_window_content_bitmap;
    if (content_bmp) {
        graphics_draw_bitmap_in_rect(ctx, content_bmp,
                                     GRect(win_x + ONESHOT_WIN_BORDER, content_y,
                                           ONESHOT_WIN_CONTENT_W, ONESHOT_WIN_CONTENT_H));
    }
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

    // Initial visibility based on setting
    if (s_flick_window == 2) {
        // Always visible
        s_flick_visible = true;
        layer_set_hidden(s_oneshot_window_layer, false);
        start_oneshot_fade();
    } else {
        // Hidden for Never and On Flick
        layer_set_hidden(s_oneshot_window_layer, true);
    }

    s_overlay_layer = layer_create(bounds);
    layer_set_update_proc(s_overlay_layer, overlay_update_proc);
    layer_add_child(root, s_overlay_layer);

    s_text_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_VOLTER_9));
    if (!s_text_font) s_text_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

    s_lightbulb_bitmap = gbitmap_create_with_resource(s_lightbulb_res_id);
    s_desktop_bitmap = gbitmap_create_with_resource(s_desktop_res_id);
    s_oneshot_window_icon_bitmap = gbitmap_create_with_resource(s_oneshot_window_icon_res_id);
    s_oneshot_window_content_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ONESHOT_WINDOW);
    s_close_button_bitmap = gbitmap_create_with_resource(s_close_button_res_id);
#ifdef PBL_PLATFORM_EMERY
    s_minimize_button_bitmap = gbitmap_create_with_resource(s_minimize_button_res_id);
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
    if (s_lightbulb_bitmap) gbitmap_destroy(s_lightbulb_bitmap);
    if (s_desktop_bitmap) gbitmap_destroy(s_desktop_bitmap);
    if (s_oneshot_window_icon_bitmap) gbitmap_destroy(s_oneshot_window_icon_bitmap);
    if (s_oneshot_window_content_bitmap) gbitmap_destroy(s_oneshot_window_content_bitmap);
    if (s_close_button_bitmap) gbitmap_destroy(s_close_button_bitmap);
#ifdef PBL_PLATFORM_EMERY
    if (s_minimize_button_bitmap) gbitmap_destroy(s_minimize_button_bitmap);
    if (s_maximize_button_bitmap) gbitmap_destroy(s_maximize_button_bitmap);
#endif
    cancel_transfer();
    if (s_flick_timer) {
        app_timer_cancel(s_flick_timer);
        s_flick_timer = NULL;
    }
}

// ---------- INBOX CALLBACK ----------
static void inbox_received_callback(DictionaryIterator *iter, void *context) {
    Tuple *t;

    t = dict_find(iter, KEY_REQUEST_CONFIG);
    if (t) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Watch asked for config");
        char json[256];
        snprintf(json, sizeof(json),
            "{\"ClockMode\":%d,\"LeadingZeros\":%d,\"ShowAMPM\":%d,"
            "\"Wallpaper\":%d,\"Inverted\":%s,\"UI_Color\":\"%s\","
            "\"FlickWindow\":%d}",
            s_clock_mode,
            s_leading_zeros_mode,
            s_show_ampm_mode,
            s_wallpaper_value,
            s_inverted ? "true" : "false",
            s_ui_color,
            s_flick_window
        );
        DictionaryIterator *out;
        app_message_outbox_begin(&out);
        dict_write_cstring(out, KEY_CONFIG_DATA, json);
        app_message_outbox_send();
        APP_LOG(APP_LOG_LEVEL_INFO, "Sent config: %s", json);
        return;
    }

    t = dict_find(iter, KEY_INVERTED);
    if (t) {
        bool val = (t->value->int32 == 1);
        if (val != s_inverted) {
            s_inverted = val;
            save_settings();
            apply_inverted(val);
            APP_LOG(APP_LOG_LEVEL_INFO, "Inverted set to %d", val);
        }
    }

    t = dict_find(iter, KEY_WALLPAPER);
    if (t) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_wallpaper_value) {
            s_wallpaper_value = val;
            save_settings();
            if (val == WALLPAPER_CUSTOM && !s_custom_wallpaper_valid) {
                s_request_attempts = 0;
            }
            update_wallpaper();
            APP_LOG(APP_LOG_LEVEL_INFO, "Wallpaper set to %d", val);
        }
    }

    t = dict_find(iter, KEY_CLOCK_MODE);
    if (t) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_clock_mode) {
            s_clock_mode = val;
            save_settings();
            update_time();
            APP_LOG(APP_LOG_LEVEL_INFO, "ClockMode set to %d", val);
        }
    }

    t = dict_find(iter, KEY_LEADING_ZEROS);
    if (t) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_leading_zeros_mode) {
            s_leading_zeros_mode = val;
            save_settings();
            update_time();
            APP_LOG(APP_LOG_LEVEL_INFO, "LeadingZeros mode set to %d", val);
        }
    }

    t = dict_find(iter, KEY_SHOW_AMPM);
    if (t) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_show_ampm_mode) {
            s_show_ampm_mode = val;
            save_settings();
            update_time();
            APP_LOG(APP_LOG_LEVEL_INFO, "ShowAMPM mode set to %d", val);
        }
    }

    t = dict_find(iter, KEY_UI_COLOR);
    if (t) {
#ifdef PBL_COLOR
        const char *hex = t->value->cstring;
        apply_ui_color(hex);
        APP_LOG(APP_LOG_LEVEL_INFO, "UI_Color set to %s", hex);
#else
        APP_LOG(APP_LOG_LEVEL_INFO, "UI_Color ignored on BW watch");
#endif
    }

    t = dict_find(iter, KEY_FLICK_WINDOW);
    if (t) {
        int val = (t->type == TUPLE_CSTRING) ? atoi(t->value->cstring) : t->value->int32;
        if (val != s_flick_window) {
            s_flick_window = val;
            save_settings();
            // Apply new mode immediately
            if (s_flick_window == 0) {
                // Never: hide and cancel timer
                s_flick_visible = false;
                if (s_flick_timer) {
                    app_timer_cancel(s_flick_timer);
                    s_flick_timer = NULL;
                }
                if (s_oneshot_window_layer) {
                    layer_set_hidden(s_oneshot_window_layer, true);
                }
                if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
            } else if (s_flick_window == 2) {
                // Always: show and cancel timer
                if (s_flick_timer) {
                    app_timer_cancel(s_flick_timer);
                    s_flick_timer = NULL;
                }
                s_flick_visible = true;
                if (s_oneshot_window_layer) {
                    layer_set_hidden(s_oneshot_window_layer, false);
                    start_oneshot_fade();
                }
                if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
            } else {
                // On Flick: hide until next flick
                if (s_flick_timer) {
                    app_timer_cancel(s_flick_timer);
                    s_flick_timer = NULL;
                }
                s_flick_visible = false;
                if (s_oneshot_window_layer) {
                    layer_set_hidden(s_oneshot_window_layer, true);
                }
                if (s_overlay_layer) layer_mark_dirty(s_overlay_layer);
            }
        }
    }

    // Custom image transfer
    t = dict_find(iter, KEY_IMAGE_DESIRED_CHECKSUM);
    if (t && s_wallpaper_value == WALLPAPER_CUSTOM) {
        uint16_t checksum = (uint16_t)t->value->int32;
        if (s_transfer_active && s_transfer_checksum != checksum) {
            cancel_transfer();
        }
        if (!s_custom_wallpaper_valid) {
            s_request_attempts = 0;
            schedule_image_request();
        }
    }

    Tuple *begin = dict_find(iter, KEY_IMAGE_BEGIN);
    if (begin && begin->value->int32 == 1) {
        uint16_t width = (uint16_t)dict_find(iter, KEY_IMAGE_WIDTH)->value->int32;
        uint16_t height = (uint16_t)dict_find(iter, KEY_IMAGE_HEIGHT)->value->int32;
        uint32_t length = (uint32_t)dict_find(iter, KEY_IMAGE_LENGTH)->value->int32;
        uint16_t checksum = (uint16_t)dict_find(iter, KEY_IMAGE_CHECKSUM)->value->int32;
        Tuple *palette = dict_find(iter, KEY_IMAGE_PALETTE);

        if (palette && palette->type == TUPLE_BYTE_ARRAY &&
            palette->length == CUSTOM_PALETTE_SIZE) {
            begin_custom_image_transfer(width, height, length, checksum, palette->value->data);
        } else {
            APP_LOG(APP_LOG_LEVEL_WARNING, "Custom image begin missing palette");
        }
    }

    Tuple *chunk = dict_find(iter, KEY_IMAGE_CHUNK);
    if (chunk) {
        if (!s_transfer_active) return;
        uint32_t offset = (uint32_t)dict_find(iter, KEY_IMAGE_OFFSET)->value->int32;
        if (chunk->type != TUPLE_BYTE_ARRAY || offset > s_transfer_received ||
            offset + chunk->length > s_transfer_pixel_length) {
            fail_transfer("chunk order");
            return;
        }
        if (offset < s_transfer_received) {
            if (offset + chunk->length <= s_transfer_received &&
                memcmp(s_transfer_pixel_data + offset, chunk->value->data, chunk->length) == 0) {
                refresh_transfer_timeout();
                return;
            }
            fail_transfer("chunk overlap");
            return;
        }
        memcpy(s_transfer_pixel_data + s_transfer_received, chunk->value->data, chunk->length);
        for (uint16_t i = 0; i < chunk->length; i++) {
            s_transfer_running_checksum =
                (uint16_t)(s_transfer_running_checksum +
                           s_transfer_pixel_data[s_transfer_received + i]);
        }
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
    APP_LOG(APP_LOG_LEVEL_WARNING, "AppMessage dropped: %d", reason);
    if (s_transfer_active) fail_transfer("inbox dropped");
}

static void outbox_failed_handler(DictionaryIterator *iterator,
                                  AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "AppMessage send failed: %d", reason);
    if (needs_custom_image() && !s_transfer_active) {
        schedule_image_request();
    }
}

static void connection_handler(bool connected) {
    if (!connected && s_transfer_active) {
        fail_transfer("phone disconnected");
        return;
    }
    if (connected && needs_custom_image() && !s_transfer_active) {
        s_request_attempts = 0;
        schedule_image_request();
    }
}

static void init(void) {
    load_settings();
    update_colors_and_resources();

    if (s_wallpaper_value == WALLPAPER_CUSTOM) {
        if (!load_persisted_custom_image()) {
            APP_LOG(APP_LOG_LEVEL_INFO, "No persisted custom image");
        }
    }

    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    window_stack_push(s_main_window, true);

    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
    connection_service_subscribe((ConnectionHandlers) {
        .pebble_app_connection_handler = connection_handler,
    });
    accel_tap_service_subscribe(accel_tap_handler);

    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_register_outbox_failed(outbox_failed_handler);
    app_message_open(1024, 256);

#ifdef PBL_COLOR
    if (s_rainbow_active) {
        start_rainbow_timer();
    }
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
    if (s_flick_timer) {
        app_timer_cancel(s_flick_timer);
        s_flick_timer = NULL;
    }
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}