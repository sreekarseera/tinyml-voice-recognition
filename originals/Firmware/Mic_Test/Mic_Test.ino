#include <driver/i2s.h>

// --- ESP32-S3 I2S PIN DEFINITIONS ---
#define I2S_WS   15  // Word Select (LRCLK)
#define I2S_SD   16  // Serial Data (SD)
#define I2S_SCK  14  // Serial Clock (BCLK)
#define I2S_PORT I2S_NUM_0

#define bufferLen 64
int32_t sBuffer[bufferLen];

void i2s_install() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000, // 16kHz audio sampling rate
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = bufferLen,
    .use_apll = false
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  // Fixed: Correct ESP-IDF function call
  i2s_set_pin(I2S_PORT, &pin_config);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);

  Serial.println("🎙️ INMP441 Microphone Test Started on ESP32-S3!");
}

void loop() {
  size_t bytesIn = 0;
  esp_err_t result = i2s_read(I2S_PORT, &sBuffer, sizeof(sBuffer), &bytesIn, portMAX_DELAY);

  if (result == ESP_OK) {
    int samplesRead = bytesIn / sizeof(int32_t);
    if (samplesRead > 0) {
      float mean = 0;
      for (int i = 0; i < samplesRead; i++) {
        int16_t sample16 = sBuffer[i] >> 14; 
        mean += abs(sample16);
      }
      mean /= samplesRead; // Average sound amplitude

      // Output for Serial Plotter
      Serial.print("SoundLevel:");
      Serial.println(mean);
    }
  }
}