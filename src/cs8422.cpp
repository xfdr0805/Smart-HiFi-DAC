#include <Arduino.h>
#include <Wire.h>
#include <cs8422.h>
#define ADDR_CHIP_ID 0x01
#define Clock_Control 0x02
#define Receiver_Input_Control 0x03
#define Receiver_Data_Control 0x04
#define Gpio_Control1 0x05
#define Gpio_Control2 0x06
#define SRC_Output_Serial_Port_Clock 0x08
#define Recovered_Master_Clock 0x09
#define Data_Routing_Control 0x0A
#define Serial_Audio_Input_Data_Format 0x0B
#define Serial_Audio_Output1_Data_Format 0x0C
#define Serial_Audio_Output2_Data_Format 0x0D
#define Receiver_Error_Unmasking 0x0E
#define Interrupt_Unmasking 0x0F
#define Interrupt_Mode 0x10
#define Format_Status 0x12
#define Receiver_Error 0x13
#define Interrupt_Status 0x14
#define PLL_Status 0x15
#define Receiver_Status 0x16

//I2C address 7bit
const int cs8422 = 0x10;
extern int i2cfail;
void cs8422_init()
{
    Wire.beginTransmission(cs8422);
    Wire.write(Clock_Control); //PDN 0
    Wire.write(0x00);
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(cs8422);
    Wire.write(SRC_Output_Serial_Port_Clock);
    Wire.write(0x4a); //
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(cs8422);
    Wire.write(Recovered_Master_Clock);
    Wire.write(0x40); //
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(cs8422);
    Wire.write(Receiver_Input_Control);
    Wire.write(0x84); //RX_MODE:1 INPUT_TYPE:1  select rx0 input
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(cs8422);
    Wire.write(Receiver_Data_Control);
    Wire.write(0x0c);
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(cs8422);
    Wire.write(Serial_Audio_Input_Data_Format);
    Wire.write(0x08); // i2s in 24bit
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(cs8422);
    Wire.write(Data_Routing_Control);
    Wire.write(0x02); //0 - Serial Audio Input Port (SDIN)  1 - AES3 Receiver Output
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(cs8422);
    Wire.write(Serial_Audio_Output1_Data_Format);
    Wire.write(0x84); //SDOUT1
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(cs8422);
    Wire.write(Serial_Audio_Output2_Data_Format);
    Wire.write(0x84); //SDOUT2
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(cs8422);
    Wire.write(Interrupt_Unmasking);
    Wire.write(0x03); //Interrept
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(cs8422);
    Wire.write(Receiver_Error_Unmasking);
    Wire.write(0x00); //Interrept屏蔽所有错误
    i2cfail += Wire.endTransmission(true);

    // Wire.beginTransmission(cs8422);
    // Wire.write(Interrupt_Mode);
    // Wire.write(0x00); //Interrept mode  Falling edge active
    // i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(cs8422);
    Wire.write(Gpio_Control2);
    Wire.write(0x03); //Interrept on gpio3
    i2cfail += Wire.endTransmission(true);
}
void select_input(uint8_t ss)
{
    if (ss < 4)
    {
        Wire.beginTransmission(cs8422);
        Wire.write(Data_Routing_Control);
        Wire.write(0x02); //0 - Serial Audio Input Port (SDIN)  1 - AES3 Receiver Output
        i2cfail += Wire.endTransmission(true);

        Wire.beginTransmission(cs8422);
        Wire.write(Receiver_Input_Control);
        Wire.write((ss << 5) | 0x84); // i2s in 24bit
        i2cfail += Wire.endTransmission(true);
        //Serial.printf("Receiver_Input_Control:0x%02X", (ss << 5) | 0x84);
    }
    else if (ss == 4)
    {
        Wire.beginTransmission(cs8422);
        Wire.write(Data_Routing_Control);
        Wire.write(0x00); //0 - Serial Audio Input Port (SDIN)  1 - AES3 Receiver Output
        i2cfail += Wire.endTransmission(true);
    }
    else
    {
        return;
    }
}
uint8_t get_cs8422_status(uint8_t reg)
{
    uint8_t data = 0;
    Wire.beginTransmission(cs8422);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom(cs8422, 1);
    if (Wire.available() > 0)
    {
        data = Wire.read();
    }
    return data;
}
uint8_t get_cs8422_id()
{
    return get_cs8422_status(ADDR_CHIP_ID);
}
uint8_t get_format_status()
{
    return get_cs8422_status(Format_Status);
}
uint8_t get_pll_status()
{
    return get_cs8422_status(PLL_Status);
}
uint8_t get_receiver_status()
{

    return get_cs8422_status(Receiver_Status);
}
uint8_t get_interrupt_status()
{

    return get_cs8422_status(Interrupt_Status);
}
uint8_t get_error_status()
{

    return get_cs8422_status(Receiver_Error);
}
