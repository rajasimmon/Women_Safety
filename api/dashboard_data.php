<?php
// ============================================================
// API: DASHBOARD DATA ENDPOINTS
// GET /api/dashboard_data.php?action=...
// ============================================================
require_once 'config.php';

$action = sanitize($_GET['action'] ?? 'overview');
$db     = getDB();

switch ($action) {

    // ── Overview Stats ──────────────────────────────────────
    case 'overview':
        $stats = [];

        $stats['total_users']   = $db->query("SELECT COUNT(*) FROM users WHERE role='user'")->fetchColumn();
        $stats['total_devices'] = $db->query("SELECT COUNT(*) FROM devices")->fetchColumn();
        $stats['online_devices']= $db->query("SELECT COUNT(*) FROM devices WHERE is_online=1 AND last_seen >= DATE_SUB(NOW(), INTERVAL 1 MINUTE)")->fetchColumn();
        $stats['total_alerts']  = $db->query("SELECT COUNT(*) FROM emergency_alerts")->fetchColumn();
        $stats['active_alerts'] = $db->query("SELECT COUNT(*) FROM emergency_alerts WHERE resolved=0")->fetchColumn();
        $stats['alerts_today']  = $db->query("SELECT COUNT(*) FROM emergency_alerts WHERE DATE(created_at)=CURDATE()")->fetchColumn();
        $stats['avg_heart_rate']= round($db->query("SELECT AVG(heart_rate) FROM sensor_data WHERE recorded_at >= DATE_SUB(NOW(), INTERVAL 1 HOUR) AND heart_rate > 0")->fetchColumn(), 1);
        $stats['critical_alerts']= $db->query("SELECT COUNT(*) FROM emergency_alerts WHERE severity='critical' AND resolved=0")->fetchColumn();

        apiSuccess($stats, 'Overview stats loaded');
        break;

    // ── Active Alerts ───────────────────────────────────────
    case 'active_alerts':
        $rows = $db->query("SELECT * FROM vw_active_alerts LIMIT 50")->fetchAll();
        apiSuccess($rows, 'Active alerts loaded');
        break;

    // ── Device Status ───────────────────────────────────────
    case 'device_status':
        $rows = $db->query("SELECT * FROM vw_device_status")->fetchAll();
        apiSuccess($rows, 'Device status loaded');
        break;

    // ── Live Sensor Data ────────────────────────────────────
    case 'live_sensor':
        $device_id = sanitize($_GET['device_id'] ?? '');
        if (!$device_id) apiError(400, 'device_id required');

        $stmt = $db->prepare("SELECT * FROM sensor_data WHERE device_id=? ORDER BY recorded_at DESC LIMIT 1");
        $stmt->execute([$device_id]);
        $data = $stmt->fetch();
        apiSuccess($data ?: [], 'Live sensor data');
        break;

    // ── Heart Rate Chart Data ───────────────────────────────
    case 'heart_rate_chart':
        $device_id = sanitize($_GET['device_id'] ?? '');
        $hours     = intval($_GET['hours'] ?? 24);
        if (!$device_id) apiError(400, 'device_id required');

        $stmt = $db->prepare("
            SELECT
              DATE_FORMAT(recorded_at,'%H:%i') as time_label,
              ROUND(AVG(NULLIF(heart_rate,0)),1) as avg_hr,
              MAX(heart_rate) as max_hr,
              MIN(NULLIF(heart_rate,0)) as min_hr,
              ROUND(AVG(NULLIF(spo2,0)),1) as avg_spo2,
              ROUND(AVG(NULLIF(temperature,0)),1) as avg_temp,
              ROUND(AVG(NULLIF(battery_level,0)),1) as avg_battery,
              MAX(vibration_count) as max_vibration
            FROM sensor_data
            WHERE device_id=?
              AND recorded_at >= DATE_SUB(NOW(), INTERVAL ? HOUR)
            GROUP BY DATE_FORMAT(recorded_at,'%Y-%m-%d %H:%i')
            ORDER BY MIN(recorded_at) ASC
            LIMIT 120
        ");
        $stmt->execute([$device_id, $hours]);
        $rows = $stmt->fetchAll();
        apiSuccess($rows, 'Heart rate chart data');
        break;

    // ── Location Track ──────────────────────────────────────
    case 'location_track':
        $device_id = sanitize($_GET['device_id'] ?? '');
        $hours     = intval($_GET['hours'] ?? 6);
        if (!$device_id) apiError(400, 'device_id required');

        $stmt = $db->prepare("CALL sp_get_location_track(?, ?)");
        $stmt->execute([$device_id, $hours]);
        $rows = $stmt->fetchAll();
        apiSuccess($rows, 'Location track loaded');
        break;

    // ── Alerts History ──────────────────────────────────────
    case 'alerts_history':
        $user_id = intval($_GET['user_id'] ?? 0);
        $limit   = intval($_GET['limit'] ?? 20);
        $offset  = intval($_GET['offset'] ?? 0);

        $where = $user_id ? "WHERE ea.user_id=$user_id" : "";
        $rows = $db->query("
            SELECT ea.*, u.full_name, u.phone, d.device_name
            FROM emergency_alerts ea
            JOIN users u ON ea.user_id = u.id
            JOIN devices d ON ea.device_id = d.device_id
            $where
            ORDER BY ea.created_at DESC
            LIMIT $limit OFFSET $offset
        ")->fetchAll();

        $total = $db->query("SELECT COUNT(*) FROM emergency_alerts $where")->fetchColumn();
        apiSuccess(['rows' => $rows, 'total' => $total], 'Alerts history loaded');
        break;

    // ── Alert Types Pie Chart ───────────────────────────────
    case 'alert_types_chart':
        $rows = $db->query("
            SELECT alert_name, COUNT(*) as count
            FROM emergency_alerts
            WHERE created_at >= DATE_SUB(NOW(), INTERVAL 30 DAY)
            GROUP BY alert_name
            ORDER BY count DESC
        ")->fetchAll();
        apiSuccess($rows, 'Alert types chart data');
        break;

    // ── Weekly Alerts Chart ─────────────────────────────────
    case 'weekly_alerts':
        $rows = $db->query("
            SELECT
              DATE_FORMAT(created_at,'%W') as day_name,
              DATE(created_at) as date,
              COUNT(*) as total,
              SUM(CASE WHEN severity='critical' THEN 1 ELSE 0 END) as critical,
              SUM(CASE WHEN severity='high' THEN 1 ELSE 0 END) as high,
              SUM(CASE WHEN severity='medium' THEN 1 ELSE 0 END) as medium
            FROM emergency_alerts
            WHERE created_at >= DATE_SUB(NOW(), INTERVAL 7 DAY)
            GROUP BY DATE(created_at)
            ORDER BY date ASC
        ")->fetchAll();
        apiSuccess($rows, 'Weekly alerts data');
        break;

    // ── Recent Activity Feed ────────────────────────────────
    case 'activity_feed':
        $rows = $db->query("
            SELECT al.*, u.full_name
            FROM activity_logs al
            LEFT JOIN users u ON al.user_id = u.id
            ORDER BY al.created_at DESC
            LIMIT 20
        ")->fetchAll();
        apiSuccess($rows, 'Activity feed loaded');
        break;

    // ── Acknowledge Alert ───────────────────────────────────
    case 'acknowledge_alert':
        if ($_SERVER['REQUEST_METHOD'] !== 'POST') apiError(405, 'POST required');
        $input    = getInput();
        $alertId  = intval($input['alert_id'] ?? 0);
        $adminId  = intval($input['admin_id'] ?? 1);
        if (!$alertId) apiError(400, 'alert_id required');

        $db->prepare("
            UPDATE emergency_alerts
            SET acknowledged=1, acknowledged_by=?, acknowledged_at=NOW()
            WHERE id=?
        ")->execute([$adminId, $alertId]);

        apiSuccess(['alert_id' => $alertId], 'Alert acknowledged');
        break;

    // ── Resolve Alert ───────────────────────────────────────
    case 'resolve_alert':
        if ($_SERVER['REQUEST_METHOD'] !== 'POST') apiError(405, 'POST required');
        $input   = getInput();
        $alertId = intval($input['alert_id'] ?? 0);
        $notes   = sanitize($input['notes'] ?? '');
        if (!$alertId) apiError(400, 'alert_id required');

        $db->prepare("
            UPDATE emergency_alerts
            SET resolved=1, resolved_at=NOW(), notes=?
            WHERE id=?
        ")->execute([$notes, $alertId]);

        apiSuccess(['alert_id' => $alertId], 'Alert resolved');
        break;

    // ── Send Device Command ─────────────────────────────────
    case 'send_command':
        if ($_SERVER['REQUEST_METHOD'] !== 'POST') apiError(405, 'POST required');
        $input     = getInput();
        $device_id = sanitize($input['device_id'] ?? '');
        $command   = sanitize($input['command']   ?? '');
        if (!$device_id || !$command) apiError(400, 'device_id and command required');

        $allowed = ['cancel_alert','trigger_buzzer','get_location','restart','silent_mode'];
        if (!in_array($command, $allowed)) apiError(400, 'Invalid command');

        $db->prepare("
            INSERT INTO device_commands (device_id, command, params, issued_by)
            VALUES (?,?,?,?)
        ")->execute([$device_id, $command, json_encode($input['params'] ?? []), $input['admin_id'] ?? 1]);

        apiSuccess(['command' => $command], 'Command queued for device');
        break;

    // ── Battery Stats ───────────────────────────────────────
    case 'battery_stats':
        $rows = $db->query("
            SELECT d.device_id, d.device_name, d.battery_level,
                   u.full_name,
                   CASE
                     WHEN d.battery_level >= 50 THEN 'good'
                     WHEN d.battery_level >= 20 THEN 'low'
                     ELSE 'critical'
                   END as battery_status
            FROM devices d JOIN users u ON d.user_id = u.id
            ORDER BY d.battery_level ASC
        ")->fetchAll();
        apiSuccess($rows, 'Battery stats loaded');
        break;

    default:
        apiError(400, 'Unknown action: ' . $action);
}
