#include <Arduino.h>
#include <driver/i2s.h>

// I2S 핀 설정 (이전과 동일)
#define I2S_WS 41
#define I2S_SD 42
#define I2S_SCK 40
#define I2S_PORT I2S_NUM_0

// 오디오 녹음 설정 (PC쪽 파이썬 코드와 일치해야 함)
const int sampleRate = 16000;
const int bitDepth = 16;
const int record_time = 30; // 녹음 시간 (초)

void setup() {
  Serial.begin(115200); // PC와 통신할 속도 설정
  Serial.println("PC로 오디오 데이터 전송을 시작하려면 ESP32의 RST 버튼을 누르세요.");

  // I2S 드라이버 설정 (이전과 동일)
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = sampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false
  };

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);

  Serial.println("녹음을 시작합니다. 30초간 데이터를 전송합니다...");
  
  // 지정된 시간 동안 오디오 데이터를 읽고 시리얼 포트로 전송
  size_t bytesRead;
  int32_t sample_32bit;
  int16_t sample_16bit;
  unsigned long startTime = millis();
  
  while(millis() - startTime < record_time * 1000) {
    // I2S에서 오디오 데이터 읽기
    i2s_read(I2S_PORT, &sample_32bit, sizeof(sample_32bit), &bytesRead, portMAX_DELAY);
    if (bytesRead > 0) {
      // 32비트 샘플을 16비트로 변환
      sample_16bit = sample_32bit >> 16;
      // 16비트(2바이트) 샘플을 시리얼로 전송
      Serial.write((uint8_t *)&sample_16bit, sizeof(sample_16bit));
    }
  }

  Serial.println("녹음 완료! 데이터 전송을 중지합니다.");

  // I2S 드라이버 제거
  i2s_driver_uninstall(I2S_PORT);
}

void loop() {
  // 모든 작업은 setup()에서 완료되므로 loop()는 비워둡니다.
  // ESP32가 재시작될 때마다 녹음이 다시 시작됩니다.
}