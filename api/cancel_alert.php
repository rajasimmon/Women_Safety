<?php
// ============================================================
// API: CANCEL ACTIVE ALERT (Called by ESP32)
// POST /api/cancel_alert.php
// ============================================================
require_once 'config.php';

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    apiError(405, 'Method not allowed');
}

$input = getInput();
$device_id = sanitize($input['device_id'] ?? '');
if (!$device_id) {
    apiError(400, 'device_id is required');
}

$db = getDB();

$stmt = $db->prepare("SELECT user_id FROM devices WHERE device_id = ?");
$stmt->execute([$device_id]);
$device = $stmt->fetch();
if (!$device) {
    apiError(404, 'Device not found');
}

// Resolve the latest unresolved alert for this device.
$resolve = $db->prepare("
    UPDATE emergency_alerts
    SET resolved = 1,
        acknowledged = 1,
        resolved_at = NOW(),
        notes = CONCAT(COALESCE(notes, ''), IF(COALESCE(notes, '') = '', '', '\n'), 'Cancelled by device')
    WHERE device_id = ? AND resolved = 0
    ORDER BY created_at DESC
    LIMIT 1
");
$resolve->execute([$device_id]);

logActivity($device['user_id'], 'cancel_alert', "Alert cancelled by device: {$device_id}");

apiSuccess([
    'device_id' => $device_id,
    'resolved_rows' => $resolve->rowCount()
], 'Alert cancellation processed');
