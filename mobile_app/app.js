const API_BASE = "../api/";
const DEVICE_ID = "WIFE_ESP32_001";
const DEVICE_TOKEN = "your_secret_token_here";

const sosButton = document.getElementById("sos-btn");
const sosStatus = document.getElementById("sos-status");
const locationText = document.getElementById("location-text");
const mapLink = document.getElementById("map-link");
const alertText = document.getElementById("alert-text");
const refreshButton = document.getElementById("refresh-btn");

let pressTimer = null;

function setStatus(text, isError = false) {
  sosStatus.textContent = text;
  sosStatus.style.color = isError ? "#b00020" : "#2a1f24";
}

async function triggerSOS() {
  setStatus("Sending SOS...");
  const position = await getCurrentPositionSafe();
  const payload = {
    device_id: DEVICE_ID,
    alert_type: 1,
    alert_name: "Mobile SOS Trigger",
    reason: "Triggered from mobile app",
    latitude: position?.latitude || 0,
    longitude: position?.longitude || 0,
    heart_rate: 0,
    spo2: 0,
    battery: 0
  };

  try {
    const res = await fetch(API_BASE + "emergency_alert.php", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "Authorization": `Bearer ${DEVICE_TOKEN}`
      },
      body: JSON.stringify(payload)
    });
    const data = await res.json();
    if (!res.ok || data.status !== "success") {
      throw new Error(data.message || "SOS failed");
    }
    setStatus("SOS sent successfully.");
    await fetchLatestAlert();
  } catch (err) {
    setStatus("SOS failed. Check API/token config.", true);
  }
}

function getCurrentPositionSafe() {
  return new Promise((resolve) => {
    if (!navigator.geolocation) {
      resolve(null);
      return;
    }

    navigator.geolocation.getCurrentPosition(
      (pos) => resolve({
        latitude: pos.coords.latitude,
        longitude: pos.coords.longitude
      }),
      () => resolve(null),
      { enableHighAccuracy: true, timeout: 7000 }
    );
  });
}

async function refreshLocation() {
  const pos = await getCurrentPositionSafe();
  if (!pos) {
    locationText.textContent = "Location permission denied or unavailable.";
    mapLink.href = "#";
    return;
  }
  locationText.textContent = `${pos.latitude.toFixed(6)}, ${pos.longitude.toFixed(6)}`;
  mapLink.href = `https://maps.google.com/?q=${pos.latitude},${pos.longitude}`;
}

async function fetchLatestAlert() {
  try {
    const res = await fetch(API_BASE + "dashboard_data.php?action=alerts_history&limit=1");
    const data = await res.json();
    const row = data?.data?.rows?.[0];
    if (!row) {
      alertText.textContent = "No recent alerts available.";
      return;
    }
    const time = row.created_at || "Unknown time";
    alertText.textContent = `${row.alert_name} (${row.severity}) at ${time}`;
  } catch (_) {
    alertText.textContent = "Could not load latest alert.";
  }
}

sosButton.addEventListener("mousedown", () => {
  setStatus("Keep holding...");
  pressTimer = setTimeout(triggerSOS, 1500);
});

sosButton.addEventListener("mouseup", () => {
  clearTimeout(pressTimer);
});

sosButton.addEventListener("mouseleave", () => {
  clearTimeout(pressTimer);
});

sosButton.addEventListener("touchstart", () => {
  setStatus("Keep holding...");
  pressTimer = setTimeout(triggerSOS, 1500);
}, { passive: true });

sosButton.addEventListener("touchend", () => {
  clearTimeout(pressTimer);
}, { passive: true });

refreshButton.addEventListener("click", async () => {
  await refreshLocation();
  await fetchLatestAlert();
});

if ("serviceWorker" in navigator) {
  navigator.serviceWorker.register("./sw.js").catch(() => {});
}

refreshLocation();
fetchLatestAlert();
