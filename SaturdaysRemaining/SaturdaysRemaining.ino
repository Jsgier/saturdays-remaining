#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

// Arduino Uno/Nano (ATmega328P), 128x64 I2C SSD1306, and DS3231.
// Set the RTC in Serial Monitor at 115200 baud with Newline selected.
// Send the current UTC time as YYYY-MM-DD HH:MM:SS.
constexpr uint8_t kOledAddress = 0x3C;  // Change to 0x3D for displays at that address.
constexpr uint32_t kSecondsPerDay = 86400UL;
constexpr uint32_t kSecondsPerWeek = 604800UL;
constexpr uint16_t kStartingTenthsOfASaturday = 22448;
constexpr uint32_t kStartingSeconds = kStartingTenthsOfASaturday * (kSecondsPerWeek / 10);
constexpr uint16_t kChartMax = 4000;
constexpr uint8_t kSampleSpacing = 32;

Adafruit_SSD1306 oled(128, 64, &Wire, -1);
RTC_DS3231 rtc;
const DateTime birthdayUtc(2026, 11, 7, 8, 0, 0);  // Midnight Pacific Standard Time.
bool clockValid = false;
char timeInput[20];
uint8_t inputLength = 0;
bool inputOverflow = false;

// Smoothed 2023 SSA male life-table probabilities at age 35.
// Heights are scaled to 255 and stored in flash to save Uno SRAM.
const uint8_t curveSamples[] PROGMEM = {
  19, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25,
  26, 27, 27, 28, 28, 29, 30, 30, 31, 33, 34, 35, 37, 38,
  40, 42, 44, 46, 48, 50, 52, 55, 57, 60, 63, 66, 69, 72,
  75, 77, 81, 84, 87, 90, 94, 97, 100, 103, 106, 109, 112, 116,
  119, 123, 126, 130, 134, 138, 143, 147, 152, 157, 162, 167, 174, 182,
  188, 195, 202, 208, 215, 221, 227, 233, 237, 241, 245, 248, 251, 253,
  255, 255, 255, 253, 249, 244, 236, 228, 217, 206, 194, 181, 167, 152,
  137, 122, 106, 92, 78, 65, 53, 42, 34, 27, 21, 16, 12, 9,
  7, 5, 4, 3, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0
};

uint32_t elapsedSeconds(uint32_t now, uint32_t start) {
  return now > start ? now - start : 0;
}

uint32_t remainingSeconds(uint32_t elapsed) {
  return elapsed < kStartingSeconds ? kStartingSeconds - elapsed : 0;
}

float curveHeight(float originalSaturdays) {
  if (originalSaturdays >= kChartMax) return 0;
  const float index = originalSaturdays / kSampleSpacing;
  const uint8_t lower = static_cast<uint8_t>(index);
  const uint8_t a = pgm_read_byte(&curveSamples[lower]);
  const uint8_t b = pgm_read_byte(&curveSamples[lower + 1]);
  return a + (b - static_cast<float>(a)) * (index - lower);
}

void drawCountdown(uint32_t elapsed) {
  const uint32_t remaining = remainingSeconds(elapsed);
  const float weeksRemaining = remaining / static_cast<float>(kSecondsPerWeek);
  const float weeksElapsed = elapsed / static_cast<float>(kSecondsPerWeek);
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.setTextSize(2);
  oled.print(weeksRemaining, 2);
  oled.setTextSize(1);
  oled.setCursor(104, 6);
  oled.print(F("SAT"));

  oled.setCursor(0, 18);
  oled.print(remaining / static_cast<float>(kSecondsPerDay), 1);
  oled.print(F(" days left"));

  constexpr uint8_t baseline = 54;
  if (remaining > 0) {
    const uint8_t meanX = static_cast<uint8_t>(
        weeksRemaining * 127 / kChartMax);
    for (uint8_t y = 29; y < baseline; y += 3) oled.drawPixel(meanX, y, SSD1306_WHITE);
  }

  int16_t previousY = baseline;
  for (int16_t x = 0; x < 128; ++x) {
    const float originalSaturdays = x * (kChartMax / 127.0f) + weeksElapsed;
    const int16_t y = baseline - static_cast<int16_t>(curveHeight(originalSaturdays) * 25 / 255 + .5f);
    if (x == 0) oled.drawPixel(x, y, SSD1306_WHITE);
    else oled.drawLine(x - 1, previousY, x, y, SSD1306_WHITE);
    previousY = y;
  }
  oled.drawFastHLine(0, baseline, 128, SSD1306_WHITE);
  oled.setCursor(0, 56);
  oled.print(F("0"));
  oled.setCursor(104, 56);
  oled.print(F("4000"));
  oled.display();
}

void setClockFromInput() {
  if (inputOverflow || inputLength != 19) {
    Serial.println(F("Enter UTC as YYYY-MM-DD HH:MM:SS."));
    return;
  }
  const char pattern[] = "####-##-## ##:##:##";
  for (uint8_t i = 0; i < 19; ++i) {
    const char c = timeInput[i];
    if (pattern[i] == '#' ? c < '0' || c > '9' : c != pattern[i]) {
      Serial.println(F("Enter UTC as YYYY-MM-DD HH:MM:SS."));
      return;
    }
  }
  if (timeInput[0] != '2' || timeInput[1] != '0') {
    Serial.println(F("Use a year from 2026 to 2099."));
    return;
  }
  timeInput[19] = '\0';
  timeInput[10] = 'T';
  DateTime incoming(timeInput);
  if (incoming.year() < 2026 || !incoming.isValid()) {
    Serial.println(F("Invalid UTC date/time."));
    return;
  }
  rtc.adjust(incoming);
  clockValid = true;
  Serial.println(F("RTC set."));
}

void readClockInput() {
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      setClockFromInput();
      inputLength = 0;
      inputOverflow = false;
    } else if (inputLength < sizeof(timeInput) - 1) {
      timeInput[inputLength++] = c;
    } else {
      inputOverflow = true;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  if (!oled.begin(SSD1306_SWITCHCAPVCC, kOledAddress)) {
    Serial.println(F("OLED initialization failed."));
    while (true) delay(1000);
  }
  oled.setTextColor(SSD1306_WHITE);
  oled.clearDisplay();
  oled.setCursor(0, 0);

  if (!rtc.begin()) {
    oled.print(F("DS3231 not found"));
    oled.display();
    while (true) delay(1000);
  }
  clockValid = !rtc.lostPower() && rtc.now().year() >= 2026;
  if (!clockValid) {
    oled.println(F("Set RTC over serial"));
    oled.println(F("UTC, 115200 baud"));
    oled.display();
  }
  Serial.println(F("To set RTC, send UTC as YYYY-MM-DD HH:MM:SS with newline."));
}

void loop() {
  readClockInput();
  if (clockValid) drawCountdown(elapsedSeconds(rtc.now().unixtime(), birthdayUtc.unixtime()));
  delay(1000);
}
