<?php
// ============================================================
// API: AUTHENTICATION (Login / Register / Logout)
// POST /api/auth.php?action=login|register|logout
// ============================================================
require_once 'config.php';

$action = sanitize($_GET['action'] ?? $_POST['action'] ?? 'login');
$db     = getDB();

switch ($action) {

    // ── LOGIN ────────────────────────────────────────────────
    case 'login':
        if ($_SERVER['REQUEST_METHOD'] !== 'POST') apiError(405, 'POST required');
        $input    = getInput();
        $email    = sanitize($input['email']    ?? '');
        $password = $input['password'] ?? '';

        if (!$email || !$password) apiError(400, 'Email and password required');

        $stmt = $db->prepare("SELECT * FROM users WHERE email=? AND is_active=1");
        $stmt->execute([$email]);
        $user = $stmt->fetch();

        if (!$user || !password_verify($password, $user['password_hash'])) {
            apiError(401, 'Invalid email or password');
        }

        // Update last login
        $db->prepare("UPDATE users SET last_login=NOW() WHERE id=?")->execute([$user['id']]);

        // Generate session token (simple JWT-like)
        $token = base64_encode(json_encode([
            'user_id' => $user['id'],
            'email'   => $user['email'],
            'role'    => $user['role'],
            'exp'     => time() + 86400
        ])) . '.' . hash_hmac('sha256',
            $user['id'] . $user['email'], JWT_SECRET);

        logActivity($user['id'], 'login', 'User logged in from ' . ($_SERVER['REMOTE_ADDR'] ?? ''));

        // Get user's device
        $stmt2 = $db->prepare("SELECT * FROM devices WHERE user_id=? LIMIT 1");
        $stmt2->execute([$user['id']]);
        $device = $stmt2->fetch();

        apiSuccess([
            'token'   => $token,
            'user'    => [
                'id'          => $user['id'],
                'full_name'   => $user['full_name'],
                'email'       => $user['email'],
                'phone'       => $user['phone'],
                'role'        => $user['role'],
                'blood_group' => $user['blood_group'],
                'profile_photo'=> $user['profile_photo'],
            ],
            'device'  => $device ?: null
        ], 'Login successful');
        break;

    // ── REGISTER ─────────────────────────────────────────────
    case 'register':
        if ($_SERVER['REQUEST_METHOD'] !== 'POST') apiError(405, 'POST required');
        $input     = getInput();
        $full_name = sanitize($input['full_name'] ?? '');
        $email     = sanitize($input['email']     ?? '');
        $phone     = sanitize($input['phone']     ?? '');
        $password  = $input['password'] ?? '';
        $blood_group = sanitize($input['blood_group'] ?? 'O+');

        if (!$full_name || !$email || !$phone || !$password) {
            apiError(400, 'All fields are required');
        }
        if (!filter_var($email, FILTER_VALIDATE_EMAIL)) {
            apiError(400, 'Invalid email format');
        }
        if (strlen($password) < 6) {
            apiError(400, 'Password must be at least 6 characters');
        }

        // Check duplicate email
        $stmt = $db->prepare("SELECT id FROM users WHERE email=?");
        $stmt->execute([$email]);
        if ($stmt->fetch()) apiError(409, 'Email already registered');

        $hash = password_hash($password, PASSWORD_BCRYPT);
        $stmt = $db->prepare("
            INSERT INTO users (full_name, email, phone, password_hash, blood_group, role)
            VALUES (?,?,?,?,?,'user')
        ");
        $stmt->execute([$full_name, $email, $phone, $hash, $blood_group]);
        $userId = $db->lastInsertId();

        // Auto-create device
        $deviceId    = 'WSD_' . str_pad($userId, 3, '0', STR_PAD_LEFT);
        $db->prepare("
            INSERT INTO devices (device_id, device_name, user_id)
            VALUES (?,?,?)
        ")->execute([$deviceId, $full_name . "'s Safety Band", $userId]);

        logActivity($userId, 'register', 'New user registered');

        apiSuccess([
            'user_id'      => $userId,
            'device_id'    => $deviceId
        ], 'Registration successful', 201);
        break;

    // ── CHANGE PASSWORD ───────────────────────────────────────
    case 'change_password':
        if ($_SERVER['REQUEST_METHOD'] !== 'POST') apiError(405, 'POST required');
        $input       = getInput();
        $userId      = intval($input['user_id']      ?? 0);
        $oldPassword = $input['old_password'] ?? '';
        $newPassword = $input['new_password'] ?? '';

        if (!$userId || !$oldPassword || !$newPassword) {
            apiError(400, 'All fields required');
        }

        $stmt = $db->prepare("SELECT password_hash FROM users WHERE id=?");
        $stmt->execute([$userId]);
        $user = $stmt->fetch();

        if (!$user || !password_verify($oldPassword, $user['password_hash'])) {
            apiError(401, 'Current password is incorrect');
        }

        $newHash = password_hash($newPassword, PASSWORD_BCRYPT);
        $db->prepare("UPDATE users SET password_hash=? WHERE id=?")->execute([$newHash, $userId]);

        logActivity($userId, 'change_password', 'Password changed');
        apiSuccess([], 'Password changed successfully');
        break;

    // ── GET PROFILE ───────────────────────────────────────────
    case 'profile':
        $userId = intval($_GET['user_id'] ?? 0);
        if (!$userId) apiError(400, 'user_id required');

        $stmt = $db->prepare("
            SELECT id, full_name, email, phone, blood_group,
                   medical_notes, address, profile_photo, role, created_at
            FROM users WHERE id=?
        ");
        $stmt->execute([$userId]);
        $user = $stmt->fetch();
        if (!$user) apiError(404, 'User not found');

        // Get emergency contacts
        $stmt2 = $db->prepare("SELECT * FROM emergency_contacts WHERE user_id=? ORDER BY priority");
        $stmt2->execute([$userId]);
        $user['emergency_contacts'] = $stmt2->fetchAll();

        // Get device info
        $stmt3 = $db->prepare("SELECT * FROM devices WHERE user_id=? LIMIT 1");
        $stmt3->execute([$userId]);
        $user['device'] = $stmt3->fetch();

        apiSuccess($user, 'Profile loaded');
        break;

    // ── UPDATE PROFILE ────────────────────────────────────────
    case 'update_profile':
        if ($_SERVER['REQUEST_METHOD'] !== 'POST') apiError(405, 'POST required');
        $input      = getInput();
        $userId     = intval($input['user_id'] ?? 0);
        if (!$userId) apiError(400, 'user_id required');

        $db->prepare("
            UPDATE users SET
              full_name=?, phone=?, blood_group=?,
              medical_notes=?, address=?
            WHERE id=?
        ")->execute([
            sanitize($input['full_name']    ?? ''),
            sanitize($input['phone']        ?? ''),
            sanitize($input['blood_group']  ?? 'O+'),
            sanitize($input['medical_notes']?? ''),
            sanitize($input['address']      ?? ''),
            $userId
        ]);

        logActivity($userId, 'update_profile', 'Profile updated');
        apiSuccess([], 'Profile updated successfully');
        break;

    // ── LIST ALL USERS (Admin) ────────────────────────────────
    case 'list_users':
        $rows = $db->query("
            SELECT u.id, u.full_name, u.email, u.phone, u.blood_group,
                   u.role, u.is_active, u.last_login, u.created_at,
                   d.device_id, d.battery_level, d.is_online, d.last_seen
            FROM users u
            LEFT JOIN devices d ON u.id = d.user_id
            ORDER BY u.created_at DESC
        ")->fetchAll();
        apiSuccess($rows, 'Users list loaded');
        break;

    default:
        apiError(400, 'Unknown action');
}
