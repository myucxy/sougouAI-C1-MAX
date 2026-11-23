#ifndef C208_VOLUME_SHEET_H
#define C208_VOLUME_SHEET_H
int volume_sheet_open(void *root);
int volume_sheet_gesture(unsigned gesture);
void volume_sheet_message(const unsigned char *message);
void volume_sheet_destroy(void);
void volume_sheet_bind(void *root);
void volume_sheet_request(void *root);
unsigned volume_sheet_active(void);
#endif

