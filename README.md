# Women's Safety IoT System

Complete project for an ESP32 wearable safety device with:
- PHP API backend
- MySQL database schema
- Web monitoring dashboard
- Mobile PWA companion
- ESP32 firmware

## Project Structure

- `api/` PHP endpoints
- `database/schema.sql` MySQL schema + demo data
- `dashboard/` Bootstrap monitoring UI
- `mobile_app/` PWA SOS app
- `esp32_firmware/main_safety_device.ino` firmware

## Quick Start (Windows + XAMPP)

1. Install XAMPP and start `Apache` + `MySQL`.
2. Copy folder `womens_safety` to `C:\xampp\htdocs\womens_safety`.
3. Open phpMyAdmin and import `database/schema.sql`.
4. Edit `api/config.php` if your DB credentials differ from:
   - Host: `localhost`
   - User: `root`
   - Password: ``
   - DB: `womens_safety_db`
5. Open:
   - Dashboard: `http://localhost/womens_safety/dashboard/`
   - Mobile app: `http://localhost/womens_safety/mobile_app/`

## Default Wife Login + Device

- Username: `wife@example.com`
- Password: `password`
- Device ID: `WIFE_ESP32_001`
- ESP32 API URL: `http://192.168.1.100/womens_safety/api/` (replace with your PC IP)
- Hardware: ESP32, MAX30102, DHT11 temperature sensor, SOS button, vibration sensor, GPS, GSM

## ESP32 Pin Map

- MAX30102: SDA `GPIO21`, SCL `GPIO22`, 3.3V, GND
- DHT11 temperature: DATA `GPIO33`, 3.3V, GND
- SOS button: `GPIO25` to GND, uses internal pull-up
- Vibration sensor: digital OUT to `GPIO35`
- VC-02 voice module: VC-02 TX to ESP32 `GPIO17`, VC-02 RX to ESP32 `GPIO16`, common GND
- GPS: GPS TX to ESP32 `GPIO18`, GPS RX to ESP32 `GPIO19`
- GSM SIM800L: GSM TX to ESP32 `GPIO32`, GSM RX to ESP32 `GPIO23`
- Buzzer: `GPIO26`
- LEDs: red `GPIO27`, green `GPIO14`, blue `GPIO13`
- Battery divider: `GPIO34`

### SW-420 vibration quick check

For isolated testing, flash `esp32_firmware/sw420_diagnostic/sw420_diagnostic.ino` first.
Power the SW-420 from `3.3V`, connect `DO` to `GPIO35`, and share `GND`.
After flashing, open Serial Monitor at `115200` baud. You should see a ready or
calibrated message and then `Vibration detected` when you tap the module. If it
never detects taps, slowly turn the SW-420 potentiometer until the module LED
changes state during a tap.

### MAX30102 quick check

Open Serial Monitor at `115200` baud after flashing the ESP32 firmware.

- For isolated testing, flash `esp32_firmware/max30102_diagnostic/max30102_diagnostic.ino` first.
- `I2C device found at 0x57` means wiring is detected.
- `No I2C devices found` usually means VCC/GND/SDA/SCL are wrong, loose, or the module has no pull-up resistors.
- `MAX30102 detected on I2C, but library initialization failed` usually means the wrong MAX3010x library is installed.
- If the sensor initializes but shows `waiting for finger`, place your finger fully over the red/IR LEDs and keep it still for a few seconds.

### DHT11 temperature quick check

Install the Arduino `DHT sensor library` and `Adafruit Unified Sensor` library before compiling.

- `DHT11 body temperature: ... C` means the sensor is reading correctly.
- `DHT11 temperature read failed` usually means DATA/VCC/GND wiring is wrong, or the DATA pin needs a 10k pull-up resistor to 3.3V.

### VC-02 voice recognition quick check

For isolated testing, flash `esp32_firmware/vc02_diagnostic/vc02_diagnostic.ino` first.

- Open Serial Monitor at `115200` baud. The diagnostic sketch cycles common VC-02 baud rates and both GPIO16/GPIO17 directions; repeat the trained command when each test line is shown.
- If you see `VC-02 RX: ...`, the ESP32 is receiving UART data from the VC-02.
- `VC-02 RX: 0x00` means the line is active but the ESP32 is not receiving a useful command byte yet.
- If you only see `0x00`, check that you are using the VC-02 UART TX pin, not an OUT/IO pin; also confirm the VC-02 is configured to send serial command bytes.
- If nothing prints, check power, common GND, baud rate, and try swapping VC-02 TX/RX wiring.
- The main firmware currently treats these VC-02 command IDs as emergency commands:
  - `0x01` = Help Me
  - `0x02` = Emergency
  - `0x03` = Save Me
  - `0x04` = Danger
  - `0x05` = Call Police
- If your diagnostic output shows different command bytes, update the `VC02_*_COMMAND` values in the firmware.

## API Smoke Tests

Use Postman/cURL:

1. Register user  
   `POST /api/auth.php?action=register`
2. Login  
   `POST /api/auth.php?action=login`
3. Push sensor data  
   `POST /api/update_device.php`
4. Trigger emergency  
   `POST /api/emergency_alert.php`
5. Dashboard stats  
   `GET /api/dashboard_data.php?action=overview`

## Firmware Setup Notes

Update in `esp32_firmware/main_safety_device.ino`:
- `WIFI_SSID`
- `WIFI_PASSWORD`
- `SERVER_URL`
- `DEVICE_ID`

## Important Fixes Applied

- Added missing `sensor_data.alert_type` column expected by API.
- Fixed MySQL stored procedure definitions for XAMPP compatibility.
- Added missing endpoint `api/cancel_alert.php`.
- Added complete `mobile_app` PWA starter files.

## Troubleshooting

- Device data not updating:
  - Verify the ESP32 `SERVER_URL` uses your PC IP address, not `localhost`.
  - Ensure `DEVICE_ID` exists in the `devices` table.
- Database import errors on procedures:
  - Re-import updated `database/schema.sql`.
- Dashboard shows demo/simulated data:
  - Confirm API path is reachable and DB has records.

Detailed setup guide: `docs/SETUP_GUIDE.md`.
