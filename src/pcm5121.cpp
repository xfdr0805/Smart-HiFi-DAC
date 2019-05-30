#include <Wire.h>
//All the "important" registers that need to be configured in the PCM5122
#define reg01 0x01
#define reg02 0x02
#define reg04 0x04
#define reg09 0x09
#define reg13 0x0D
#define reg14 0x0E
#define reg19 0x13
#define reg20 0x14 //PLL P
#define reg21 0x15 //PLL J
#define reg22 0x16 //PLL D
#define reg23 0x17
#define reg24 0x18 //PLL R
#define reg27 0x1B //DSP Clk Divider
#define reg28 0x1C //DAC Clk Divider
#define reg29 0x1D //NCP Clock Divider
#define reg30 0x1E //OSR Clock Divider
#define reg37 0x25
#define reg40 0x28
#define reg61 0x3D
#define reg62 0x3E
#define reg65 0x41
#define reg86 0x56
#define reg91 0x5B //These bits indicate the currently detected audio sampling rate
#define reg94 0x5E //This bit indicates whether the PLL is locked or not. The PLL will be reported as unlocked when it is disabled.
#define reg95 0x5F
#define reg108 0x6C
#define reg118 0x76
//I2C address for the PCM5121 as it is configured on the PCB design
const int pcm5122 = 0x4C;
//If this is > 0 then the PCM5122 DAC failed i2c communitication
int i2cfail = 0;
/*
 * pcm5122_init
 * I2C commands that initilize the pcb5122 and allow it to output audio
 */
void pcm5121_init()
{
    Wire.beginTransmission(pcm5122);
    Wire.write(reg02);
    Wire.write(0x10); //standby mode
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(reg01);
    Wire.write(0x11); //reset reg
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(reg02);
    Wire.write(0x00); //Normal operation
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(reg13);
    Wire.write(0x10); //The PLL reference clock is BCK
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(reg14);
    Wire.write(0x10); //PLL clock
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(reg37);
    Wire.write(0x7D); //Ignore FS  BCK  SCK detection ......
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(reg61); //default 0db
    Wire.write(0x00);  //The Left digital volume is 24 dB to -103 dB in -0.5 dB step
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(reg62);
    Wire.write(0x00); //The  Right digital volume is 24 dB to -103 dB in -0.5 dB step
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(reg65);
    Wire.write(0x07); //This bit controls the behavior of the auto mute upon zero sample detection
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(0x00);
    Wire.write(0x01); //page 1 select
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(0x05);
    Wire.write(0x02); //XSMUTE  External Under Voltage Protection
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(0x00);
    Wire.write(0x00); //page 1 select
    i2cfail += Wire.endTransmission(true);
}
void set_pcm5121_volume(uint8_t vol_l, uint8_t vol_r)
{
    uint8_t volume;
    float l = 256 / 100.0 * vol_l;
    volume = 256 - (uint8_t)l;
    Wire.beginTransmission(pcm5122);
    Wire.write(reg61);  //default 0db
    Wire.write(volume); //The Left digital volume is 24 dB to -103 dB in -0.5 dB step
    i2cfail += Wire.endTransmission(true);

    Wire.beginTransmission(pcm5122);
    Wire.write(reg62);
    Wire.write(volume); //The  Right digital volume is 24 dB to -103 dB in -0.5 dB step
    i2cfail += Wire.endTransmission(true);
}
uint8_t get_fs_pll_status()
{
    uint8_t data = 0;
    Wire.beginTransmission(pcm5122);
    Wire.write(reg91);
    Wire.endTransmission();
    Wire.requestFrom(pcm5122, 1);
    if (Wire.available() > 0)
    {
        data = Wire.read();
    }
    return data;
}
uint8_t get_lock_status()
{
    uint8_t data = 0;
    Wire.beginTransmission(pcm5122);
    Wire.write(reg94);
    Wire.endTransmission();
    Wire.requestFrom(pcm5122, 1);
    if (Wire.available() > 0)
    {
        data = Wire.read();
    }
    return data;
}