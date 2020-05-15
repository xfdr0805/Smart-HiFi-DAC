/*
 * @Date: 2019-11-13 11:07:29
 * @LastEditors: 何光辉
 * @LastEditTime: 2020-05-15 08:54:17
 * @FilePath: \CS8422_OLED_ESP8266\src\cs8422.h
 */
#ifndef _CS8422_H_
#define _CS8422_H_
void cs8422_init();
uint8_t get_cs8422_id();
uint8_t get_format_status();
uint8_t get_pll_status();
uint8_t get_receiver_status();
void select_input(uint8_t ss);
uint8_t get_interrupt_status();
uint8_t get_error_status();
void set_cs8422(uint8_t reg, uint8_t data);
#endif