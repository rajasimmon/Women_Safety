<?php
// ============================================================
// API: EMERGENCY ALERT HANDLER (Called by ESP32)
// POST /api/emergency_alert.php
// ============================================================
require_once 'config.php';

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    apiError(405, 'Method not allowed');
}

$input  = getInput();

if (empty($input['device_id'])) apiError(400, 'device_id required');

$device_id = sanitize($input['device_id']);

$db = getDB();

// Get device + user info
$stmt = $db->prepare("
    SELECT d.*, u.full_name, u.phone, u.email, u.blood_group
    FROM devices d JOIN users u ON d.user_id = u.id
    WHERE d.device_id = ?
");
$stmt->execute([$device_id]);
$deviceInfo = $stmt->fetch();
if (!$deviceInfo) apiError(404, 'Device not found');

$user_id   = $deviceInfo['user_id'];
$alertType = intval($input['alert_type'] ?? 0);
$alertName = sanitize($input['alert_name'] ?? 'Emergency Alert');
$reason    = sanitize($input['reason']     ?? '');
$latitude  = floatval($input['latitude']   ?? 0);
$longitude = floatval($input['longitude']  ?? 0);
$heartRate = floatval($input['heart_rate'] ?? 0);
$spo2      = floatval($input['spo2']       ?? 0);
$battery   = intval($input['battery']      ?? 0);

// Determine severity
$severity = 'high';
if ($alertType == 1) $severity = 'critical';            // SOS
if ($alertType == 3 || $alertType == 4) $severity = 'medium'; // Heart rate
if ($alertType == 6) $severity = 'medium';              // Geofence

// Reverse geocode (if GPS available)
$address = '';
if ($latitude != 0 && $longitude != 0) {
    $geoUrl = "https://nominatim.openstreetmap.org/reverse?format=json&lat={$latitude}&lon={$longitude}";
    $geoCtx = stream_context_create(['http' => [
        'header' => 'User-Agent: SafetyDevice/2.0',
        'timeout' => 2,
    ]]);
    $geoRes = @file_get_contents($geoUrl, false, $geoCtx);
    if ($geoRes) {
        $geoData = json_decode($geoRes, true);
        $address = $geoData['display_name'] ?? '';
    }
}

// Insert emergency alert
$stmt = $db->prepare("
    INSERT INTO emergency_alerts
    (device_id, user_id, alert_type, alert_name, reason, latitude, longitude,
     location_address, heart_rate, spo2, battery_level, severity)
    VALUES (?,?,?,?,?,?,?,?,?,?,?,?)
");
$stmt->execute([
    $device_id, $user_id, $alertType, $alertName, $reason,
    $latitude, $longitude, $address, $heartRate, $spo2, $battery, $severity
]);
$alertId = $db->lastInsertId();

// Get emergency contacts
$stmt = $db->prepare("
    SELECT * FROM emergency_contacts 
    WHERE user_id = ? AND is_active = 1 
    ORDER BY priority ASC
");
$stmt->execute([$user_id]);
$contacts = $stmt->fetchAll();

$mapsLink = ($latitude != 0)
    ? "https://maps.google.com/?q={$latitude},{$longitude}"
    : "GPS unavailable";

$smsMessage = "🚨 EMERGENCY ALERT!\n"
    . "Person: {$deviceInfo['full_name']}\n"
    . "Alert: {$alertName}\n"
    . "Reason: {$reason}\n"
    . "Heart Rate: {$heartRate} BPM\n"
    . "Blood Group: {$deviceInfo['blood_group']}\n"
    . "Location: {$mapsLink}\n"
    . "Address: {$address}\n"
    . "Battery: {$battery}%\n"
    . "Device: {$device_id}";

$notificationsSent = 0;
$emailNotificationsSent = 0;
foreach ($contacts as $contact) {
    if ($contact['notify_sms']) {
        $smsSent = sendSMSViaTwilio($contact['phone'], $smsMessage);
        // Log notification
        $stmt2 = $db->prepare("
            INSERT INTO alert_notifications
            (alert_id, contact_id, notification_type, recipient, message, status, sent_at)
            VALUES (?,?,?,?,?,?,NOW())
        ");
        $stmt2->execute([
            $alertId, $contact['id'], 'sms',
            $contact['phone'], $smsMessage,
            $smsSent ? 'sent' : 'failed'
        ]);
        if ($smsSent) $notificationsSent++;
    }

    if ($contact['notify_email'] && !empty($contact['email'])) {
        $emailSent = sendAlertEmail(
            $contact['email'],
            $contact['contact_name'],
            $deviceInfo['full_name'],
            $alertName, $reason, $mapsLink, $address,
            $heartRate, $spo2, $battery, $alertId
        );
        $stmt3 = $db->prepare("
            INSERT INTO alert_notifications
            (alert_id, contact_id, notification_type, recipient, message, status, sent_at)
            VALUES (?,?,?,?,?,?,NOW())
        ");
        $stmt3->execute([
            $alertId, $contact['id'], 'email',
            $contact['email'], "Email alert sent for: $alertName",
            $emailSent ? 'sent' : 'failed'
        ]);
        if ($emailSent) $emailNotificationsSent++;
    }
}

// Update alert SMS/email flags
$db->prepare("UPDATE emergency_alerts SET sms_sent=?, email_sent=? WHERE id=?")
   ->execute([$notificationsSent > 0 ? 1 : 0, $emailNotificationsSent > 0 ? 1 : 0, $alertId]);

// Log activity
logActivity($user_id, 'emergency_alert',
    "Alert ID: {$alertId} | Type: {$alertName} | Device: {$device_id}");

apiSuccess([
    'alert_id'            => $alertId,
    'notifications_sent'  => $notificationsSent,
    'contacts_notified'   => count($contacts),
    'severity'            => $severity,
    'address'             => $address
], 'Emergency alert processed');

// ============================================================
// SMS VIA TWILIO
// ============================================================
function sendSMSViaTwilio($to, $message) {
    if (TWILIO_SID === 'YOUR_TWILIO_SID') return false; // Not configured

    $url  = "https://api.twilio.com/2010-04-01/Accounts/" . TWILIO_SID . "/Messages.json";
    $data = http_build_query(['To' => $to, 'From' => TWILIO_FROM, 'Body' => $message]);

    $ch = curl_init($url);
    curl_setopt_array($ch, [
        CURLOPT_POST           => true,
        CURLOPT_POSTFIELDS     => $data,
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_USERPWD        => TWILIO_SID . ':' . TWILIO_TOKEN,
        CURLOPT_HTTPHEADER     => ['Content-Type: application/x-www-form-urlencoded'],
        CURLOPT_TIMEOUT        => 10,
    ]);
    $response = curl_exec($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    return $httpCode === 201;
}

// ============================================================
// EMAIL ALERT
// ============================================================
function sendAlertEmail($to, $toName, $userName, $alertName,
    $reason, $mapsLink, $address, $hr, $spo2, $battery, $alertId) {
    $subject = "🚨 EMERGENCY ALERT - {$userName} needs help!";
    $body = "
    <html><body style='font-family:Arial;background:#f5f5f5;padding:20px'>
    <div style='max-width:600px;margin:auto;background:white;border-radius:10px;overflow:hidden'>
      <div style='background:#e74c3c;padding:20px;text-align:center'>
        <h1 style='color:white;margin:0'>🚨 EMERGENCY ALERT</h1>
        <p style='color:#ffcdd2;margin:5px 0'>Alert ID: #{$alertId}</p>
      </div>
      <div style='padding:30px'>
        <p>Dear {$toName},</p>
        <p><strong>{$userName}</strong> has triggered an emergency alert.</p>
        <table style='width:100%;border-collapse:collapse'>
          <tr style='background:#ffeaea'>
            <td style='padding:10px;font-weight:bold'>Alert Type</td>
            <td style='padding:10px'>{$alertName}</td>
          </tr>
          <tr>
            <td style='padding:10px;font-weight:bold'>Reason</td>
            <td style='padding:10px'>{$reason}</td>
          </tr>
          <tr style='background:#ffeaea'>
            <td style='padding:10px;font-weight:bold'>Heart Rate</td>
            <td style='padding:10px'>{$hr} BPM</td>
          </tr>
          <tr>
            <td style='padding:10px;font-weight:bold'>SpO2</td>
            <td style='padding:10px'>{$spo2}%</td>
          </tr>
          <tr style='background:#ffeaea'>
            <td style='padding:10px;font-weight:bold'>Battery</td>
            <td style='padding:10px'>{$battery}%</td>
          </tr>
          <tr>
            <td style='padding:10px;font-weight:bold'>Address</td>
            <td style='padding:10px'>{$address}</td>
          </tr>
        </table>
        <div style='text-align:center;margin:20px 0'>
          <a href='{$mapsLink}' style='background:#e74c3c;color:white;padding:12px 30px;
             border-radius:5px;text-decoration:none;font-weight:bold'>
            📍 View Live Location
          </a>
        </div>
        <p style='color:#999;font-size:12px'>
          This is an automated alert from Women Safety Device System.<br>
          Please respond immediately if you cannot reach the person.
        </p>
      </div>
    </div>
    </body></html>";

    $headers  = "MIME-Version: 1.0\r\n";
    $headers .= "Content-type: text/html; charset=UTF-8\r\n";
    $headers .= "From: " . SMTP_FROM . "\r\n";
    $headers .= "Reply-To: " . SMTP_FROM . "\r\n";

    return mail($to, $subject, $body, $headers);
}
