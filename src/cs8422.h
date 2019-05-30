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
#endif