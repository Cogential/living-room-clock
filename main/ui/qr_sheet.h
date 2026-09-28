#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Full-screen sheet with a QR code for `url`, so an article can be opened on a
 * phone - the clock has no browser and cannot usefully have one.
 *
 * Lives on the top layer, above the chrome, and closes on any tap or by itself
 * after a minute. Showing a new sheet replaces the current one. An empty `url`
 * shows the story without a code rather than doing nothing, so the tap still
 * visibly registers.
 */
void ui_qr_sheet_show(uint32_t accent, const char *heading, const char *title,
                      const char *detail, const char *url);

/** Close the sheet if one is showing. */
void ui_qr_sheet_close(void);

#ifdef __cplusplus
}
#endif
