import { updateDateTime } from './utils.js';
import { updateSensorUI, updateAutomationUI } from './ui.js';
import './pwa.js'; // Imports and runs PWA logic automatically
import './navigation.js'; // Imports and runs Navigation logic automatically

// Initialize date and time
updateDateTime();
setInterval(updateDateTime, 60000);

// Consecutive /api/sensors failures before we show the "backend unreachable"
// banner. This is a different failure mode from "ESP32 disconnected" (which
// the topbar badge already shows) — this means the dashboard itself can't
// reach the Flask backend at all (server down, network issue, etc).
const BACKEND_FAILURE_THRESHOLD = 2;
let consecutiveFailures = 0;

function showBackendBanner(message) {
    const banner = document.getElementById("backendBanner");
    const text = document.getElementById("backendBannerText");
    if (!banner) return;
    if (text) text.textContent = message;
    banner.style.display = "flex";
}

function hideBackendBanner() {
    const banner = document.getElementById("backendBanner");
    if (banner) banner.style.display = "none";
}

function fetchESP32Data() {
    fetch('/api/sensors')
        .then(response => {
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return response.json();
        })
        .then(data => {
            consecutiveFailures = 0;
            hideBackendBanner();

            // Update the UI components
            updateSensorUI(data);
            updateAutomationUI(data);

            const badge = document.getElementById("espBadge");
            const dot = document.getElementById("espDot");
            const text = document.getElementById("espText");

            // Handle ESP32 Connection Status
            if (data.esp32Connected) {
                text.textContent = "ESP32 CONNECTED";
                badge.style.color = "var(--primary)";
                dot.style.background = "var(--primary)";
                dot.style.boxShadow = "0 0 10px var(--primary)";
                dot.style.animation = "pulse 2s infinite";
            } else {
                text.textContent = "ESP32 DISCONNECTED";
                badge.style.color = "var(--danger)";
                dot.style.background = "var(--danger)";
                dot.style.boxShadow = "none";
                dot.style.animation = "none";
            }

            // Update footer timestamp
            const time = new Date().toLocaleTimeString("en-US", { hour: "numeric", minute: "2-digit" });
            document.getElementById("footerStatus").textContent = `System online • Last sync at ${time}`;

            // NOTE: The daily averages live calculation block was removed
            // to match the updated HTML structure.
        })
        .catch(err => {
            console.error("Connection error:", err);
            consecutiveFailures += 1;

            const footerStatus = document.getElementById("footerStatus");
            if (footerStatus) {
                footerStatus.textContent = "Connection lost";
            }

            if (consecutiveFailures >= BACKEND_FAILURE_THRESHOLD) {
                showBackendBanner("Backend unreachable — the dashboard can't reach the server. Retrying...");
            }
        });
}

async function loadHistoricalData() {
    try {
        const response = await fetch('/api/history');
        const data = await response.json();
        
        if (data.status === 'success' && data.history.length > 0) {
            // 1. Populate the Recent strip on the dashboard (taking the last 2 days)
            const recentList = document.querySelector('.panel-strip .history-list');
            if (recentList) {
                recentList.innerHTML = '';
                const recentItems = data.history.slice(-2).reverse();
                recentItems.forEach(day => {
                    recentList.innerHTML += `
                        <div class="history-item">
                            <span class="history-date">${day.date}</span>
                            <div class="history-stats">
                                <span class="t"><i class="ph-fill ph-thermometer"></i> ${day.temp}&deg;C</span>
                                <span class="h"><i class="ph-fill ph-drop"></i> ${day.hum}%</span>
                                <span class="s"><i class="ph-fill ph-potted-plant"></i> ${day.soil}%</span>
                            </div>
                        </div>
                    `;
                });
            }

            // 2. Populate the 7-Day Modal Table using the new ID
            const modalList = document.getElementById('modalHistoryList');
            if (modalList) {
                modalList.innerHTML = '';
                data.history.slice().reverse().forEach(day => {
                    modalList.innerHTML += `
                        <div style="display: grid; grid-template-columns: 1.2fr 1fr 1fr 1fr; align-items: center; padding: 12px 16px; background: linear-gradient(145deg, rgba(255,255,255,0.06), rgba(255,255,255,0.01)); border: 1px solid rgba(255,255,255,0.04); border-radius: 12px;">
                            <div style="display: flex; flex-direction: column; gap: 2px;">
                                <span style="font-size: 13px; font-weight: 600; color: var(--text-main);">${day.date}</span>
                            </div>
                            <div style="text-align: center; color: var(--c-temp); font-size: 13px; font-weight: 700;">${day.temp}&deg;</div>
                            <div style="text-align: center; color: var(--c-hum); font-size: 13px; font-weight: 700;">${day.hum}%</div>
                            <div style="text-align: right; color: var(--c-soil); font-size: 13px; font-weight: 700;">${day.soil}%</div>
                        </div>
                    `;
                });
            }
        }
    } catch (err) {
        console.error("Could not load historical averages:", err);
    }
}

loadHistoricalData();
// Initial fetch and start polling every 2 seconds
fetchESP32Data();
setInterval(fetchESP32Data, 2000);
