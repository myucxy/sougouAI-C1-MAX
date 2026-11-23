#include "level.h"

static unsigned square_root(uint32_t value)
{
    uint32_t result = 0;
    for (uint32_t bit = 1u << 15; bit; bit >>= 1) {
        uint32_t candidate = result | bit;
        if (candidate * candidate <= value) result = candidate;
    }
    return (unsigned)result;
}

static unsigned rms(uint32_t energy, unsigned count)
{
    return count ? square_root((energy / count) << 9) : 0;
}

static unsigned height(unsigned value, unsigned peak)
{
    unsigned amplitude = (3u * value + peak) / 4u;
    if (amplitude <= 96u) return 0;
    if (amplitude >= 8192u) return 100;
    return (amplitude - 96u) * 100u / (8192u - 96u);
}

static uint8_t smooth(unsigned previous, unsigned target)
{
    /* Half-distance attack, quarter-distance release; rounding reaches zero. */
    if (target > previous) return (uint8_t)(previous + (target - previous + 1u) / 2u);
    return (uint8_t)(previous - (previous - target + 3u) / 4u);
}

void xl_reset(struct xl_level *state)
{
    if (state) *state = (struct xl_level){0};
}

int xl_update(struct xl_level *state, const int16_t *pcm, unsigned count)
{
    if (!state) return 0;
    if (!pcm || !count || count > XL_MAX_SAMPLES) {
        xl_reset(state);
        return 0;
    }
    int32_t sum = 0;
    unsigned clipped = 0;
    for (unsigned i = 0; i < count; ++i) {
        sum += pcm[i];
        if (pcm[i] == INT16_MIN || pcm[i] == INT16_MAX) clipped = 1;
    }
    int32_t mean = sum / (int32_t)count;
    uint32_t energy = 0;
    unsigned peak = 0, begin = 0;
    for (unsigned bar = 0; bar < XL_POINTS; ++bar) {
        unsigned end = count * (bar + 1u) / XL_POINTS;
        uint32_t part_energy = 0;
        unsigned part_peak = 0;
        for (unsigned i = begin; i < end; ++i) {
            int32_t centered = (int32_t)pcm[i] - mean;
            uint32_t magnitude = (uint32_t)(centered < 0 ? -centered : centered);
            /* |centered| <= 65535; 320*(65535^2 >> 9) < UINT32_MAX.
             * Discarding nine energy bits keeps the accumulator 32-bit. */
            part_energy += (magnitude * magnitude) >> 9;
            if (magnitude > part_peak) part_peak = (unsigned)magnitude;
        }
        state->points[bar] = smooth(state->points[bar],
                                    height(rms(part_energy, end - begin), part_peak));
        energy += part_energy;
        if (part_peak > peak) peak = part_peak;
        begin = end;
    }
    unsigned value = rms(energy, count);
    state->level = smooth(state->level, height(value, peak));
    state->received = 1;
    state->clipped = (uint8_t)clipped;
    state->active = (uint8_t)(!clipped && value >= XL_ACTIVE_RMS);
    return 1;
}
