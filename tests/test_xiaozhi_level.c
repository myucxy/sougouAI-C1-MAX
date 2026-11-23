#include "../patches/xiaozhi-official/level.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static uint32_t random_value = 0x208u;
static uint32_t random_next(void)
{
    random_value = random_value * 1664525u + 1013904223u;
    return random_value;
}

static void bounds(const struct xl_level *s)
{
    assert(s->level <= 100 && s->received <= 1 && s->active <= 1 && s->clipped <= 1);
    assert(!s->active || (s->received && !s->clipped));
    for (unsigned i = 0; i < XL_POINTS; ++i) assert(s->points[i] <= 100);
}

static void empty(const struct xl_level *s)
{
    assert(!s->level && !s->received && !s->active && !s->clipped);
    for (unsigned i = 0; i < XL_POINTS; ++i) assert(!s->points[i]);
}

static void constant(int16_t *pcm, int16_t value)
{
    for (unsigned i = 0; i < XL_MAX_SAMPLES; ++i) pcm[i] = value;
}

static void waveform(int16_t *pcm, int amplitude, int offset)
{
    static const int16_t sine[16] = {
        0, 392, 724, 946, 1024, 946, 724, 392,
        0, -392, -724, -946, -1024, -946, -724, -392
    };
    for (unsigned i = 0; i < XL_MAX_SAMPLES; ++i) {
        int value = sine[i % 16u] * amplitude / 1024 + offset;
        if (value > INT16_MAX) value = INT16_MAX;
        if (value < INT16_MIN) value = INT16_MIN;
        pcm[i] = (int16_t)value;
    }
}

static void settle(struct xl_level *s, const int16_t *pcm)
{
    for (unsigned i = 0; i < 16; ++i) {
        assert(xl_update(s, pcm, XL_MAX_SAMPLES));
        bounds(s);
    }
}

static void test_dc_and_noise(void)
{
    const int16_t dc[] = {0, 1, -1, 96, -96, 2048, -23000, 32766, -32767,
                          INT16_MAX, INT16_MIN};
    int16_t pcm[XL_MAX_SAMPLES];
    struct xl_level s;
    for (unsigned v = 0; v < sizeof(dc) / sizeof(dc[0]); ++v) {
        xl_reset(&s);
        constant(pcm, dc[v]);
        settle(&s, pcm);
        assert(s.received && !s.active && !s.level);
        assert(s.clipped == (dc[v] == INT16_MIN || dc[v] == INT16_MAX));
        for (unsigned i = 0; i < XL_POINTS; ++i) assert(!s.points[i]);
    }
    for (unsigned frame = 0; frame < 100; ++frame) {
        for (unsigned i = 0; i < XL_MAX_SAMPLES; ++i)
            pcm[i] = (int16_t)((int)(random_next() >> 25) - 64 + 12000);
        assert(xl_update(&s, pcm, XL_MAX_SAMPLES));
        assert(s.received && !s.active && !s.clipped && !s.level);
        for (unsigned i = 0; i < XL_POINTS; ++i) assert(!s.points[i]);
    }
    /* A single accepted sample reports receipt, but has no AC information. */
    pcm[0] = 20000;
    assert(xl_update(&s, pcm, 1) && s.received && !s.active && !s.level);
}

static void test_waveforms_and_dc_offsets(void)
{
    struct xl_level s = {0}, positive = {0}, negative = {0};
    int16_t pcm[XL_MAX_SAMPLES], shifted[XL_MAX_SAMPLES], copy[XL_MAX_SAMPLES];
    waveform(pcm, 4000, 0);
    memcpy(copy, pcm, sizeof(pcm));
    settle(&s, pcm);
    assert(s.received && s.active && !s.clipped && s.level >= 35 && s.level <= 40);
    assert(!memcmp(pcm, copy, sizeof(pcm)));
    waveform(shifted, 4000, 20000);
    settle(&positive, shifted);
    waveform(shifted, 4000, -20000);
    settle(&negative, shifted);
    assert(!memcmp(&s, &positive, sizeof(s)) && !memcmp(&s, &negative, sizeof(s)));
    unsigned medium = s.level;
    waveform(pcm, 8000, 0);
    settle(&s, pcm);
    assert(s.active && !s.clipped && s.level > medium + 30u && s.level < 90);
    waveform(pcm, 12000, 0);
    settle(&s, pcm);
    assert(s.active && !s.clipped && s.level == 100);
    /* Startup/ramp and silence release are bounded and settle, with fresh flags. */
    xl_reset(&s);
    unsigned previous = 0;
    for (unsigned i = 0; i < 16; ++i) {
        assert(xl_update(&s, pcm, XL_MAX_SAMPLES));
        assert(s.level >= previous && s.active);
        if (!i) assert(s.level > 0 && s.level < 100);
        previous = s.level;
    }
    constant(pcm, -20000);
    assert(xl_update(&s, pcm, XL_MAX_SAMPLES));
    assert(!s.active && s.received && !s.clipped && s.level > 0 && s.level < previous);
    previous = s.level;
    for (unsigned i = 0; i < 32; ++i) {
        assert(xl_update(&s, pcm, XL_MAX_SAMPLES));
        assert(!s.active && s.level <= previous);
        previous = s.level;
    }
    assert(!s.level);
    for (unsigned i = 0; i < XL_POINTS; ++i) assert(!s.points[i]);
}

static void test_points(void)
{
    struct xl_level s = {0};
    int16_t pcm[XL_MAX_SAMPLES];
    for (unsigned i = 0; i < XL_MAX_SAMPLES; ++i) {
        unsigned bar = i / (XL_MAX_SAMPLES / XL_POINTS);
        int amplitude = (int)(bar + 1u) * 900;
        pcm[i] = (int16_t)(i & 1u ? amplitude : -amplitude);
    }
    settle(&s, pcm);
    assert(s.active && s.points[0] > 0 && s.points[XL_POINTS - 1u] < 100);
    for (unsigned i = 1; i < XL_POINTS; ++i) assert(s.points[i] > s.points[i - 1u]);
    xl_reset(&s);
    constant(pcm, 0);
    for (unsigned i = XL_MAX_SAMPLES / 2u; i < XL_MAX_SAMPLES; ++i)
        pcm[i] = (int16_t)(i & 1u ? 4000 : -4000);
    settle(&s, pcm);
    for (unsigned i = 0; i < XL_POINTS; ++i) {
        if (i < XL_POINTS / 2u) assert(!s.points[i]);
        else assert(s.points[i] > 40);
    }
}

static void test_clipping(void)
{
    struct xl_level s = {0};
    int16_t pcm[XL_MAX_SAMPLES + 1u];
    waveform(pcm, 40000, 0);
    settle(&s, pcm);
    assert(s.received && s.clipped && !s.active && s.level == 100);
    for (unsigned i = 0; i < XL_MAX_SAMPLES; ++i)
        pcm[i] = i & 1u ? INT16_MAX : INT16_MIN;
    settle(&s, pcm);
    assert(s.received && s.clipped && !s.active && s.level == 100);
    for (unsigned i = 0; i < XL_MAX_SAMPLES; ++i) {
        for (unsigned rail = 0; rail < 2; ++rail) {
            waveform(pcm, 4000, 0);
            pcm[i] = rail ? INT16_MAX : INT16_MIN;
            assert(xl_update(&s, pcm, XL_MAX_SAMPLES));
            assert(s.received && s.clipped && !s.active);
        }
    }
    waveform(pcm, 4000, 0);
    pcm[XL_MAX_SAMPLES] = INT16_MIN;
    assert(xl_update(&s, pcm, XL_MAX_SAMPLES));
    assert(s.received && s.active && !s.clipped);
    pcm[0] = INT16_MIN;
    assert(xl_update(&s, pcm, 1) && s.clipped && !s.active && s.received);
    /* Extreme DC bias makes a centered sample exceed the int16_t range. */
    constant(pcm, INT16_MAX);
    pcm[0] = INT16_MIN;
    assert(xl_update(&s, pcm, XL_MAX_SAMPLES) && s.clipped && !s.active);
    bounds(&s);
}

static void test_reset_and_invalid(void)
{
    struct xl_level s = {0}, other = {0};
    int16_t pcm[XL_MAX_SAMPLES];
    waveform(pcm, 8000, 0);
    settle(&s, pcm);
    empty(&other);
    xl_reset(&s);
    empty(&s);
    xl_reset(NULL);
    assert(!xl_update(NULL, pcm, XL_MAX_SAMPLES));
    assert(!xl_update(NULL, NULL, UINT_MAX));
    settle(&s, pcm);
    /* Empty update is the caller's explicit capture-gap/staleness signal. */
    assert(!xl_update(&s, NULL, 0));
    empty(&s);
    settle(&s, pcm);
    assert(!xl_update(&s, NULL, XL_MAX_SAMPLES));
    empty(&s);
    settle(&s, pcm);
    assert(!xl_update(&s, pcm, 0));
    empty(&s);
    settle(&s, pcm);
    int16_t single = 42;
    assert(!xl_update(&s, &single, XL_MAX_SAMPLES + 1u));
    empty(&s);
    assert(!xl_update(&s, &single, UINT_MAX));
    empty(&s);
    assert(xl_update(&other, pcm, XL_MAX_SAMPLES) && other.active);
    empty(&s);
}

static void test_lengths_and_random(void)
{
    struct xl_level s = {0};
    int16_t pcm[XL_MAX_SAMPLES];
    for (unsigned count = 1; count <= XL_MAX_SAMPLES; ++count) {
        constant(pcm, -32767);
        xl_reset(&s);
        assert(xl_update(&s, pcm, count));
        assert(s.received && !s.level && !s.active && !s.clipped);
        waveform(pcm, 8000, 0);
        assert(xl_update(&s, pcm, count));
        bounds(&s);
    }
    for (unsigned frame = 0; frame < 20000; ++frame) {
        unsigned count = random_next() % (XL_MAX_SAMPLES + 2u);
        unsigned clipped = 0;
        for (unsigned i = 0; i < XL_MAX_SAMPLES; ++i) {
            pcm[i] = (int16_t)((int32_t)(random_next() >> 16) - 32768);
            if (i < count && (pcm[i] == INT16_MIN || pcm[i] == INT16_MAX)) clipped = 1;
        }
        if (count && count <= XL_MAX_SAMPLES) {
            assert(xl_update(&s, pcm, count));
            assert(s.received && s.clipped == clipped);
        } else {
            assert(!xl_update(&s, pcm, count));
            empty(&s);
        }
        bounds(&s);
    }
}

int main(void)
{
    test_dc_and_noise();
    test_waveforms_and_dc_offsets();
    test_points();
    test_clipping();
    test_reset_and_invalid();
    test_lengths_and_random();
    puts("PASS: silence/DC/noise, waveform/DC offsets, smoothing, 8 envelope points, clipping, reset/gaps, all lengths and 20000 random frames.");
    return 0;
}
