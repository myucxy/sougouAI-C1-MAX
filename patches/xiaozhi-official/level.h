#ifndef C208_XIAOZHI_LEVEL_H
#define C208_XIAOZHI_LEVEL_H
#include <stdint.h>

#define XL_MAX_SAMPLES 320u
#define XL_POINTS 8u
#define XL_ACTIVE_RMS 256u

/* One caller-owned meter per PCM stream; initialize with xl_reset() or {0}.
 * These are signal activity measurements, not speech or microphone-health tests.
 * received/active/clipped describe only the latest update:
 *   received: a nonempty, accepted PCM frame (including silence or DC).
 *   active: DC-rejected RMS >= XL_ACTIVE_RMS, with no samples at either rail.
 *   clipped: at least one raw sample equals INT16_MIN or INT16_MAX.
 * level and points are smoothed 0..100 heights. Points cover eight consecutive
 * time slices of the frame, suitable for bars or a line envelope, not FFT bins.
 */
struct xl_level {
    uint8_t level, received, active, clipped;
    uint8_t points[XL_POINTS];
};

void xl_reset(struct xl_level *state);
/* Accept 1..XL_MAX_SAMPLES signed 16-bit mono samples; return 1 if accepted.
 * NULL PCM, zero count or an oversized frame clears the meter and returns 0,
 * without reading PCM. A NULL state is harmless and returns 0.
 * Call with regular frames: smoothing is per update, not per elapsed time.
 * There is no clock: the caller must reset/send an empty update on capture gaps.
 * DC rejection subtracts the frame mean. Display amplitude uses (3*RMS+peak)/4,
 * a 96-count floor and 8192-count ceiling; activity uses the unsmoothed RMS.
 * Fixed thresholds need board/gain calibration and cannot establish mic health.
 */
int xl_update(struct xl_level *state, const int16_t *pcm, unsigned count);
#endif
