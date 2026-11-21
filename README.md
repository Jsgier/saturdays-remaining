# Saturdays Remaining

A desk clock that counts down an estimated lifetime in Saturdays. An Arduino Nano reads a battery-backed clock and draws a smoothed life-table distribution on a 128 × 64 OLED.

The display shows the estimate in fractional Saturdays and days. Every elapsed day subtracts one day, or 1/7 of a Saturday. The curve shifts left as time passes, and the dotted vertical line tracks the mean.

## Hardware

- Arduino Nano with an ATmega328P
- 128 × 64 I²C OLED with an SSD1306 controller
- DS3231 real-time clock (RTC) breakout with its supported backup battery
- USB data cable and jumper wires

Connect the OLED and RTC in parallel on the Nano’s I²C bus.

| Signal | ATmega328P Nano | OLED | DS3231 |
| --- | --- | --- | --- |
| SDA | A4 | SDA | SDA |
| SCL | A5 | SCL | SCL |
| Ground | GND | GND | GND |
| Power | Compatible supply rail | VCC or VIN | VCC or VIN |

Check each breakout’s supply voltage and I²C logic-level rating before connecting it to the Nano. The sketch uses OLED address `0x3C`. Set `kOledAddress` to `0x3D` if your module uses that address.

## Upload and set the clock

1. Open [SaturdaysRemaining.ino](SaturdaysRemaining/SaturdaysRemaining.ino) in the [Arduino IDE](https://www.arduino.cc/en/software/).
2. Install **Adafruit SSD1306**, **Adafruit GFX Library**, and **RTClib** by Adafruit using Library Manager. Accept the dependency prompts.
3. Connect the Nano with a USB data cable. Select **Tools → Board → Arduino AVR Boards → Arduino Nano**, then select its serial port and **ATmega328P** under **Tools → Processor**. If the port is present and upload fails with a sync error, try **ATmega328P (Old Bootloader)**.
4. Click **Upload**.
5. Open **Serial Monitor** at **115200 baud**, choose **Newline**, and send the current **UTC** time in `YYYY-MM-DD HH:MM:SS` format. The sketch replies with `RTC set.` and starts the display.

With a working backup battery, the DS3231 keeps time when USB power is disconnected. Set it again if the sketch reports that the clock has lost power.

## How the estimate works

The default starts at **2,244.8 Saturdays** on the configured 35th birthday. The sketch subtracts elapsed time from that fixed starting estimate, using seven 24-hour days per Saturday. At zero, the numeric countdown stops.

The curve comes from the male column of the [Social Security Administration’s 2023 period life table](https://www.ssa.gov/OACT/STATS/table4c6.html), conditional on reaching age 35. For each yearly age interval, the probability comes from the drop in survivor counts across that interval divided by the number of survivors at age 35. The sketch stores samples of the smoothed curve in flash. This is a population estimate, and the countdown uses the same age-35 baseline throughout.

## Configuration

- `birthdayUtc` sets the UTC start date and time.
- `kStartingTenthsOfASaturday` sets the initial numeric estimate in tenths of a Saturday. The default `22448` means 2,244.8.
- `curveSamples` holds the shape of the age-35 distribution. If you change the starting estimate to use a different life table or starting age, regenerate these samples from the corresponding distribution to keep the plot and its mean aligned.

The countdown math and drawing bounds were checked in a host-side test. An Arduino build and a check on physical hardware are still pending.
