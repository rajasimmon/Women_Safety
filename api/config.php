<?php
// ============================================================
// SYSTEM CONFIGURATION
// Women's Safety Device - API Backend
// ============================================================

define('DB_HOST',     'localhost');
define('DB_USER',     'root');
define('DB_PASS',     '');
define('DB_NAME',     'womens_safety_db');
define('DB_CHARSET',  'utf8mb4');

define('API_VERSION', '2.0');
define('APP_NAME',    'Women Safety System');
define('BASE_URL',    'http://localhost/womens_safety/');
define('TIMEZONE',    'Asia/Kolkata');

// Security
define('JWT_SECRET',  'wsd_jwt_secret_key_2024_secure');
define('API_RATE_LIMIT', 100); // requests per minute

// SMS Config (Twilio)
define('TWILIO_SID',    'YOUR_TWILIO_SID');
define('TWILIO_TOKEN',  'YOUR_TWILIO_AUTH_TOKEN');
define('TWILIO_FROM',   '+1XXXXXXXXXX');

// Email Config
define('SMTP_HOST',     'smtp.gmail.com');
define('SMTP_PORT',     587);
define('SMTP_USER',     'your@gmail.com');
define('SMTP_PASS',     'your_app_password');
define('SMTP_FROM',     'alerts@safetywear.com');

date_default_timezone_set(TIMEZONE);

// CORS Headers
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, X-Requested-With');
header('Content-Type: application/json; charset=UTF-8');

if (($_SERVER['REQUEST_METHOD'] ?? '') === 'OPTIONS') {
    http_response_code(200);
    exit();
}

// ============================================================
// DATABASE CONNECTION (PDO)
// ============================================================
function getDB() {
    static $pdo = null;
    if ($pdo === null) {
        try {
            $dsn = "mysql:host=" . DB_HOST . ";dbname=" . DB_NAME . ";charset=" . DB_CHARSET;
            $options = [
                PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
                PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
                PDO::ATTR_EMULATE_PREPARES   => false,
            ];
            $pdo = new PDO($dsn, DB_USER, DB_PASS, $options);
        } catch (PDOException $e) {
            apiError(500, 'Database connection failed: ' . $e->getMessage());
        }
    }
    return $pdo;
}

// ============================================================
// RESPONSE HELPERS
// ============================================================
function apiSuccess($data = [], $message = 'Success', $code = 200) {
    http_response_code($code);
    echo json_encode([
        'status'    => 'success',
        'message'   => $message,
        'data'      => $data,
        'timestamp' => date('Y-m-d H:i:s')
    ]);
    exit();
}

function apiError($code = 400, $message = 'Error', $errors = []) {
    http_response_code($code);
    echo json_encode([
        'status'  => 'error',
        'message' => $message,
        'errors'  => $errors,
        'timestamp' => date('Y-m-d H:i:s')
    ]);
    exit();
}

// ============================================================
// AUTH HELPERS
// ============================================================
function getInput() {
    $input = json_decode(file_get_contents('php://input'), true);
    return $input ?: [];
}

function sanitize($value) {
    return htmlspecialchars(strip_tags(trim($value)));
}

// ============================================================
// LOGGING
// ============================================================
function logActivity($user_id, $action, $description = '') {
    try {
        $db = getDB();
        $stmt = $db->prepare("INSERT INTO activity_logs (user_id, action, description, ip_address) VALUES (?,?,?,?)");
        $stmt->execute([$user_id, $action, $description, $_SERVER['REMOTE_ADDR'] ?? '']);
    } catch (Exception $e) {
        // Silent fail for logging
    }
}
