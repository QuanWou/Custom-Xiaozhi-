#include <ESP_I2S.h>
#include <esp_heap_caps.h>
#include <string.h>

// Cảm ứng chạm
#define TOUCH_PIN 21

// Mic INMP441
#define MIC_SCK 16
#define MIC_WS  17
#define MIC_SD  18

// Amply MAX98357A
#define AMP_LRC  11
#define AMP_BCLK 12
#define AMP_DIN  13

constexpr int SAMPLE_RATE = 16000;
constexpr int BLOCK_FRAMES = 128;
constexpr int MIC_GAIN = 8;
constexpr unsigned long DEBOUNCE_MS = 35;

I2SClass mic(I2S_NUM_0);
I2SClass speaker(I2S_NUM_1);

int32_t micBuffer[BLOCK_FRAMES * 2];
int16_t *recorded = nullptr;

size_t capacity = 0;
size_t recordedCount = 0;

bool recording = false;
bool bufferFull = false;
bool stableTouch = false;
bool lastRawTouch = false;
unsigned long touchChangedAt = 0;

void startRecording() {
  recordedCount = 0;
  bufferFull = false;
  recording = true;
  Serial.println("Dang ghi am... Tha tay de phat.");
}

void playRecording() {
  if (recordedCount == 0) {
    Serial.println("Chua ghi duoc am thanh.");
    return;
  }

  Serial.printf("Dang phat %.1f giay am thanh...\n",
                (float)recordedCount / SAMPLE_RATE);

  // Thứ tự: BCLK, LRC, DIN
  speaker.setPins(AMP_BCLK, AMP_LRC, AMP_DIN);

  if (!speaker.begin(
        I2S_MODE_STD,
        SAMPLE_RATE,
        I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_STEREO
      )) {
    Serial.println("LOI: Khong khoi tao duoc loa.");
    return;
  }

  int16_t stereo[BLOCK_FRAMES * 2] = {};

  // Một đoạn im lặng trước khi phát.
  speaker.write(stereo, sizeof(stereo));

  for (size_t pos = 0; pos < recordedCount;) {
    size_t frames = min((size_t)BLOCK_FRAMES, recordedCount - pos);

    for (size_t i = 0; i < frames; i++) {
      int16_t value = recorded[pos + i];
      stereo[i * 2]     = value; // Kênh trái
      stereo[i * 2 + 1] = value; // Kênh phải
    }

    speaker.write(stereo, frames * 2 * sizeof(int16_t));
    pos += frames;
  }

  // Phát im lặng để kết thúc tiếng.
  memset(stereo, 0, sizeof(stereo));
  for (int i = 0; i < 10; i++) {
    speaker.write(stereo, sizeof(stereo));
  }

  speaker.end();
  Serial.println("Phat xong. Cham de ghi lan nua.");
}

void finishRecording() {
  recording = false;
  bufferFull = false;
  playRecording();
}

void recordBlock() {
  size_t bytesRead = mic.readBytes(
    (char *)micBuffer,
    sizeof(micBuffer)
  );

  // Một khung gồm 32 bit trái và 32 bit phải.
  size_t framesRead = bytesRead / (2 * sizeof(int32_t));

  for (size_t i = 0; i < framesRead; i++) {
    if (recordedCount >= capacity) {
      recording = false;
      bufferFull = true;
      Serial.println("Da day bo nho. Tha tay de phat.");
      return;
    }

    // L/R của mic nối GND: lấy kênh trái.
    int32_t value = (micBuffer[i * 2] >> 16) * MIC_GAIN;

    if (value > 30000) value = 30000;
    if (value < -30000) value = -30000;

    recorded[recordedCount++] = (int16_t)value;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  delay(700); // Để cảm ứng ổn định; không chạm khi vừa cấp nguồn.

  // Ưu tiên PSRAM: ghi tối đa 15 giây.
  capacity = SAMPLE_RATE * 15;
  recorded = (int16_t *)heap_caps_malloc(
    capacity * sizeof(int16_t),
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
  );

  // Nếu PSRAM chưa bật: dùng RAM trong, tối đa 3 giây.
  if (!recorded) {
    capacity = SAMPLE_RATE * 3;
    recorded = (int16_t *)heap_caps_malloc(
      capacity * sizeof(int16_t),
      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
    );
  }

  if (!recorded) {
    Serial.println("LOI: Khong du RAM de ghi am.");
    while (true) delay(1000);
  }

  // Thứ tự: SCK, WS, output không dùng, SD input.
  mic.setPins(MIC_SCK, MIC_WS, -1, MIC_SD);

  if (!mic.begin(
        I2S_MODE_STD,
        SAMPLE_RATE,
        I2S_DATA_BIT_WIDTH_32BIT,
        I2S_SLOT_MODE_STEREO
      )) {
    Serial.println("LOI: Khong khoi tao duoc mic.");
    while (true) delay(1000);
  }

  stableTouch = digitalRead(TOUCH_PIN);
  lastRawTouch = stableTouch;
  touchChangedAt = millis();

  Serial.printf("San sang. Ghi toi da %u giay.\n",
                (unsigned)(capacity / SAMPLE_RATE));
  Serial.println("Cham va giu de ghi, tha de phat.");
}

void loop() {
  bool rawTouch = digitalRead(TOUCH_PIN);

  if (rawTouch != lastRawTouch) {
    lastRawTouch = rawTouch;
    touchChangedAt = millis();
  }

  // Chỉ xử lý khi trạng thái chạm đã ổn định.
  if (rawTouch != stableTouch &&
      millis() - touchChangedAt >= DEBOUNCE_MS) {
    stableTouch = rawTouch;

    if (stableTouch) {
      startRecording();
    } else if (recording || bufferFull) {
      finishRecording();
    }
  }

  if (recording) {
    recordBlock();
  } else {
    delay(5);
  }
}