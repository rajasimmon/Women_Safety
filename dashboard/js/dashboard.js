"use strict";

const API_BASE = "../api/";
const FALLBACK_DEVICE_ID = "WIFE_ESP32_001";
const REFRESH_MS = 5000;

let activeDeviceId = FALLBACK_DEVICE_ID;
let latestSensor = null;
let charts = {};
let map = null;
let marker = null;

const demoSensor = {
  device_id: FALLBACK_DEVICE_ID,
  heart_rate: 76,
  spo2: 98,
  battery_level: 84,
  temperature: 36.7,
  vibration_count: 0,
  voice_command: 0,
  latitude: 12.971598,
  longitude: 77.594562,
  altitude: 905,
  speed: 0,
  gps_fixed: true,
  wifi_connected: true,
  gsm_connected: true,
  emergency_active: false,
  alert_type: 0
};

document.addEventListener("DOMContentLoaded", () => {
  bindEvents();
  startClock();
  initCharts();
  refreshAll();
  setInterval(refreshLiveSensor, REFRESH_MS);
});

function bindEvents() {
  document.querySelectorAll("[data-page]").forEach(btn => {
    btn.addEventListener("click", () => showPage(btn.dataset.page));
  });

  document.querySelectorAll("[data-jump]").forEach(btn => {
    btn.addEventListener("click", () => showPage(btn.dataset.jump));
  });

  document.getElementById("menu-btn")?.addEventListener("click", () => toggleSidebar(true));
  document.getElementById("backdrop")?.addEventListener("click", () => toggleSidebar(false));
  document.getElementById("refresh-btn")?.addEventListener("click", refreshAll);
  document.getElementById("reload-alerts")?.addEventListener("click", loadAlerts);
  document.getElementById("reload-devices")?.addEventListener("click", loadDevices);
  document.getElementById("manual-sos")?.addEventListener("click", triggerManualSOS);
  document.getElementById("hr-range")?.addEventListener("change", () => {
    loadHeartRateChart();
    loadSensorSignalChart();
  });
}

function showPage(page) {
  document.querySelectorAll(".page").forEach(section => section.classList.remove("active"));
  document.getElementById("page-" + page)?.classList.add("active");

  document.querySelectorAll(".nav-link").forEach(btn => {
    btn.classList.toggle("active", btn.dataset.page === page);
  });

  const titles = {
    overview: ["Overview", "Live wearable safety monitoring"],
    live: ["Live Sensors", "Raw readings from the safety wearable"],
    "sensor-status": ["Sensor Status", "Working check for each wearable sensor"],
    alerts: ["Emergency Alerts", "SOS, heart, vibration, geofence, and voice events"],
    location: ["Location", "NEO-6M GPS position and map"],
    devices: ["Devices", "ESP32 wearable device fleet"],
    settings: ["Settings", "Current sensor configuration"]
  };

  setText("page-title", titles[page]?.[0] || "Dashboard");
  setText("page-subtitle", titles[page]?.[1] || "");

  if (page === "alerts") loadAlerts();
  if (page === "devices") loadDevices();
  if (page === "location") setTimeout(initMap, 100);
  toggleSidebar(false);
}

function toggleSidebar(show) {
  document.getElementById("sidebar")?.classList.toggle("show", show);
  document.getElementById("backdrop")?.classList.toggle("show", show);
}

function startClock() {
  const tick = () => {
    setText("clock", new Date().toLocaleTimeString("en-IN", {
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit"
    }));
  };
  tick();
  setInterval(tick, 1000);
}

async function refreshAll() {
  rotateRefresh(true);
  await initActiveDevice();
  await Promise.all([
    loadOverview(),
    refreshLiveSensor(),
    loadAlerts(),
    loadDevices(),
    loadHeartRateChart(),
    loadSensorSignalChart(),
    loadAlertTypesChart()
  ]);
  rotateRefresh(false);
}

async function initActiveDevice() {
  try {
    const devices = await apiGet("dashboard_data.php?action=device_status");
    if (devices?.length && devices[0].device_id) activeDeviceId = devices[0].device_id;
  } catch {
    activeDeviceId = FALLBACK_DEVICE_ID;
  }
}

async function loadOverview() {
  try {
    const stats = await apiGet("dashboard_data.php?action=overview");
    updateStats(stats);
  } catch {
    updateStats({
      total_users: 1,
      total_devices: 1,
      online_devices: 1,
      active_alerts: 0,
      alerts_today: 0,
      avg_heart_rate: 76,
      critical_alerts: 0
    });
  }
}

function updateStats(stats) {
  setText("stat-active-alerts", number(stats.active_alerts));
  setText("stat-alerts-today", `${number(stats.alerts_today)} today`);
  setText("stat-online-devices", number(stats.online_devices));
  setText("stat-total-devices", `of ${number(stats.total_devices)} total`);
  setText("stat-total-users", number(stats.total_users));
  setText("stat-avg-hr", stats.avg_heart_rate || "--");
  setText("nav-alert-count", number(stats.active_alerts));

  const active = Number(stats.critical_alerts || stats.active_alerts || 0);
  document.getElementById("emergency-strip")?.classList.toggle("d-none", active === 0);
  if (active > 0) setText("emergency-text", `${active} alert(s) require attention.`);
}

async function refreshLiveSensor() {
  let data = null;
  try {
    data = await apiGet(`dashboard_data.php?action=live_sensor&device_id=${encodeURIComponent(activeDeviceId)}`);
  } catch {
    data = makeDemoSensor();
  }

  if (!data || Object.keys(data).length === 0) data = makeDemoSensor();
  latestSensor = normalizeSensor(data);
  updateLiveUI(latestSensor);
}

function normalizeSensor(d) {
  const alertType = Number(d.alert_type || 0);
  const voiceRaw = d.voice_command ?? d.last_voice_command ?? d.sound_level ?? null;
  const voiceCommand = voiceRaw !== null ? Number(voiceRaw) : (alertType === 2 ? 1 : 0);

  return {
    device_id: d.device_id || activeDeviceId,
    heart_rate: Number(d.heart_rate || 0),
    spo2: Number(d.spo2 || 0),
    battery_level: Number(d.battery_level ?? d.battery ?? 0),
    temperature: Number(d.temperature || 0),
    vibration_count: Number(d.vibration_count || 0),
    voice_command: voiceCommand,
    latitude: Number(d.latitude || 0),
    longitude: Number(d.longitude || 0),
    altitude: Number(d.altitude || 0),
    speed: Number(d.speed || 0),
    gps_fixed: Boolean(Number(d.gps_fixed ?? d.gpsFixed ?? 0) || d.gps_fixed === true),
    wifi_connected: Boolean(Number(d.wifi_connected ?? 0) || d.wifi_connected === true),
    gsm_connected: Boolean(Number(d.gsm_connected ?? 0) || d.gsm_connected === true),
    emergency_active: Boolean(Number(d.emergency_active ?? 0) || d.emergency_active === true),
    alert_type: alertType,
    recorded_at: d.recorded_at || d.last_seen || new Date().toISOString()
  };
}

function updateLiveUI(d) {
  setText("active-device-label", `${d.device_id} - ${d.wifi_connected ? "online" : "offline"}`);
  document.getElementById("device-dot")?.classList.toggle("online", d.wifi_connected);

  setText("vital-hr", d.heart_rate ? Math.round(d.heart_rate) : "--");
  setText("vital-spo2", d.spo2 ? d.spo2.toFixed(1) : "--");
  setText("vital-battery", d.battery_level ? Math.round(d.battery_level) : "--");
  setText("vital-temp", d.temperature ? d.temperature.toFixed(1) : "--");
  setText("vital-vibration", d.vibration_count);
  setText("vital-voice", voiceLabel(d.voice_command));
  setText("vital-gps", d.gps_fixed ? "Fixed" : "No fix");
  setText("last-update", `Last update: ${formatTime(d.recorded_at)}`);

  colorVital("hr-card", hrStatus(d.heart_rate));
  colorVital("spo2-card", d.spo2 && d.spo2 < 94 ? "danger" : "normal");
  colorVital("battery-card", d.battery_level < 20 ? "danger" : d.battery_level < 40 ? "warning" : "normal");

  updateLiveTable(d);
  updateSensorStatusPage(d);
  updateLocationPanel(d);
  updateMapMarker(d);

  if (d.emergency_active) {
    document.getElementById("emergency-strip")?.classList.remove("d-none");
    setText("emergency-text", `${d.device_id} has an active emergency.`);
  }
}

function updateSensorStatusPage(d) {
  const grid = document.getElementById("sensor-status-grid");
  if (!grid) return;

  const ageMs = readingAgeMs(d.recorded_at);
  const fresh = ageMs !== null && ageMs <= 15000;
  const recent = ageMs !== null && ageMs <= 60000;

  const checks = [
    sensorCheck({
      icon: "fa-heart-pulse",
      name: "MAX30102",
      value: d.heart_rate ? `${Math.round(d.heart_rate)} BPM` : "--",
      note: "Heart rate and SpO2",
      status: fresh && (d.heart_rate > 0 || d.spo2 > 0) ? "working" : recent ? "waiting" : "check",
      message: d.heart_rate > 0 || d.spo2 > 0 ? "Reading received" : "Place finger on sensor"
    }),
    sensorCheck({
      icon: "fa-temperature-half",
      name: "DHT11",
      value: d.temperature ? `${d.temperature.toFixed(1)} C` : "--",
      note: "Body temperature",
      status: fresh && d.temperature > 0 ? "working" : recent ? "waiting" : "check",
      message: d.temperature > 0 ? "Reading received" : "No temperature value"
    }),
    sensorCheck({
      icon: "fa-wave-square",
      name: "SW-420",
      value: d.vibration_count,
      note: "Vibration sensor",
      status: fresh && (d.vibration_count > 0 || d.alert_type === 5) ? "working" : recent ? "waiting" : "check",
      message: d.vibration_count > 0 || d.alert_type === 5 ? "Vibration detected" : "Tap module to test"
    }),
    sensorCheck({
      icon: "fa-microphone-lines",
      name: "VC-02",
      value: voiceLabel(d.voice_command),
      note: "Voice command",
      status: fresh && d.voice_command ? "working" : recent ? "waiting" : "check",
      message: d.voice_command ? "Command received" : "Say command to test"
    }),
    sensorCheck({
      icon: "fa-location-dot",
      name: "NEO-6M",
      value: d.gps_fixed ? "Fixed" : "No fix",
      note: "GPS module",
      status: fresh && d.gps_fixed && d.latitude && d.longitude ? "working" : recent ? "waiting" : "check",
      message: d.gps_fixed ? "Location received" : "Move near open sky"
    }),
    sensorCheck({
      icon: "fa-battery-three-quarters",
      name: "Battery ADC",
      value: d.battery_level ? `${Math.round(d.battery_level)}%` : "--",
      note: "Battery monitor",
      status: fresh && d.battery_level > 0 ? "working" : recent ? "waiting" : "check",
      message: d.battery_level > 0 ? "Voltage received" : "No battery value"
    }),
    sensorCheck({
      icon: "fa-wifi",
      name: "WiFi",
      value: d.wifi_connected ? "Online" : "Offline",
      note: "Dashboard upload",
      status: fresh && d.wifi_connected ? "working" : "check",
      message: d.wifi_connected ? "Sending data" : "Check network"
    }),
    sensorCheck({
      icon: "fa-tower-cell",
      name: "SIM800L",
      value: d.gsm_connected ? "Ready" : "Offline",
      note: "SMS and call backup",
      status: fresh && d.gsm_connected ? "working" : recent ? "waiting" : "check",
      message: d.gsm_connected ? "Module ready" : "Check SIM/power"
    })
  ];

  grid.innerHTML = checks.map(renderSensorCheck).join("");

  const working = checks.filter(item => item.status === "working").length;
  const check = checks.filter(item => item.status === "check").length;
  const summary = document.getElementById("sensor-health-summary");
  if (summary) {
    summary.className = `health-summary ${check ? "check" : working === checks.length ? "working" : "waiting"}`;
    summary.textContent = check ? `${check} need check` : `${working}/${checks.length} working`;
  }

  setText("status-device-id", d.device_id);
  setText("status-last-update", `${formatTime(d.recorded_at)}${ageMs !== null ? ` (${ageLabel(ageMs)})` : ""}`);
  setText("status-wifi", d.wifi_connected ? "Online" : "Offline");
  setText("status-gsm", d.gsm_connected ? "Ready" : "Offline");
}

function sensorCheck(config) {
  return config;
}

function renderSensorCheck(item) {
  const labels = {
    working: "Working",
    waiting: "Waiting",
    check: "Check"
  };

  return `
    <article class="sensor-status-card ${item.status}">
      <div class="sensor-status-icon"><i class="fa-solid ${item.icon}"></i></div>
      <div class="sensor-status-body">
        <div>
          <h3>${escapeHtml(item.name)}</h3>
          <span>${escapeHtml(item.note)}</span>
        </div>
        <strong>${escapeHtml(item.value)}</strong>
        <small>${escapeHtml(item.message)}</small>
      </div>
      <span class="sensor-state">${labels[item.status]}</span>
    </article>
  `;
}

function updateLiveTable(d) {
  const rows = [
    ["MAX30102 Heart Rate", d.heart_rate ? `${Math.round(d.heart_rate)} BPM` : "--", hrStatusLabel(d.heart_rate)],
    ["MAX30102 SpO2", d.spo2 ? `${d.spo2.toFixed(1)}%` : "--", d.spo2 < 94 && d.spo2 > 0 ? "Low oxygen" : "Normal"],
    ["DHT11 Body Temperature", d.temperature ? `${d.temperature.toFixed(1)} C` : "--", "Body temperature"],
    ["SW-420 Vibration", d.vibration_count, d.vibration_count >= 3 || d.alert_type === 5 ? "Trigger level" : "Quiet"],
    ["VC-02 Voice Command", voiceLabel(d.voice_command), d.voice_command ? `Command 0x${hex(d.voice_command)}` : "No command"],
    ["Battery", `${Math.round(d.battery_level)}%`, d.battery_level < 20 ? "Charge needed" : "Good"],
    ["GPS", d.gps_fixed ? "Fixed" : "No fix", `${coord(d.latitude)}, ${coord(d.longitude)}`],
    ["WiFi", d.wifi_connected ? "Online" : "Offline", "Data upload link"],
    ["GSM", d.gsm_connected ? "Ready" : "Offline", "SMS/call channel"]
  ];

  const html = rows.map(([name, value, note]) => `
    <div>
      <span>${escapeHtml(name)}</span>
      <strong>${escapeHtml(value)}</strong>
      <small>${escapeHtml(note)}</small>
    </div>
  `).join("");

  document.getElementById("live-table").innerHTML = html;
}

function updateLocationPanel(d) {
  setText("loc-lat", coord(d.latitude));
  setText("loc-lng", coord(d.longitude));
  setText("loc-speed", `${d.speed.toFixed(1)} km/h`);
  setText("loc-alt", `${Math.round(d.altitude)} m`);
  const link = document.getElementById("maps-link");
  if (link) link.href = `https://maps.google.com/?q=${d.latitude},${d.longitude}`;
}

async function loadAlerts() {
  let alerts = [];
  try {
    const payload = await apiGet("dashboard_data.php?action=alerts_history&limit=30");
    alerts = payload.rows || payload || [];
  } catch {
    alerts = demoAlerts();
  }

  renderRecentAlerts(alerts.slice(0, 5));
  renderAlertsTable(alerts);
}

function renderRecentAlerts(alerts) {
  const el = document.getElementById("recent-alerts");
  if (!el) return;

  el.innerHTML = alerts.length ? alerts.map(alert => `
    <tr>
      <td>${escapeHtml(alert.full_name || "User")}</td>
      <td>${escapeHtml(alert.alert_name || "Alert")}</td>
      <td>${severityBadge(alert.severity)}</td>
      <td>${formatTime(alert.created_at)}</td>
      <td>${statusBadge(alert.resolved)}</td>
    </tr>
  `).join("") : emptyRow(5, "No recent alerts");
}

function renderAlertsTable(alerts) {
  const el = document.getElementById("alerts-table");
  if (!el) return;

  el.innerHTML = alerts.length ? alerts.map(alert => {
    const location = alert.latitude && alert.longitude
      ? `${coord(alert.latitude)}, ${coord(alert.longitude)}`
      : "--";

    return `
      <tr>
        <td>${alert.id || "--"}</td>
        <td>${escapeHtml(alert.full_name || "User")}<br><small>${escapeHtml(alert.device_id || "")}</small></td>
        <td>${escapeHtml(alert.alert_name || "Alert")}<br>${severityBadge(alert.severity)}</td>
        <td>${escapeHtml(alert.reason || "--")}</td>
        <td>${Math.round(Number(alert.heart_rate || 0)) || "--"} BPM<br><small>${Math.round(Number(alert.battery_level || 0)) || "--"}% battery</small></td>
        <td>${location}</td>
        <td>${statusBadge(alert.resolved)}</td>
        <td>${alert.resolved ? "" : `<button class="btn btn-sm btn-outline-success" onclick="resolveAlert(${Number(alert.id)})">Resolve</button>`}</td>
      </tr>
    `;
  }).join("") : emptyRow(8, "No alerts found");
}

async function resolveAlert(id) {
  if (!id) return;
  try {
    await apiPost("dashboard_data.php?action=resolve_alert", { alert_id: id, notes: "Resolved from dashboard" });
    showToast("Alert resolved");
    loadAlerts();
    loadOverview();
  } catch (error) {
    showToast(error.message || "Unable to resolve alert", "danger");
  }
}

async function loadDevices() {
  let devices = [];
  try {
    devices = await apiGet("dashboard_data.php?action=device_status");
  } catch {
    devices = [demoDevice()];
  }

  const grid = document.getElementById("devices-grid");
  if (!grid) return;

  grid.innerHTML = devices.length ? devices.map(device => {
    const battery = Number(device.battery_level || 0);
    const online = Boolean(Number(device.is_online ?? 0) || device.is_online === true);
    return `
      <article class="device-card">
        <h3>${escapeHtml(device.device_name || "Safety Wearable")}</h3>
        <p>${escapeHtml(device.device_id || "--")} - ${online ? "Online" : "Offline"}</p>
        <div class="d-flex justify-content-between mb-2">
          <span>Battery</span><strong>${Math.round(battery)}%</strong>
        </div>
        <div class="battery-track"><div class="battery-fill ${battery < 20 ? "critical" : battery < 40 ? "low" : ""}" style="width:${Math.max(0, Math.min(100, battery))}%"></div></div>
        <div class="chip-grid mt-3">
          <span>MAX30102</span><span>DHT11</span><span>SW-420</span><span>VC-02</span><span>GPS</span><span>GSM</span>
        </div>
      </article>
    `;
  }).join("") : `<div class="text-muted">No devices found</div>`;
}

function initCharts() {
  const hrCtx = document.getElementById("heartRateChart");
  if (hrCtx) {
    charts.heartRate = new Chart(hrCtx, {
      type: "line",
      data: {
        labels: [],
        datasets: [{
          label: "Heart Rate",
          data: [],
          borderColor: "#dc2626",
          backgroundColor: "rgba(220, 38, 38, 0.08)",
          fill: true,
          tension: 0.35
        }]
      },
      options: chartOptions({ min: 40, max: 150 })
    });
  }

  const alertCtx = document.getElementById("alertTypesChart");
  if (alertCtx) {
    charts.alertTypes = new Chart(alertCtx, {
      type: "doughnut",
      data: {
        labels: [],
        datasets: [{ data: [], backgroundColor: ["#dc2626", "#d97706", "#2563eb", "#0f766e", "#16a34a"] }]
      },
      options: { plugins: { legend: { position: "bottom" } }, cutout: "62%" }
    });
  }

  const signalCtx = document.getElementById("sensorSignalChart");
  if (signalCtx) {
    charts.sensorSignal = new Chart(signalCtx, {
      type: "line",
      data: {
        labels: [],
        datasets: [
          { label: "Heart BPM", data: [], borderColor: "#dc2626", backgroundColor: "rgba(220,38,38,0.08)", tension: 0.35, yAxisID: "vitals" },
          { label: "SpO2 %", data: [], borderColor: "#2563eb", backgroundColor: "rgba(37,99,235,0.08)", tension: 0.35, yAxisID: "vitals" },
          { label: "Temp C", data: [], borderColor: "#d97706", backgroundColor: "rgba(217,119,6,0.08)", tension: 0.35, yAxisID: "vitals" },
          { label: "Battery %", data: [], borderColor: "#16a34a", backgroundColor: "rgba(22,163,74,0.08)", tension: 0.35, yAxisID: "vitals" },
          { label: "Vibration", data: [], borderColor: "#7c3aed", backgroundColor: "rgba(124,58,237,0.08)", tension: 0.35, yAxisID: "events" }
        ]
      },
      options: sensorSignalOptions()
    });
  }
}

async function loadHeartRateChart() {
  const hours = document.getElementById("hr-range")?.value || "24";
  let rows = [];

  try {
    rows = await apiGet(`dashboard_data.php?action=heart_rate_chart&device_id=${encodeURIComponent(activeDeviceId)}&hours=${hours}`);
  } catch {
    rows = demoHeartRows(Number(hours));
  }

  const labels = rows.map(row => row.time_label || row.label);
  const data = rows.map(row => Number(row.avg_hr || row.heart_rate || 0));
  updateChart(charts.heartRate, labels, data);
}

async function loadSensorSignalChart() {
  const hours = document.getElementById("hr-range")?.value || "24";
  let rows = [];

  try {
    rows = await apiGet(`dashboard_data.php?action=heart_rate_chart&device_id=${encodeURIComponent(activeDeviceId)}&hours=${hours}`);
  } catch {
    rows = demoHeartRows(Number(hours));
  }

  if (!charts.sensorSignal) return;

  charts.sensorSignal.data.labels = rows.map(row => row.time_label || row.label);
  charts.sensorSignal.data.datasets[0].data = rows.map(row => Number(row.avg_hr || row.heart_rate || 0));
  charts.sensorSignal.data.datasets[1].data = rows.map(row => Number(row.avg_spo2 || row.spo2 || 0));
  charts.sensorSignal.data.datasets[2].data = rows.map(row => Number(row.avg_temp || row.temperature || 0));
  charts.sensorSignal.data.datasets[3].data = rows.map(row => Number(row.avg_battery || row.battery_level || 0));
  charts.sensorSignal.data.datasets[4].data = rows.map(row => Number(row.max_vibration || row.vibration_count || 0));
  charts.sensorSignal.update();
}

async function loadAlertTypesChart() {
  let rows = [];
  try {
    rows = await apiGet("dashboard_data.php?action=alert_types_chart");
  } catch {
    rows = [
      { alert_name: "SOS Button Pressed", count: 3 },
      { alert_name: "Voice Command", count: 2 },
      { alert_name: "High Heart Rate", count: 2 },
      { alert_name: "Excessive Vibration", count: 1 }
    ];
  }

  if (!charts.alertTypes) return;
  charts.alertTypes.data.labels = rows.map(row => row.alert_name || "Alert");
  charts.alertTypes.data.datasets[0].data = rows.map(row => Number(row.count || 0));
  charts.alertTypes.update();
}

function initMap() {
  if (map) {
    setTimeout(() => map.invalidateSize(), 50);
    return;
  }

  const start = latestSensor || demoSensor;
  map = L.map("map").setView([start.latitude || 12.971598, start.longitude || 77.594562], 15);
  L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
    attribution: "OpenStreetMap"
  }).addTo(map);

  marker = L.marker([start.latitude || 12.971598, start.longitude || 77.594562]).addTo(map);
  marker.bindPopup("Safety wearable location");
  updateMapMarker(start);
}

function updateMapMarker(d) {
  if (!map || !marker || !d.latitude || !d.longitude) return;
  marker.setLatLng([d.latitude, d.longitude]);
  marker.setPopupContent(`${escapeHtml(d.device_id)}<br>${coord(d.latitude)}, ${coord(d.longitude)}`);
  map.setView([d.latitude, d.longitude], map.getZoom());
}

async function triggerManualSOS() {
  if (!confirm("Trigger manual SOS alert on this dashboard?")) return;

  const payload = {
    device_id: activeDeviceId,
    alert_type: 1,
    alert_name: "Dashboard SOS",
    reason: "Triggered manually from dashboard",
    latitude: latestSensor?.latitude || 0,
    longitude: latestSensor?.longitude || 0,
    heart_rate: latestSensor?.heart_rate || 0,
    spo2: latestSensor?.spo2 || 0,
    battery: latestSensor?.battery_level || 0
  };

  try {
    await apiPost("emergency_alert.php", payload);
    document.getElementById("emergency-strip")?.classList.remove("d-none");
    setText("emergency-text", "Manual SOS triggered from dashboard.");
    showToast("Manual SOS triggered", "danger");
    await Promise.all([loadAlerts(), loadOverview()]);
  } catch (error) {
    showToast(error.message || "Unable to trigger SOS", "danger");
  }
}

async function apiGet(path) {
  const response = await fetch(API_BASE + path);
  const json = await response.json();
  if (!response.ok || json.status !== "success") throw new Error(json.message || "API request failed");
  return json.data;
}

async function apiPost(path, payload) {
  const response = await fetch(API_BASE + path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload)
  });
  const json = await response.json();
  if (!response.ok || json.status !== "success") throw new Error(json.message || "API request failed");
  return json.data;
}

function makeDemoSensor() {
  const t = Date.now();
  return {
    ...demoSensor,
    heart_rate: 76 + Math.sin(t / 7000) * 10 + (Math.random() - 0.5) * 4,
    spo2: 97 + Math.sin(t / 9000) * 1.2,
    battery_level: Math.max(20, demoSensor.battery_level - Math.random() * 0.05),
    temperature: 36.6 + (Math.random() - 0.5) * 0.4,
    vibration_count: Math.random() > 0.9 ? Math.floor(Math.random() * 4) : 0,
    voice_command: Math.random() > 0.97 ? 1 : 0,
    recorded_at: new Date().toISOString()
  };
}

function demoAlerts() {
  return [
    { id: 1, full_name: "Wife User", device_id: FALLBACK_DEVICE_ID, alert_name: "Voice Command", severity: "critical", reason: "VC-02 emergency command", heart_rate: 92, battery_level: 82, latitude: 12.971598, longitude: 77.594562, resolved: 0, created_at: new Date().toISOString() },
    { id: 2, full_name: "Wife User", device_id: FALLBACK_DEVICE_ID, alert_name: "SOS Button Pressed", severity: "critical", reason: "Triple press SOS", heart_rate: 118, battery_level: 83, latitude: 12.971598, longitude: 77.594562, resolved: 1, created_at: new Date(Date.now() - 3600000).toISOString() },
    { id: 3, full_name: "Wife User", device_id: FALLBACK_DEVICE_ID, alert_name: "High Heart Rate", severity: "high", reason: "MAX30102 exceeded threshold", heart_rate: 134, battery_level: 85, latitude: 12.971598, longitude: 77.594562, resolved: 1, created_at: new Date(Date.now() - 86400000).toISOString() }
  ];
}

function demoDevice() {
  return {
    device_id: FALLBACK_DEVICE_ID,
    device_name: "ESP32 Safety Wearable",
    battery_level: 84,
    is_online: true
  };
}

function demoHeartRows(hours) {
  const points = Math.min(hours * 4, 96);
  const now = Date.now();
  return Array.from({ length: points + 1 }, (_, index) => {
    const minutesBack = (points - index) * 15;
    const date = new Date(now - minutesBack * 60000);
    return {
      time_label: date.toLocaleTimeString("en-IN", { hour: "2-digit", minute: "2-digit" }),
      avg_hr: 76 + Math.sin(index / 5) * 12 + (Math.random() - 0.5) * 5,
      avg_spo2: 97 + Math.sin(index / 7) * 1.4,
      avg_temp: 36.6 + Math.sin(index / 9) * 0.4,
      avg_battery: Math.max(20, 86 - index * 0.08),
      max_vibration: Math.random() > 0.92 ? Math.floor(Math.random() * 4) : 0
    };
  });
}

function chartOptions(scale = {}) {
  return {
    responsive: true,
    maintainAspectRatio: false,
    plugins: { legend: { display: false } },
    scales: {
      y: { suggestedMin: scale.min, suggestedMax: scale.max, grid: { color: "rgba(148,163,184,0.2)" } },
      x: { grid: { display: false }, ticks: { maxTicksLimit: 8 } }
    }
  };
}

function sensorSignalOptions() {
  return {
    responsive: true,
    maintainAspectRatio: false,
    interaction: { mode: "index", intersect: false },
    plugins: {
      legend: { position: "bottom", labels: { boxWidth: 10, usePointStyle: true } }
    },
    scales: {
      vitals: {
        type: "linear",
        position: "left",
        suggestedMin: 0,
        suggestedMax: 150,
        grid: { color: "rgba(148,163,184,0.2)" }
      },
      events: {
        type: "linear",
        position: "right",
        suggestedMin: 0,
        suggestedMax: 5,
        grid: { drawOnChartArea: false }
      },
      x: { grid: { display: false }, ticks: { maxTicksLimit: 8 } }
    }
  };
}

function updateChart(chart, labels, data) {
  if (!chart) return;
  chart.data.labels = labels;
  chart.data.datasets[0].data = data;
  chart.update();
}

function severityBadge(severity = "medium") {
  const safe = String(severity || "medium").toLowerCase();
  const cls = ["critical", "high", "medium", "low"].includes(safe) ? safe : "medium";
  return `<span class="badge-soft badge-${cls}">${escapeHtml(cls)}</span>`;
}

function statusBadge(resolved) {
  const done = Number(resolved || 0) === 1 || resolved === true;
  return `<span class="badge-soft ${done ? "badge-closed" : "badge-open"}">${done ? "Resolved" : "Open"}</span>`;
}

function emptyRow(cols, text) {
  return `<tr><td colspan="${cols}" class="text-center text-muted py-4">${escapeHtml(text)}</td></tr>`;
}

function voiceLabel(command) {
  const value = Number(command || 0);
  if (!value) return "Idle";
  const commands = {
    1: "Help Me",
    2: "Emergency",
    3: "Save Me",
    4: "Danger",
    5: "Call Police"
  };
  if (commands[value]) return `${commands[value]} 0x${hex(value)}`;
  return `Cmd 0x${hex(value)}`;
}

function hrStatus(hr) {
  const value = Number(hr || 0);
  if (value > 130 || (value > 0 && value < 45)) return "danger";
  if (value > 100 || (value > 0 && value < 55)) return "warning";
  return "normal";
}

function hrStatusLabel(hr) {
  const value = Number(hr || 0);
  if (!value) return "Waiting";
  if (value > 130) return "High";
  if (value < 45) return "Low";
  if (value > 100) return "Elevated";
  return "Normal";
}

function colorVital(id, status) {
  const el = document.getElementById(id);
  if (!el) return;
  el.classList.remove("normal", "warning", "danger");
  el.classList.add(status);
}

function formatTime(value) {
  if (!value) return "--";
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return String(value);
  return date.toLocaleString("en-IN", { hour: "2-digit", minute: "2-digit", day: "2-digit", month: "short" });
}

function readingAgeMs(value) {
  if (!value) return null;
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return null;
  return Math.max(0, Date.now() - date.getTime());
}

function ageLabel(ms) {
  const seconds = Math.round(ms / 1000);
  if (seconds < 60) return `${seconds}s ago`;
  const minutes = Math.round(seconds / 60);
  return `${minutes}m ago`;
}

function coord(value) {
  const num = Number(value || 0);
  return num ? num.toFixed(6) : "--";
}

function number(value) {
  return Number(value || 0);
}

function hex(value) {
  return Number(value || 0).toString(16).padStart(2, "0").toUpperCase();
}

function setText(id, value) {
  const el = document.getElementById(id);
  if (el) el.textContent = value;
}

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>"']/g, char => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#039;"
  }[char]));
}

function rotateRefresh(active) {
  const icon = document.querySelector("#refresh-btn i");
  if (icon) icon.style.animation = active ? "spin 0.8s linear infinite" : "";
}

function showToast(message, type = "success") {
  const toast = document.getElementById("mainToast");
  const body = document.getElementById("toast-message");
  if (!toast || !body || !window.bootstrap) return;

  toast.style.background = type === "danger" ? "#dc2626" : type === "warning" ? "#d97706" : "#142033";
  body.textContent = message;
  bootstrap.Toast.getOrCreateInstance(toast, { delay: 2500 }).show();
}

window.resolveAlert = resolveAlert;
