// ⚠️ REPLACE THIS HEADER WITH YOUR ACTUAL EDGE IMPULSE PROJECT HEADER!
#include <ESP32S3-Voice-Command_inferencing.h>

#include <driver/i2s.h>

// --- ESP32-S3 I2S PIN DEFINITIONS ---
#define I2S_WS   15  // LRCLK
#define I2S_SD   16  // SD
#define I2S_SCK  14  // BCLK
#define I2S_PORT I2S_NUM_0

// --- HARDWARE POWER PROFILING TRIGGER PIN ---
#define PROFILING_PIN 4  // Toggles HIGH during NN inference for PPK2 energy measurement

static bool debug_nn = false;

// Fixed: Edge Impulse expects a float array for signal_from_buffer
static float sample_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

void i2s_init() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = EI_CLASSIFIER_FREQUENCY, // 16000 Hz
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_start(I2S_PORT);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  pinMode(PROFILING_PIN, OUTPUT);
  digitalWrite(PROFILING_PIN, LOW);

  i2s_init();

  Serial.println("==================================================");
  Serial.println("⚡ TinyML Energy & Latency Benchmarking Initialized!");
  Serial.println("==================================================");
}

void loop() {
  // 1. Capture 1 second of audio samples into float buffer
  int32_t i2s_raw_buffer[64];
  size_t bytes_read = 0;
  int samples_collected = 0;

  while (samples_collected < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
    esp_err_t result = i2s_read(I2S_PORT, &i2s_raw_buffer, sizeof(i2s_raw_buffer), &bytes_read, portMAX_DELAY);
    if (result == ESP_OK && bytes_read > 0) {
      int samples = bytes_read / sizeof(int32_t);
      for (int i = 0; i < samples && samples_collected < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i++) {
        // Convert 32-bit raw I2S sample to float scale for Edge Impulse DSP
        sample_buffer[samples_collected++] = (float)(i2s_raw_buffer[i] >> 14);
      }
    }
  }

  // 2. Wrap float buffer into Edge Impulse signal
  signal_t signal;
  numpy::signal_from_buffer(sample_buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

  ei_impulse_result_t result = { 0 };

  // 3. Hardware Profiling & Microsecond Latency Measurement
  // digitalWrite(PROFILING_PIN, HIGH);  // Pin goes HIGH (3.3V) during active inference
  
  // uint32_t start_us = micros();
  // EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
  // uint32_t total_latency_us = micros() - start_us;
  
  // digitalWrite(PROFILING_PIN, LOW);   // Pin drops LOW (0V) when inference completes
  // --- TEMPORARY TEST TO WIDEN D0 PULSE ---
digitalWrite(PROFILING_PIN, HIGH);  // Pin 4 goes HIGH
delay(100);                         // Hold HIGH for 100ms so it creates a wide square wave

uint32_t start_us = micros();
EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
uint32_t total_latency_us = micros() - start_us;

digitalWrite(PROFILING_PIN, LOW);   // Pin 4 drops LOW

  if (r != EI_IMPULSE_OK) {
    Serial.printf("ERR: Failed to run classifier (%d)\n", r);
    return;
  }

  // 4. Output Benchmark Data
  Serial.println("\n--------------------------------------------------");
  Serial.printf("⏱️  DSP/MFCC Latency:  %d ms\n", result.timing.dsp);
  Serial.printf("⏱️  NN Inference Time: %d ms (%d µs)\n", result.timing.classification, total_latency_us);
  Serial.printf("⚡ Total Pipeline Time: %d ms\n", result.timing.dsp + result.timing.classification);
  Serial.println("--------------------------------------------------");

  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    Serial.printf("  %s: %.2f%%\n", result.classification[ix].label, result.classification[ix].value * 100.0f);
  }

  delay(1000);
}