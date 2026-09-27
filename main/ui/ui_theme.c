#include "ui/ui_theme.h"

void ui_make_layer(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_set_gradient(lv_obj_t *obj, lv_color_t top, lv_color_t bottom)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, top, 0);
    lv_obj_set_style_bg_grad_color(obj, bottom, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
}

/* Concentric translucent discs rather than one disc with a huge blur.
 *
 * Measured on this panel: a 620 px circle with a 310 px box-shadow dropped the
 * whole UI from ~28 fps to well under 1 fps, because LVGL's software shadow
 * cost scales with blur radius over the shadow's area and it is recomputed on
 * every refresh. Stacked alpha fills approximate the same falloff for the cost
 * of a few rounded-rect blits. */
#define GLOW_RINGS 7

lv_obj_t *ui_add_glow(lv_obj_t *parent, lv_color_t color, int diameter,
                      int x, int y, lv_opa_t opa)
{
    lv_obj_t *glow = lv_obj_create(parent);
    lv_obj_remove_style_all(glow);
    lv_obj_set_size(glow, diameter, diameter);
    lv_obj_set_pos(glow, x, y);
    lv_obj_set_style_bg_opa(glow, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(glow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(glow, LV_OBJ_FLAG_CLICKABLE);

    /* Each ring is faint; the overlap is what builds brightness toward the
     * middle, so the edge stays soft. */
    lv_opa_t step = (lv_opa_t)(opa / 3 + 1);

    for (int i = 0; i < GLOW_RINGS; i++) {
        int size = diameter - (diameter * i) / GLOW_RINGS;
        lv_obj_t *ring = lv_obj_create(glow);
        lv_obj_remove_style_all(ring);
        lv_obj_set_size(ring, size, size);
        lv_obj_align(ring, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, step, 0);
        lv_obj_set_style_bg_color(ring, color, 0);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    }

    return glow;
}

lv_obj_t *ui_add_rect(lv_obj_t *parent, int x, int y, int w, int h,
                      uint32_t color, lv_opa_t opa, int radius)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, w, h);
    lv_obj_set_pos(r, x, y);
    lv_obj_set_style_bg_opa(r, opa, 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(color), 0);
    lv_obj_set_style_radius(r, radius, 0);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
    return r;
}

void ui_add_scanlines(lv_obj_t *parent, uint32_t color, int spacing, lv_opa_t opa)
{
    if (spacing < 4) {
        spacing = 4;  /* any denser and this is hundreds of objects */
    }
    for (int y = 0; y < UI_SCREEN_H; y += spacing) {
        ui_add_rect(parent, 0, y, UI_SCREEN_W, 1, color, opa, 0);
    }
}

void ui_add_corner_brackets(lv_obj_t *parent, int inset, int len, int thick,
                            uint32_t color, lv_opa_t opa)
{
    const int right = UI_SCREEN_W - inset;
    const int bottom = UI_SCREEN_H - inset;

    /* Each corner is two rectangles rather than a rotated shape: LVGL would
     * resample a transformed object and these are meant to be crisp. */
    ui_add_rect(parent, inset, inset, len, thick, color, opa, 0);
    ui_add_rect(parent, inset, inset, thick, len, color, opa, 0);

    ui_add_rect(parent, right - len, inset, len, thick, color, opa, 0);
    ui_add_rect(parent, right - thick, inset, thick, len, color, opa, 0);

    ui_add_rect(parent, inset, bottom - thick, len, thick, color, opa, 0);
    ui_add_rect(parent, inset, bottom - len, thick, len, color, opa, 0);

    ui_add_rect(parent, right - len, bottom - thick, len, thick, color, opa, 0);
    ui_add_rect(parent, right - thick, bottom - len, thick, len, color, opa, 0);
}

void ui_add_ornate_rule(lv_obj_t *parent, int y, int width, uint32_t color, lv_opa_t opa)
{
    int x = (UI_SCREEN_W - width) / 2;
    ui_add_rect(parent, x, y, width, 2, color, opa, 0);

    /* A square turned on its point reads as a diamond without a rotation. */
    lv_obj_t *d = ui_add_rect(parent, UI_SCREEN_W / 2 - 7, y - 6, 14, 14, color, opa, 3);
    lv_obj_set_style_transform_rotation(d, 450, 0);
    lv_obj_set_style_transform_pivot_x(d, 7, 0);
    lv_obj_set_style_transform_pivot_y(d, 7, 0);
}
