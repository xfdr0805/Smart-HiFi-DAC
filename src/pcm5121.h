#ifndef _PCM5121_H_
#define _PCM5121_H_
void pcm5121_init();
void set_pcm5121_volume(uint8_t vol_l, uint8_t vol_r);
uint8_t get_fs_pll_status();
uint8_t get_lock_status();
#endif