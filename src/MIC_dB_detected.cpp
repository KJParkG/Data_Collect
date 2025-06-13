#include "driver/i2s.h"
#include <Arduino.h>

#define I2S_WS_PIN  41  // Word Select
#define I2S_SD_PIN  42  // Serial Data
#define I2S_SCK_PIN 40  // Serial Clock
#define I2S_PORT    I2S_NUM_0

#define SAMPLE_RATE 44100 // 초당 샘플링 횟수
#define SAMPLE_BITS I2S_BITS_PER_SAMPLE_32BIT
#define SAMPLES_TO_READ 1024

#define MIC_SENSITIVITY -26.0
#define FULL_SCALE_32_BIT 2147483647.0

void initI2S();

void setup(){
    Serial.begin(115200);
    Serial.println("dB check");
    initI2S();
}

void loop(){
    int32_t samples[SAMPLES_TO_READ];
  size_t bytes_read = 0;

  esp_err_t result = i2s_read(I2S_PORT, &samples, sizeof(samples), &bytes_read, portMAX_DELAY);

  if (result == ESP_OK) {
    
    if (bytes_read > 0) {
      double sum_sq = 0;
      int samples_read = bytes_read / sizeof(int32_t);

      
      for (int i = 0; i < samples_read; i++) {
        int32_t sample = samples[i] >> 8; 
        
        double normalized_sample = (double)sample / FULL_SCALE_32_BIT;
        sum_sq += normalized_sample * normalized_sample;
      }
      
      double rms = sqrt(sum_sq / samples_read);

      if (rms < 1e-6) {
        rms = 1e-6;
      }

      double db = 20 * log10(rms) + MIC_SENSITIVITY + 94;

      Serial.printf("RMS: %.6f, 데시벨(dBA): %.2f dB\n", rms, db);
    }
  } else {
    Serial.printf("I2S 읽기 오류: %d\n", result);
  }

  delay(100); // 0.1초마다 측정
}

void initI2S() { // I2S 드라이버 초기화 함수
    Serial.println("I2S Initializing...");
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_PIN
    };
    i2s_set_pin(I2S_PORT, &pin_config);
    Serial.println("I2S Initialized.");
}