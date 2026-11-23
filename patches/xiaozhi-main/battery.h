#ifndef C208_AI_BATTERY_H
#define C208_AI_BATTERY_H

#include <stdint.h>

#define AI_BATTERY_WIDTH 80u
#define AI_BATTERY_HEIGHT 30u
#define AI_BATTERY_PIXELS (AI_BATTERY_WIDTH * AI_BATTERY_HEIGHT)

enum ai_battery_charge {
    AI_BATTERY_DISCHARGING = 0,
    AI_BATTERY_CHARGING = 1,
    AI_BATTERY_FULL = 2,
    AI_BATTERY_EXTERNAL_UNKNOWN = 3
};

/* PMIC state shared by the XiaoZhi and MP3 status displays. */
unsigned ai_battery_charging(void);

/* Redraw a caller-owned, tightly packed, row-major RGB565 status bitmap.
 * pixels must hold AI_BATTERY_PIXELS uint16_t values; NULL is a no-op.
 * The caller positions this bitmap in its UI.
 * Each call clears the background to black and writes white percentage text.
 * percent 0..100 controls the fill, including in FULL; larger values show --%
 * with no fill. Positive levels always fill at least one column.
 * CHARGING adds a yellow bolt, FULL a white check, EXTERNAL_UNKNOWN a cyan plug.
 * Other charge values use the neutral/discharging appearance.
 * No allocation, native UI, battery sampling, or retained state is involved.
 */
void ai_battery_draw(uint16_t *pixels, unsigned percent,
                     enum ai_battery_charge charging);

#endif
