// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void fz_video_init(int w,int h);
void fz_present_pixels(const uint32_t*,int w,int h);
uint16_t fz_read_line(int max,uint16_t*buf,int timeout,int width,int continued);
uint16_t fz_read_char(int timeout,int cursor);
void fz_transcript_char(uint16_t);
void fz_transcript_erase(int);
void fz_transcript_window(uint16_t,int window,int row);
void fz_layout_changed(void);
int fz_main_window(int*x,int*y,int*w,int*h);
char* fz_file_dialog(const char*,int);
int fz_save_complete(int);
void fz_before_opcode(void);
void fz_engine_started(void);
void fz_tick(void);
void fz_engine_exit(int);
void fz_engine_error(const char*);
int fz_has_pictures(void);
int fz_has_sound(void);
int fz_audio_volume(int);
void fz_audio_unavailable(void);
int fz_audio_active(void);
void fz_set_mouse(int x,int y);
uint16_t fz_mouse_state(int*x,int*y);
void fz_capture_boundary(int safe);
int fz_vm_main(int,char**);
int fz_vm_save(const char*);
int fz_vm_restore(const char*);
int fz_screen_save(const char*);
int fz_screen_restore(const char*);
void fz_draw_input(uint16_t*,int,int,int,int,int);
void fz_get_text_position(int*,int*);
void fz_flush(void);
int fz_is_terminator(uint16_t);
int fz_char_valid(uint16_t);
int fz_text_width(const uint16_t*);
void fz_check_audio(void);
void fz_suspend_audio(int);
#ifdef __cplusplus
}
#endif
