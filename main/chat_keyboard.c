#include "chat_keyboard.h"
#include "esp_log.h"
#include <string.h>

LV_FONT_DECLARE(font_ru_18);

static const char *TAG = "KEYBOARD";

/* ---------- Раскладки ---------- */
static const char *KB_RU_LOWER[] = {
    "йцукенгшщзхъ",
    "фывапролджэ",
    "ячсмитьбю",
};
static const char *KB_RU_UPPER[] = {
    "ЙЦУКЕНГШЩЗХЪ",
    "ФЫВАПРОЛДЖЭ",
    "ЯЧСМИТЬБЮ",
};

static const char *KB_EN_LOWER[] = {
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm",
};
static const char *KB_EN_UPPER[] = {
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM",
};

static const char *KB_NUM_ROW1[] = {"1234567890"};
static const char *KB_NUM_ROW2[] = {"-/:;()$&@\""};
static const char *KB_NUM_ROW3[] = {".,?!'`~^="};

/* ---------- Состояние ---------- */
typedef enum {
    LAYOUT_RU,
    LAYOUT_EN,
    LAYOUT_NUM,
} layout_t;

static layout_t s_layout = LAYOUT_RU;
static bool s_shift = false;
static lv_obj_t *s_keyboard = NULL;
static lv_obj_t *s_key_btns[64];
static int s_key_count = 0;
static keyboard_key_cb_t s_cb = NULL;

/* ---------- Предварительное объявление ---------- */
void chat_keyboard_redraw(void);

/* ---------- Единый обработчик клавиш ---------- */
static void key_event_cb(lv_event_t *e)
{
    const char *key = (const char *)lv_event_get_user_data(e);
    if (!key) return;

    if (strcmp(key, "LANG") == 0) {
        if (s_layout == LAYOUT_RU) s_layout = LAYOUT_EN;
        else if (s_layout == LAYOUT_EN) s_layout = LAYOUT_RU;
        chat_keyboard_redraw();
    } else if (strcmp(key, "NUM") == 0) {
        s_layout = (s_layout == LAYOUT_NUM) ? LAYOUT_RU : LAYOUT_NUM;
        chat_keyboard_redraw();
    } else if (strcmp(key, "SHIFT") == 0) {
        s_shift = !s_shift;
        chat_keyboard_redraw();
    } else {
        if (s_cb) s_cb(key);
    }
}

/* ---------- Создание кнопки ---------- */
static lv_obj_t *create_key(lv_obj_t *parent, const char *label,
                             lv_coord_t x, lv_coord_t y, lv_coord_t w,
                             const char *value)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, 50);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &font_ru_18, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, key_event_cb, LV_EVENT_CLICKED, (void *)value);

    if (s_key_count < 64) {
        s_key_btns[s_key_count++] = btn;
    }
    return btn;
}

/* ---------- Очистка ---------- */
static void clear_keyboard(void)
{
    for (int i = 0; i < s_key_count; i++) {
        if (s_key_btns[i]) {
            lv_obj_del(s_key_btns[i]);
            s_key_btns[i] = NULL;
        }
    }
    s_key_count = 0;
}

/* ---------- Перерисовка ---------- */
void chat_keyboard_redraw(void)
{
    if (!s_keyboard) return;

    clear_keyboard();

    const lv_coord_t KEY_W = 36;
    const lv_coord_t KEY_H = 50;
    const lv_coord_t GAP_X = 2;
    const lv_coord_t GAP_Y = 4;
    const lv_coord_t START_X = 4;
    const lv_coord_t START_Y = 4;

    if (s_layout == LAYOUT_NUM) {
        const char **rows[] = { KB_NUM_ROW1, KB_NUM_ROW2, KB_NUM_ROW3 };
        for (int r = 0; r < 3; r++) {
            const char *row = rows[r][0];
            int len = strlen(row);
            for (int i = 0; i < len; i++) {
                char buf[2] = { row[i], 0 };
                create_key(s_keyboard, buf,
                           START_X + i * (KEY_W + GAP_X),
                           START_Y + r * (KEY_H + GAP_Y),
                           KEY_W, buf);
            }
        }
    } else {
        const char **rows;
        if (s_layout == LAYOUT_RU) {
            rows = s_shift ? KB_RU_UPPER : KB_RU_LOWER;
        } else {
            rows = s_shift ? KB_EN_UPPER : KB_EN_LOWER;
        }

        for (int r = 0; r < 3; r++) {
            const char *row = rows[r];
            int len = strlen(row);
            for (int i = 0; i < len; i++) {
                char buf[2] = { row[i], 0 };
                create_key(s_keyboard, buf,
                           START_X + i * (KEY_W + GAP_X),
                           START_Y + r * (KEY_H + GAP_Y),
                           KEY_W, buf);
            }
        }
    }

    // Нижний ряд
    lv_coord_t bottom_y = START_Y + 3 * (KEY_H + GAP_Y);

    const char *lang_label = (s_layout == LAYOUT_RU) ? "EN" :
                             (s_layout == LAYOUT_EN) ? "RU" : "RU";
    create_key(s_keyboard, lang_label, 4, bottom_y, 50, "LANG");

    const char *num_label = (s_layout == LAYOUT_NUM) ? "АБВ" : "123";
    create_key(s_keyboard, num_label, 58, bottom_y, 50, "NUM");

    create_key(s_keyboard, "SH", 112, bottom_y, 50, "SHIFT");
    create_key(s_keyboard, " ", 166, bottom_y, 120, " ");
    create_key(s_keyboard, "DEL", 290, bottom_y, 56, "BACKSPACE");
    create_key(s_keyboard, "OK", 350, bottom_y, 56, "ENTER");
}

/* ---------- Публичные функции ---------- */
lv_obj_t *chat_keyboard_create(lv_obj_t *parent, keyboard_key_cb_t cb)
{
    s_cb = cb;

    s_keyboard = lv_obj_create(parent);
    lv_obj_set_size(s_keyboard, 410, 280);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_keyboard, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(s_keyboard, 0, 0);
    lv_obj_set_style_pad_all(s_keyboard, 0, 0);
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_SCROLLABLE);

    chat_keyboard_redraw();

    ESP_LOGI(TAG, "Клавиатура создана");
    return s_keyboard;
}

void chat_keyboard_set_visible(bool visible)
{
    if (!s_keyboard) return;
    if (visible) {
        lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void chat_keyboard_reset(void)
{
    s_layout = LAYOUT_RU;
    s_shift = false;
    chat_keyboard_redraw();
}
