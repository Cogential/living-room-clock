#pragma once

#include <stdbool.h>
#include <time.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Snapshot of "now", computed once per frame and handed to the active face. */
typedef struct {
    struct tm tm;
    int  millis;      /* 0..999, for smooth sweeping motion */
    bool time_valid;  /* false until SNTP has landed at least once */
} clock_time_t;

/**
 * One selectable clock style.
 *
 * Only one face is live at a time, so faces keep their widget pointers in
 * module statics rather than threading a context through every callback.
 */
typedef struct {
    const char *name;        /* shown in the status bar and the settings picker */
    bool        light_chrome;/* true when the face is dark and chrome must be light */

    /** Build the face into `parent` and return its root object. */
    lv_obj_t *(*create)(lv_obj_t *parent);
    /** Called ~30x/second while this face is active. */
    void (*tick)(const clock_time_t *now);
    /**
     * Optional. A short tap that no chrome button claimed, in screen
     * coordinates. The chrome layer sits above every face and receives all
     * touches, so this is the only way a face sees one. NULL for the purely
     * decorative faces, which is most of them.
     */
    void (*tap)(int x, int y);
} clock_face_t;

/* The registered faces, in the order they appear in the picker. */
extern const clock_face_t face_minimal;
extern const clock_face_t face_cyberpunk;
extern const clock_face_t face_outrun;
extern const clock_face_t face_pipboy;
extern const clock_face_t face_rapture;
extern const clock_face_t face_stardew;
extern const clock_face_t face_tavern;
extern const clock_face_t face_blockcraft;
extern const clock_face_t face_aperture;
extern const clock_face_t face_hyrule;
extern const clock_face_t face_cuckoo;
extern const clock_face_t face_dashboard;
extern const clock_face_t face_photo;

int ui_face_count(void);
const char *ui_face_name(int index);
int ui_face_current(void);

/** Index of the face stored in settings, or the default if none is stored. */
int ui_face_selected(void);

/** Record `index` as the chosen face and persist it. Does not load it. */
void ui_face_select(int index);

/** Build the whole interface on the active screen and start the render loop. */
void ui_start(void);

/** Switch to a face by index, sliding from the current one. */
void ui_show_face(int index);

/*
 * Full-screen apps.
 *
 * An app hides the clock chrome and takes the screen; ui_close_app() rebuilds
 * the face that was showing. Settings is just the first one.
 */
void ui_open_app(void (*open_fn)(void));
void ui_close_app(void);
bool ui_app_open(void);

/** Settings, via the same mechanism. */
void ui_open_settings(void);
void ui_close_settings(void);
bool ui_settings_open(void);

/** Format hours according to the 24/12-hour build setting. */
int ui_display_hour(const struct tm *t);

/** "AM"/"PM" for 12-hour mode, empty string in 24-hour mode. */
const char *ui_meridiem(const struct tm *t);

#ifdef __cplusplus
}
#endif
