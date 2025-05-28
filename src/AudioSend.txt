#include <Arduino.h>
#include "driver/i2s.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"

// --- Wi-Fi 설정 ---
const char* ssid = "AtoZ_LAB";      // 여기에 사용하는 Wi-Fi SSID를 입력하세요.
const char* password = "atoz9897!";  // 여기에 Wi-Fi 비밀번호를 입력하세요.

// --- 업로드 서버 API 정보 ---
const char* upload_server = "192.168.219.106";
const int   upload_port   = 8080;
// HTML form의 action 속성에 따라 실제 업로드 경로를 지정합니다.
const char* upload_path   = "/FarmData/fileUpload.do"; 
const char* device_id     = "PKJSDEVICE"; // 장치명

// --- NTP(시간 서버) 설정 ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 9 * 3600; // 한국 표준시(UTC+9)
const int   daylightOffset_sec = 0;

// --- I2S 및 오디오 설정 ---
#define I2S_WS_PIN    41  // Word Select
#define I2S_SD_PIN    42  // Serial Data
#define I2S_SCK_PIN   40  // Serial Clock
#define I2S_PORT      I2S_NUM_0

const int SAMPLE_RATE = 44100;
const int BIT_DEPTH = 16;
const int NUM_CHANNELS = 2;
const int RECORD_SECONDS = 30; // 녹음 시간(초)

// WAV 파일 관련 상수
const int WAV_HEADER_SIZE = 44;
const uint32_t AUDIO_DATA_SIZE = RECORD_SECONDS * SAMPLE_RATE * NUM_CHANNELS * (BIT_DEPTH / 8);

// 오디오 데이터를 저장할 PSRAM 버퍼
int16_t* audio_buffer_psram = NULL;
byte wav_header[WAV_HEADER_SIZE];

// --- 함수 프로토타입 선언 ---
void initI2S();
void createWavHeader(byte* header, uint32_t audioDataSize);
void recordAudio();
void uploadWavFile();


// =========================================================================
// ===                          SETUP                                    ===
// =========================================================================
void setup(){
  Serial.begin(115200);
  delay(5000);
  Serial.println("Audio Upload Program Started");

  // 1. Wi-Fi 연결
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // 2. NTP 서버로부터 시간 정보 가져오기
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  char time_str[20];
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
  Serial.print("Current Time: ");
  Serial.println(time_str);

  // 3. PSRAM 초기화 및 오디오 버퍼 할당
  if (!psramInit()){
    Serial.println("PSRAM not found");
    while(1);
  }
  audio_buffer_psram = (int16_t*)ps_malloc(AUDIO_DATA_SIZE);
  if (audio_buffer_psram == NULL) {
    Serial.println("PSRAM buffer allocation FAILED!");
    while (1);
  }
  Serial.println("PSRAM audio buffer allocated successfully.");

  // 4. I2S 초기화 및 녹음
  initI2S();
  recordAudio();

  // 5. WAV 헤더 생성
  createWavHeader(wav_header, AUDIO_DATA_SIZE);

  // 6. 서버로 WAV 파일 업로드 (세션 과정 없이 바로 업로드)
  uploadWavFile();

  Serial.println("\n--- Process Finished ---");
}


// =========================================================================
// ===                           LOOP                                    ===
// =========================================================================
void loop() {
  // 모든 작업은 setup()에서 한 번만 실행되므로 loop는 비워둡니다.
  delay(10000);
}


// =========================================================================
// ===                        함수 정의                                  ===
// =========================================================================

/**
 * @brief I2S 드라이버를 초기화합니다.
 */
void initI2S() {
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

/**
 * @brief WAV 파일 헤더를 생성합니다.
 * @param header 헤더를 저장할 바이트 배열
 * @param audioDataSize 오디오 데이터의 전체 크기
 */
void createWavHeader(byte* header, uint32_t audioDataSize) {
  uint32_t fileSize = audioDataSize + WAV_HEADER_SIZE - 8;
  uint32_t byteRate = SAMPLE_RATE * NUM_CHANNELS * (BIT_DEPTH / 8);
  uint16_t blockAlign = NUM_CHANNELS * (BIT_DEPTH / 8);

  header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
  memcpy(&header[4], &fileSize, 4);
  header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';
  header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
  header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0;
  header[20] = 1; header[21] = 0;
  header[22] = NUM_CHANNELS; header[23] = 0;
  memcpy(&header[24], &SAMPLE_RATE, 4);
  memcpy(&header[28], &byteRate, 4);
  memcpy(&header[32], &blockAlign, 2);
  header[34] = BIT_DEPTH; header[35] = 0;
  header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
  memcpy(&header[40], &audioDataSize, 4);
  
  Serial.println("WAV header created.");
}


/**
 * @brief I2S 마이크로부터 오디오를 녹음하여 PSRAM 버퍼에 저장합니다.
 */
void recordAudio() {
  if (audio_buffer_psram == NULL) {
    Serial.println("PSRAM buffer not allocated!");
    return;
  }
  Serial.printf("Starting %d seconds recording...\n", RECORD_SECONDS);
  i2s_start(I2S_PORT);

  size_t total_bytes_written_to_psram = 0;
  const int i2s_read_buffer_size = 4096;
  int8_t* i2s_read_buffer = (int8_t*)malloc(i2s_read_buffer_size);

  if(i2s_read_buffer == NULL){
    Serial.println("Failed to allocate I2S read buffer!");
    return;
  }

  while (total_bytes_written_to_psram < AUDIO_DATA_SIZE) {
    size_t bytes_read_from_i2s = 0;
    i2s_read(I2S_PORT, i2s_read_buffer, i2s_read_buffer_size, &bytes_read_from_i2s, pdMS_TO_TICKS(1000));
    
    if (bytes_read_from_i2s > 0) {
      int samples_to_process = bytes_read_from_i2s / 4;
      for (int i = 0; i < samples_to_process; i++) {
        if (total_bytes_written_to_psram < AUDIO_DATA_SIZE) {
          int32_t sample32 = ((int32_t*)i2s_read_buffer)[i];
          audio_buffer_psram[total_bytes_written_to_psram / 2] = (int16_t)(sample32 >> 16);
          total_bytes_written_to_psram += 2;
        } else {
          break;
        }
      }
    }
  }

  i2s_stop(I2S_PORT);
  free(i2s_read_buffer);
  Serial.println("Recording finished.");
  Serial.printf("Total bytes written to PSRAM buffer: %u / %u\n", total_bytes_written_to_psram, AUDIO_DATA_SIZE);
}

/**
 * @brief 녹음된 WAV 파일을 서버에 업로드합니다. (최종 수정 버전)
 */
void uploadWavFile() {
    if (WiFi.status() != WL_CONNECTED || audio_buffer_psram == NULL) {
        Serial.println("WiFi not connected or Audio buffer is empty. Cannot upload.");
        return;
    }

    Serial.println("\n--- Starting Final File Upload ---");

    WiFiClient client;
    
    Serial.printf("Connecting to server: %s:%d\n", upload_server, upload_port);
    if (!client.connect(upload_server, upload_port)) {
        Serial.println("Connection to server FAILED!");
        return;
    }
    Serial.println("Connection SUCCESSFUL.");

    String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    
    // -- 폼 데이터 각 파트 생성 --
    String head = "--" + boundary + "\r\n" +
                  "Content-Disposition: form-data; name=\"i\"\r\n\r\n" +
                  String(device_id) + "\r\n";

    struct tm timeinfo;
    getLocalTime(&timeinfo);
    char time_buffer[15];
    strftime(time_buffer, sizeof(time_buffer), "%Y%m%d%H%M%S", &timeinfo);
    
    head += "--" + boundary + "\r\n" +
            "Content-Disposition: form-data; name=\"d\"\r\n\r\n" +
            String(time_buffer) + "\r\n";
    String filename = String(device_id) + ".wav";
    head += "--" + boundary + "\r\n" +
            "Content-Disposition: form-data; name=\"awfile\"; filename=\"" + filename + "\"\r\n" +
            "Content-Type: audio/wav\r\n\r\n";

    String tail = "\r\n--" + boundary + "--\r\n";
    
    // -- 전체 컨텐츠 길이 계산 --
    uint32_t contentLength = head.length() + WAV_HEADER_SIZE + AUDIO_DATA_SIZE + tail.length();

    // -- HTTP POST 요청 헤더 작성 --
    client.println(String("POST ") + upload_path + " HTTP/1.1");
    client.println(String("Host: ") + upload_server);
    client.println("Connection: close");
    client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    client.println(String("Content-Length: ") + contentLength);
    client.println();

    // -- 페이로드 전송 --
    Serial.println("Sending payload...");
    client.print(head);
    client.write((const byte*)wav_header, WAV_HEADER_SIZE);
    client.write((const byte*)audio_buffer_psram, AUDIO_DATA_SIZE);
    client.print(tail);
    Serial.println("Payload sent.");

    // -- 서버 응답 확인 (5초 타임아웃) --
    Serial.println("Waiting for server response...");
    unsigned long timeout = millis();
    while (!client.available() && millis() - timeout < 5000) {
        delay(10);
    }
    
    if (!client.available()) {
        Serial.println("No response from server.");
    } else {
        Serial.println("--- Server Response ---");
        while(client.available()){
            String line = client.readStringUntil('\n');
            Serial.println(line);
        }
        Serial.println("-----------------------");
    }

    client.stop();
    Serial.println("Upload process finished.");
}