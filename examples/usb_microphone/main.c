/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This examples creates a USB Microphone device using the TinyUSB
 * library and captures data from a PDM microphone using a sample
 * rate of 16 kHz, to be sent the to PC.
 * 
 * The USB microphone code is based on the TinyUSB audio_test example.
 * 
 * https://github.com/hathach/tinyusb/tree/master/examples/device/audio_test
 */

#include "pico/analog_microphone.h"

#include "usb_microphone.h"

#define RING_BUFFER_SAMPLES (SAMPLE_BUFFER_SIZE * 4)

static int16_t ring_buffer[RING_BUFFER_SAMPLES];
static volatile uint32_t rb_write = 0;
static volatile uint32_t rb_read  = 0;

//#include "analog_microphone.h"
//#include "pdm_microphone.h"

void on_analog_samples_ready(void); // Forward declaration

// configuration
const struct analog_microphone_config config = {
    .gpio = 26,              // ADC-compatible GPIO (26-28)
    .bias_voltage = 1.25,    // Adjust based on your mic (MAX9814 typically 1.25V)
    .sample_rate = 48000,    // 48 kHz
    .sample_buffer_size = SAMPLE_BUFFER_SIZE
};

// variables
//uint16_t sample_buffer[SAMPLE_BUFFER_SIZE];
__attribute__((aligned(4))) uint16_t sample_buffer[SAMPLE_BUFFER_SIZE];

// callback functions
//void on_pdm_samples_ready();
void on_usb_microphone_tx_ready();

int main(void)
{
  // initialize and start the analog microphone
  analog_microphone_init(&config);
  analog_microphone_set_samples_ready_handler(on_analog_samples_ready);
  analog_microphone_start();

  // initialize the USB microphone interface
  usb_microphone_init();
  usb_microphone_set_tx_ready_handler(on_usb_microphone_tx_ready);

  while (1) {
    // run the USB microphone task continuously
    usb_microphone_task();
  }

  return 0;
}

void on_analog_samples_ready()
{
    int16_t temp[SAMPLE_BUFFER_SIZE];

    analog_microphone_read(temp, SAMPLE_BUFFER_SIZE);

    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++)
    {
        // Convert ADC → signed PCM16
        int16_t pcm = ((int16_t)temp[i] - 2048) << 4;

        uint32_t next = (rb_write + 1) % RING_BUFFER_SAMPLES;

        // Drop sample if buffer full (prevents overwrite)
        if (next != rb_read) {
            ring_buffer[rb_write] = pcm;
            rb_write = next;
        }
    }
}

/*
 *   void on_analog_samples_ready()
*{
 *   // callback from library when all the samples in the library
 *   // internal sample buffer are ready for reading 
 *   samples_read = analog_microphone_read(sample_buffer, SAMPLE_BUFFER_SIZE);
*}
 */

//void on_pdm_samples_ready()
//{
  // Callback from library when all the samples in the library
  // internal sample buffer are ready for reading.
  //
  // Read new samples into local buffer.
  //pdm_microphone_read(sample_buffer, SAMPLE_BUFFER_SIZE);
//}

void on_usb_microphone_tx_ready()
{
    static uint8_t usb_buf[CFG_TUD_AUDIO_EP_SZ_IN];

    usb_buf[0] = 0x00; // UAC2 header

    int16_t *pcm = (int16_t *)&usb_buf[1];

    uint32_t available =
        (rb_write >= rb_read)
        ? (rb_write - rb_read)
        : (RING_BUFFER_SAMPLES - rb_read + rb_write);

    uint32_t needed = SAMPLE_BUFFER_SIZE;

    for (uint32_t i = 0; i < needed; i++)
    {
        if (available > 0)
        {
            pcm[i] = ring_buffer[rb_read];
            rb_read = (rb_read + 1) % RING_BUFFER_SAMPLES;
            available--;
        }
        else
        {
            pcm[i] = 0; // underrun → silence
        }
    }

    usb_microphone_write(usb_buf, CFG_TUD_AUDIO_EP_SZ_IN);
}


