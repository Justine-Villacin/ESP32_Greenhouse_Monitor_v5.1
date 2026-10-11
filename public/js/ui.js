// Automation cards show one of three states, not two:
//   ACTIVE   - the actuator is genuinely running
//   STANDBY  - its sensor is fine, conditions just don't call for it
//   OFFLINE  - the sensor it depends on is disconnected, so the relay is
//              forced off for safety and the card says so explicitly.
export function updateAutomationUI(data) {
    // --- Pump (depends on soil sensor) ---
    const pumpCard = document.getElementById("autoWateringCard");
    const pumpPill = document.getElementById("pumpPill");
    pumpCard.classList.remove("active-pump", "offline");
    
    if (!data.soilConnected) {
        pumpCard.classList.add("offline");
        document.getElementById("pumpTitle").textContent = "Auto-Watering";
        document.getElementById("pumpDesc").textContent = "Soil sensor disconnected — disabled.";
        if (pumpPill) pumpPill.textContent = "Offline";
    } else if (data.pump) {
        pumpCard.classList.add("active-pump");
        document.getElementById("pumpTitle").textContent = "Auto-Watering";
        document.getElementById("pumpDesc").textContent = "Soil moisture <= 40% (Dry). Pumping water.";
        if (pumpPill) pumpPill.textContent = "Active";
    } else {
        document.getElementById("pumpTitle").textContent = "Auto-Watering";
        document.getElementById("pumpDesc").textContent = "Soil moisture >= 55% (Hydrated).";
        if (pumpPill) pumpPill.textContent = "Standby";
    }

    // --- Grow light (depends on light sensor) ---
    const lightCard = document.getElementById("autoLightCard");
    const lightPill = document.getElementById("lightPill");
    lightCard.classList.remove("active-light", "offline");
    
    if (!data.lightConnected) {
        lightCard.classList.add("offline");
        document.getElementById("lightTitle").textContent = "Grow Light";
        document.getElementById("lightDesc").textContent = "Light sensor disconnected — disabled.";
        if (lightPill) lightPill.textContent = "Offline";
    } else if (data.lightState) {
        lightCard.classList.add("active-light");
        document.getElementById("lightTitle").textContent = "Grow Light";
        document.getElementById("lightDesc").textContent = "Low light detected (<= 30%). Supplementing.";
        if (lightPill) lightPill.textContent = "Active";
    } else {
        document.getElementById("lightTitle").textContent = "Grow Light";
        document.getElementById("lightDesc").textContent = "Ambient light is sufficient.";
        if (lightPill) lightPill.textContent = "Standby";
    }

    // --- Exhaust fan (depends on DHT temperature sensor) ---
    const fanCard = document.getElementById("autoFanCard");
    const fanPill = document.getElementById("fanPill");
    fanCard.classList.remove("active-fan", "offline");
    
    if (!data.dhtConnected) {
        fanCard.classList.add("offline");
        document.getElementById("fanTitle").textContent = "Exhaust Fan";
        document.getElementById("fanDesc").textContent = "Temperature sensor disconnected — disabled.";
        if (fanPill) fanPill.textContent = "Offline";
    } else if (data.fan) {
        fanCard.classList.add("active-fan");
        document.getElementById("fanTitle").textContent = "Exhaust Fan";
        document.getElementById("fanDesc").textContent = "High temp >= 30°C. Cooling down.";
        if (fanPill) fanPill.textContent = "Active";
    } else {
        document.getElementById("fanTitle").textContent = "Exhaust Fan";
        document.getElementById("fanDesc").textContent = "Temperature is optimal (< 28.5°C).";
        if (fanPill) fanPill.textContent = "Standby";
    }
}

function setSensorOffline(cardSelector, isOffline) {
    const card = document.querySelector(cardSelector);
    if (card) card.classList.toggle("offline", isOffline);
}

export function updateSensorUI(data) {
    const tempEl = document.getElementById("temperature");
    const humEl  = document.getElementById("humidity");
    const soilEl = document.getElementById("soil");
    const lightEl = document.getElementById("light");

    const tempStatus  = tempEl  && tempEl.closest(".reading").querySelector(".reading-state");
    const humStatus   = humEl   && humEl.closest(".reading").querySelector(".reading-state");
    const soilStatus  = soilEl  && soilEl.closest(".reading").querySelector(".reading-state");
    const lightStatus = lightEl && lightEl.closest(".reading").querySelector(".reading-state");

    setSensorOffline(".sensor.temperature", !data.dhtConnected);
    setSensorOffline(".sensor.humidity",    !data.dhtConnected);

    if (tempEl && humEl) {
        if (!data.dhtConnected) {
            tempEl.textContent = "--";
            if (tempStatus) { tempStatus.textContent = "OFFLINE"; tempStatus.style.color = "var(--danger)"; }
            humEl.textContent = "--";
            if (humStatus) { humStatus.textContent = "OFFLINE"; humStatus.style.color = "var(--danger)"; }
        } else {
            tempEl.textContent = Number(data.temp).toFixed(1);
            if (tempStatus) {
                tempStatus.textContent = data.temp >= 30 ? "WARNING" : "NORMAL";
                tempStatus.style.color = data.temp >= 30 ? "var(--danger)" : "var(--primary)";
            }
            humEl.textContent = Math.round(data.hum);
            if (humStatus) {
                humStatus.textContent = data.hum >= 60 && data.hum <= 80 ? "NORMAL" : "WARNING";
                humStatus.style.color = data.hum >= 60 && data.hum <= 80 ? "var(--primary)" : "var(--warning)";
            }
        }
    }

    setSensorOffline(".sensor.soil", !data.soilConnected);
    if (soilEl) {
        if (!data.soilConnected) {
            soilEl.textContent = "--";
            if (soilStatus) { soilStatus.textContent = "OFFLINE"; soilStatus.style.color = "var(--danger)"; }
        } else {
            soilEl.textContent = Math.round(data.soil);
            if (soilStatus) {
                soilStatus.textContent = data.soil <= 40 ? "LOW" : "GOOD";
                soilStatus.style.color = data.soil <= 40 ? "var(--danger)" : "var(--primary)";
            }
        }
    }

    setSensorOffline(".sensor.light", !data.lightConnected);
    if (lightEl) {
        if (!data.lightConnected) {
            lightEl.textContent = "--";
            if (lightStatus) { lightStatus.textContent = "OFFLINE"; lightStatus.style.color = "var(--danger)"; }
        } else {
            lightEl.textContent = Math.round(data.light);
            if (lightStatus) {
                lightStatus.textContent = data.light >= 300 ? "OPTIMAL" : "LOW";
                lightStatus.style.color  = data.light >= 300 ? "var(--primary)" : "var(--danger)";
            }
        }
    }

    let score = 100;
    let tempStatusText = "Optimal";
    let humStatusText = "Optimal";
    let soilStatusText = "Moist";
    let lightStatusText = "Optimal";

    if (data.dhtConnected) {
        if (data.temp >= 30) {
            score -= 25;
            tempStatusText = "Too Hot";
        } else if (data.temp < 15) {
            score -= 15;
            tempStatusText = "Too Cold";
        }
    } else {
        score -= 25;
        tempStatusText = "No Data";
    }

    if (data.dhtConnected) {
        if (data.hum > 85) {
            score -= 15;
            humStatusText = "Too Humid";
        } else if (data.hum < 50) {
            score -= 20;
            humStatusText = "Dry Air";
        }
    } else {
        score -= 20;
        humStatusText = "No Data";
    }

    if (data.soilConnected) {
        if (data.soil <= 40) {
            score -= 35;
            soilStatusText = "Dry";
        } else if (data.soil > 85) {
            score -= 10;
            soilStatusText = "Too Wet";
        }
    } else {
        score -= 35;
        soilStatusText = "No Data";
    }

    if (data.lightConnected) {
        if (data.light < 300) {
            score -= 25;
            lightStatusText = "Low";
        }
    } else {
        score -= 25;
        lightStatusText = "No Data";
    }

    score = Math.max(0, score);

    const healthScoreEl = document.getElementById("healthScore");
    const healthProgressEl = document.getElementById("healthProgress");

    if (healthScoreEl) {
        healthScoreEl.textContent = `${score}%`;
        const ringContainer = document.getElementById("vigorRingContainer");
        if (ringContainer) {
            ringContainer.style.setProperty('--pct', score);
        }
    }
    if (healthProgressEl) {
        healthProgressEl.style.width = `${score}%`;

        if (score >= 80) {
            healthProgressEl.style.background = "var(--primary)";
            healthProgressEl.style.boxShadow = "0 0 10px var(--primary-glow)";
        } else if (score >= 50) {
            healthProgressEl.style.background = "var(--warning)";
            healthProgressEl.style.boxShadow = "0 0 10px var(--warning-glow)";
        } else {
            healthProgressEl.style.background = "var(--danger)";
            healthProgressEl.style.boxShadow = "0 0 10px var(--danger-glow)";
        }
    }

    function setMetricUI(id, text) {
        const el = document.getElementById(id);
        if (!el) return;
        el.textContent = text;

        el.className = "metric-value";
        if (text === "Optimal" || text === "Moist") {
            el.classList.add("good");
        } else if (text === "Low" || text === "Too Wet" || text === "Dry Air" || text === "Too Cold") {
            el.classList.add("warning");
        } else if (text === "Too Hot" || text === "Dry" || text === "Too Humid" || text === "No Data") {
            el.classList.add("danger");
        }
    }

    setMetricUI("tempHealth", tempStatusText);
    setMetricUI("humidityHealth", humStatusText);
    setMetricUI("soilHealth", soilStatusText);
    setMetricUI("lightHealth", lightStatusText);

    // ==========================================
    // DYNAMIC SMART INSIGHTS & CLIMATE STATUS
    // ==========================================
    const insightTemp = document.getElementById("insightTemp");
    const insightSoil = document.getElementById("insightSoil");
    const climateText = document.querySelector(".climate-text");

    if (insightTemp) {
        if (!data.dhtConnected) {
            insightTemp.style.display = "none";
        } else if (data.temp >= 30.0) {
            insightTemp.style.display = "flex";
            insightTemp.querySelector("h4").textContent = "High Temperature Alert";
            insightTemp.querySelector("p").textContent = `Temp is ${data.temp.toFixed(1)}°C (>= 30°C). Exhaust fan active.`;
        } else {
            insightTemp.style.display = "none";
        }
    }

    if (insightSoil) {
        if (!data.soilConnected) {
            insightSoil.style.display = "none";
        } else if (data.soil <= 40) {
            insightSoil.style.display = "flex";
            insightSoil.querySelector("h4").textContent = "Irrigation Recommended";
            insightSoil.querySelector("p").textContent = `Soil moisture is at ${Math.round(data.soil)}% (<= 40%). Pump standby/active.`;
        } else {
            insightSoil.style.display = "none";
        }
    }

    if (climateText) {
        let activeActs = [];
        if (data.fan) activeActs.push("Fan Cooling");
        if (data.pump) activeActs.push("Watering");
        if (data.lightState) activeActs.push("Grow Light");

        if (activeActs.length > 0) {
            climateText.innerHTML = `<strong>Climate System Active</strong> Regulating: ${activeActs.join(", ")}.`;
        } else {
            climateText.innerHTML = `<strong>Climate System Stable</strong> All metrics within optimal thresholds.`;
        }
    }
}