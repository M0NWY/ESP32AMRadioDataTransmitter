//code by bitluni (send me a high five if you like the code)

//replace this include file by your header file generated by the conversion tool
#include "sample.h"

#include <soc/rtc.h>
#include "driver/i2s.h"

static const i2s_port_t i2s_num = (i2s_port_t)I2S_NUM_0; // i2s port number

//static i2s_config_t i2s_config;
static const i2s_config_t i2s_config = {
     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
     .sample_rate = 1000000,  //not really used
     .bits_per_sample = (i2s_bits_per_sample_t)I2S_BITS_PER_SAMPLE_16BIT, 
     .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
     .communication_format = I2S_COMM_FORMAT_STAND_MSB,
     .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
     .dma_buf_count = 2,
     .dma_buf_len = 1024  //big buffers to avoid noises
};

void setup() 
{
  Serial.begin(115200);
  setCpuFrequencyMhz(240);                              //highest cpu frequency
  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);    //start i2s driver
  i2s_set_pin(i2s_num, NULL);                           //use internal DAC
  i2s_set_sample_rates(i2s_num, 1000000);               //dummy sample rate, since the function fails at high values

  //this is the hack that enables the highest sampling rate possible ~13MHz, have fun
  SET_PERI_REG_BITS(I2S_CLKM_CONF_REG(0), I2S_CLKM_DIV_A_V, 1, I2S_CLKM_DIV_A_S);
  SET_PERI_REG_BITS(I2S_CLKM_CONF_REG(0), I2S_CLKM_DIV_B_V, 1, I2S_CLKM_DIV_B_S);
  SET_PERI_REG_BITS(I2S_CLKM_CONF_REG(0), I2S_CLKM_DIV_NUM_V, 2, I2S_CLKM_DIV_NUM_S); 
  SET_PERI_REG_BITS(I2S_SAMPLE_RATE_CONF_REG(0), I2S_TX_BCK_DIV_NUM_V, 2, I2S_TX_BCK_DIV_NUM_S); 
}

//buffer to store modulated samples, I2S samples of the esp32 are always 16Bit
short buff[1024];

//sine represented in 16 values. at 13MHz sampling rate the resulting carrier is at around 835KHz
int sintab[] = {0, 48, 90, 117, 127, 117, 90, 48, 0, -48, -90, -117, -127, -117, -90, -48};

// a sine represented as 65 values gives a frequency of 200khz - 66 values gives 196khz. grumble, the current signal is 198khz, either should work.
// 65
int sintab200[] = {0, 12, 25, 37, 48, 59, 70, 80, 89, 98, 105, 112, 117, 122, 125, 127, 128, 128, 126, 124, 120, 115, 109, 102, 94, 85, 75, 65, 54, 42, 31, 18, 6, -6, -18, -31, -42, -54, -65, -75, -85, -94, -102, -109, -115, -120, -124, -126, -128, -128, -127, -125, -122, -117, -112, -105, -98, -89, -80, -70, -59, -48, -37, -25, -12 } ;

unsigned long long pos = 0;         //current position in the audio sample, using fixed point
unsigned int posLow = 0;
unsigned long long posInc = ((unsigned long long)sampleRate << 32) / 835000;  //sample fixed increment

void loop() 
{
  //fill the sound buffer
  for(int i = 0; i < 1024; i+=16)
  {
    if(posLow >= sampleCount) posLow = sampleCount - 1;
    //taking current sample
    int s = samples[posLow] + 128;
    //modulating that sample on the 16 values of the carrier wave (respect to I2S byte order, 16Bit/Sample)
    for(int j = 0; j < 16; j += 4)
    {          
      buff[i + j + 1] = (sintab[j + 0] * s + 0x8000);
      buff[i + j + 0] = (sintab[j + 1] * s + 0x8000);
      buff[i + j + 3] = (sintab[j + 2] * s + 0x8000);
      buff[i + j + 2] = (sintab[j + 3] * s + 0x8000);
    }
    pos += posInc;
    posLow = pos >> 32;
    if(posLow >= sampleCount)
      pos = posLow = 0;
  }
  //write the buffer (waits until a buffer is ready to be filled, that's timing for free)
  size_t bytes_written;
  i2s_write(i2s_num, (char*)buff, sizeof(buff), &bytes_written, portMAX_DELAY);
}
