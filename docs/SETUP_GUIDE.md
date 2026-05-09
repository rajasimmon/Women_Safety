# Step-by-Step Setup and Error Check Guide

## 1. Install Required Software

1. Install XAMPP (Apache + MySQL).
2. Install Arduino IDE (for ESP32 upload).
3. Install ESP32 board package in Arduino IDE.

## 2. Place Project in XAMPP

1. Copy the project folder to:
   `C:\xampp\htdocs\womens_safety`
2. Start `Apache` and `MySQL` from XAMPP Control Panel.

## 3. Create Database

1. Open `http://localhost/phpmyadmin`.
2. Import:
   `C:\xampp\htdocs\womens_safety\database\schema.sql`
3. Confirm DB `womens_safety_db` is created.

## 4. Configure Backend

1. Open:
   `C:\xampp\htdocs\womens_safety\api\config.php`
2. Verify DB credentials.
3. (Optional) Add real Twilio/SMTP credentials if you want SMS/Email sending.

## 5. Verify API Endpoints

Check these URLs in browser/Postman:

1. `GET http://localhost/womens_safety/api/dashboard_data.php?action=overview`
2. `GET http://localhost/womens_safety/api/dashboard_data.php?action=active_alerts`

If JSON response appears, API is running.

## 6. Open Dashboard

1. Go to:
   `http://localhost/womens_safety/dashboard/`
2. Confirm cards/charts load.
3. Click side menu pages to ensure no blank page or JS errors.

## 7. Open Mobile App

1. Go to:
   `http://localhost/womens_safety/mobile_app/`
2. Allow location permission.
3. Press and hold SOS button for 1.5 seconds to trigger emergency API.

## 8. Configure Firmware

Update in `esp32_firmware/main_safety_device.ino`:

1. `WIFI_SSID`, `WIFI_PASSWORD`
2. `SERVER_URL` (your local server IP, not localhost if ESP32 uses Wi-Fi)
3. `DEVICE_ID` matching the `devices` table

Then upload to ESP32.

## 9. Runtime Error Checklist

1. API returns `500`:
   - DB not imported or credentials wrong.
2. API returns `401`:
   - Wrong `DEVICE_ID` or ESP32 server URL.
3. Dashboard no live data:
   - ESP32 not posting to `update_device.php`.
   - Check `device_id` match.
4. GPS location empty:
   - GPS no fix yet; test outdoors.

## 10. Final Validation Flow

1. Register user (`auth.php?action=register`)
2. Login user (`auth.php?action=login`)
3. Send sensor payload (`update_device.php`)
4. Trigger emergency (`emergency_alert.php`)
5. Verify alert appears on dashboard alerts page
6. Resolve/acknowledge alert from dashboard
