#include "ui/ui_fonts.h"

#include "esp_log.h"

static const char *TAG = "fonts";

/* Embedded by EMBED_FILES in main/CMakeLists.txt. */
extern const uint8_t ubuntu_th_start[] asm("_binary_Ubuntu_Th_ttf_start");
extern const uint8_t ubuntu_th_end[] asm("_binary_Ubuntu_Th_ttf_end");
extern const uint8_t ubuntu_m_start[] asm("_binary_Ubuntu_M_ttf_start");
extern const uint8_t ubuntu_m_end[] asm("_binary_Ubuntu_M_ttf_end");
extern const uint8_t ubuntu_mono_start[] asm("_binary_UbuntuMono_B_ttf_start");
extern const uint8_t ubuntu_mono_end[] asm("_binary_UbuntuMono_B_ttf_end");

typedef struct {
    ui_font_family_t family;
    int size;
    lv_font_t *font;
} font_cache_entry_t;

/* Faces are created once and kept for the life of the app. Going over just
 * falls back to the built-in font rather than thrashing. */
#define FONT_CACHE_MAX 24

/*
 * Per-font glyph cache depth.
 *
 * Tiny TTF allocates two LRU caches of this many entries for every font
 * instance, and because sub-16 KB allocations stay in internal RAM these add up
 * fast: at the library default of 256 entries across a dozen sizes, walking the
 * settings pages drained internal memory until the SDIO driver could not get a
 * buffer. Display sizes only ever render digits and punctuation, so they need
 * far less than a UI font showing arbitrary text.
 */
#define CACHE_ENTRIES_DISPLAY 24
#define CACHE_ENTRIES_UI      64
#define DISPLAY_SIZE_PX       60

static font_cache_entry_t s_cache[FONT_CACHE_MAX];
static int s_cache_count = 0;

static void family_data(ui_font_family_t family, const uint8_t **data, size_t *len)
{
    switch (family) {
    case UI_FONT_MEDIUM:
        *data = ubuntu_m_start;
        *len = (size_t)(ubuntu_m_end - ubuntu_m_start);
        break;
    case UI_FONT_MONO:
        *data = ubuntu_mono_start;
        *len = (size_t)(ubuntu_mono_end - ubuntu_mono_start);
        break;
    case UI_FONT_THIN:
    default:
        *data = ubuntu_th_start;
        *len = (size_t)(ubuntu_th_end - ubuntu_th_start);
        break;
    }
}

const lv_font_t *ui_font(ui_font_family_t family, int size)
{
    for (int i = 0; i < s_cache_count; i++) {
        if (s_cache[i].family == family && s_cache[i].size == size) {
            return s_cache[i].font;
        }
    }

    if (s_cache_count >= FONT_CACHE_MAX) {
        ESP_LOGW(TAG, "font cache full, falling back for %d/%dpx", (int)family, size);
        return LV_FONT_DEFAULT;
    }

    const uint8_t *data = NULL;
    size_t len = 0;
    family_data(family, &data, &len);

    size_t cache_entries = size >= DISPLAY_SIZE_PX ? CACHE_ENTRIES_DISPLAY
                                                   : CACHE_ENTRIES_UI;
    lv_font_t *font = lv_tiny_ttf_create_data_ex(data, len, size,
                                                 LV_FONT_KERNING_NORMAL, cache_entries);
    if (font == NULL) {
        ESP_LOGE(TAG, "failed to render family %d at %dpx", (int)family, size);
        return LV_FONT_DEFAULT;
    }

    s_cache[s_cache_count++] = (font_cache_entry_t){ family, size, font };
    return font;
}

const lv_font_t *ui_symbol_font(int size)
{
    /* Only these four built-ins are compiled in; see sdkconfig.defaults. */
    if (size >= 24) {
        return &lv_font_montserrat_24;
    }
    if (size >= 20) {
        return &lv_font_montserrat_20;
    }
    if (size >= 16) {
        return &lv_font_montserrat_16;
    }
    return &lv_font_montserrat_14;
}
