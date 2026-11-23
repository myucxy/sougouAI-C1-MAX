#ifndef C208_OFFICIAL_AUDIO_H
#define C208_OFFICIAL_AUDIO_H
#include <stdint.h>
/* Original native driver ABI; only operate after exclusive open succeeds. */
int xa_quiescent(void);
int xa_capture_open(void);
int xa_capture_read(int16_t *mono,unsigned capacity_samples);
int xa_capture_close(void);
/* Value-only fault snapshot; never stores PCM or network identity. */
void xa_capture_report(unsigned error);
int xa_playback_open(void);
int xa_playback_write(const int16_t *mono,unsigned frames);
/* Nonblocking normal end: 0 pending, 1 tail heard with output held, -1 fault. */
int xa_playback_finish(void);
/* True only when the normal tail completed and the running output is reusable. */
int xa_playback_parked(void);
/* Feed silence during network gaps and between replies; caller must keep ticking. */
int xa_playback_keepalive(void);
int xa_playback_close(void);
#endif
