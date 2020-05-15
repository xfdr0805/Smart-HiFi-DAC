/*
 * @Date: 2019-11-13 11:07:29
 * @LastEditors: 何光辉
 * @LastEditTime: 2020-05-14 15:54:11
 * @FilePath: \CS8422_OLED_ESP8266\src\pcm5121.h
 */
#ifndef _PCM5121_H_
#define _PCM5121_H_
void pcm5121_init();
void set_pcm5121_volume(uint8_t vol_l, uint8_t vol_r);
uint8_t get_fs_pll_status();
uint8_t get_lock_status();
uint8_t get_FS_status();
#endif