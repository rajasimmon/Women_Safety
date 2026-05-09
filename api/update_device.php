<?php
// ============================================================
// API: UPDATE DEVICE SENSOR DATA (Called by ESP32)
// POST /api/update_device.php
// ============================================================
require_once 'config.php';

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    apiError(405, 'Method not allowed');
}

$input = getInput();

// Validate required fields
if (empty($input['device_id'])) {
    apiError(400, 'device_id is required');
}

$device_id = sanitize($input['device_id']);

$db = getDB();

// Get user_id from device
$stmt = $db->prepare("SELECT user_id FROM devices WHERE device_id = ?");
$stmt->execute([$device_id]);
$device = $stmt->fetch();
if (!$device) apiError(404, 'Device not found');

$user_id = $device['user_id'];
$batteryLevel = intval($input['battery'] ?? $input['battery_level'] ?? 0);

// Insert sensor data
$stmt = $db->prepare("
    INSERT INTO sensor_data 
    (device_id, user_id, heart_rate, spo2, accel_x, accel_y, accel_z,
     gyro_x, gyro_y, gyro_z, temperature, vibration_count, sound_level, battery_level,
     latitude, longitude, altitude, speed, gps_fixed, wifi_connected,
     gsm_connected, emergency_active, alert_type)
    VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
");
$stmt->execute([
    $device_id,
    $user_id,
    floatval($input['heart_rate']    ?? 0),
    floatval($input['spo2']          ?? 0),
    floatval($input['accel_x']       ?? 0),
    floatval($input['accel_y']       ?? 0),
    floatval($input['accel_z']       ?? 0),
    floatval($input['gyro_x']        ?? 0),
    floatval($input['gyro_y']        ?? 0),
    floatval($input['gyro_z']        ?? 0),
    floatval($input['temperature']   ?? 0),
    intval($input['vibration_count'] ?? 0),
    floatval($input['sound_level']   ?? $input['voice_command'] ?? 0),
    $batteryLevel,
    floatval($input['latitude']      ?? 0),
    floatval($input['longitude']     ?? 0),
    floatval($input['altitude']      ?? 0),
    floatval($input['speed']         ?? 0),
    intval($input['gps_fixed']       ?? 0),
    intval($input['wifi_connected']  ?? 1),
    intval($input['gsm_connected']   ?? 0),
    intval($input['emergency_active']?? 0),
    intval($input['alert_type']      ?? 0)
]);

// Insert location history if GPS fixed
if (!empty($input['gps_fixed']) && floatval($input['latitude']) != 0) {
    $stmt2 = $db->prepare("
        INSERT INTO location_history 
        (device_id, user_id, latitude, longitude, altitude, speed, gps_fixed)
        VALUES (?,?,?,?,?,?,?)
    ");
    $stmt2->execute([
        $device_id, $user_id,
        floatval($input['latitude']),
        floatval($input['longitude']),
        floatval($input['altitude'] ?? 0),
        floatval($input['speed']    ?? 0),
        1
    ]);
}

// Insert heart rate history
if (floatval($input['heart_rate'] ?? 0) > 0) {
    $hr = floatval($input['heart_rate']);
    $hrStatus = 'normal';
    if ($hr > 130) $hrStatus = 'high';
    elseif ($hr > 110) $hrStatus = 'medium';
    elseif ($hr < 45) $hrStatus = 'critical';
    elseif ($hr < 55) $hrStatus = 'low';

    $stmt3 = $db->prepare("
        INSERT INTO heart_rate_history (device_id, user_id, heart_rate, spo2, status)
        VALUES (?,?,?,?,?)
    ");
    $stmt3->execute([$device_id, $user_id, $hr, floatval($input['spo2'] ?? 0), $hrStatus]);
}

// Update device status
$stmt4 = $db->prepare("
    UPDATE devices SET 
        battery_level = ?, is_online = 1, last_seen = NOW(),
        ip_address = ?
    WHERE device_id = ?
");
$stmt4->execute([
    $batteryLevel,
    $_SERVER['REMOTE_ADDR'] ?? '',
    $device_id
]);

// Check for pending commands
$stmt5 = $db->prepare("
    SELECT id, command, params FROM device_commands 
    WHERE device_id = ? AND status = 'pending' 
    ORDER BY created_at ASC LIMIT 1
");
$stmt5->execute([$device_id]);
$pendingCommand = $stmt5->fetch();

$response = ['status' => 'ok', 'timestamp' => time()];
if ($pendingCommand) {
    $response['command'] = $pendingCommand['command'];
    $response['params']  = json_decode($pendingCommand['params'], true);
    // Mark command as sent
    $db->prepare("UPDATE device_commands SET status='sent' WHERE id=?")->execute([$pendingCommand['id']]);
}

apiSuccess($response, 'Data updated successfully');
