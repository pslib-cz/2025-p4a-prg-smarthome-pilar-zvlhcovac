/* ============================================================
   Smart Humidifier — Dashboard JS (MQTT & UI logic)
   ============================================================ */

const DOM = {
  login: {
    overlay: document.getElementById('login-overlay'),
    btnConnect: document.getElementById('btn-connect'),
    urlInput: document.getElementById('broker-url'),
    userInput: document.getElementById('mqtt-user'),
    passInput: document.getElementById('mqtt-pass'),
    errorMsg: document.getElementById('login-error')
  },
  app: document.getElementById('app'),
  header: {
    statusDot: document.getElementById('status-dot'),
    statusText: document.getElementById('status-text'),
    btnDisconnect: document.getElementById('btn-disconnect')
  },
  sensors: {
    humidityVal: document.getElementById('val-humidity'),
    humiditySub: document.getElementById('sub-humidity'),
    temperatureVal: document.getElementById('val-temperature'),
    temperatureSub: document.getElementById('sub-temperature')
  },
  control: {
    targetVal: document.getElementById('val-target'),
    btnMinus: document.getElementById('btn-minus'),
    btnPlus: document.getElementById('btn-plus'),
    btnModes: document.querySelectorAll('.btn-mode'),
    statusIcon: document.getElementById('status-icon'),
    statusLabel: document.getElementById('status-label'),
    statusContainer: document.getElementById('humidifier-status')
  },
  diag: {
    rssi: document.getElementById('diag-rssi'),
    uptime: document.getElementById('diag-uptime'),
    broker: document.getElementById('diag-broker'),
    last: document.getElementById('diag-last')
  }
};

let mqttClient = null;
let humChart = null;

const TOPICS = {
  SUB: {
    HUMIDITY: 'humidifier/sensor/humidity',
    TEMPERATURE: 'humidifier/sensor/temperature',
    TARGET: 'humidifier/number/target',
    POWER: 'humidifier/switch/power',
    STATUS: 'humidifier/status',
    RSSI: 'humidifier/sensor/rssi',
    UPTIME: 'humidifier/sensor/uptime'
  },
  PUB: {
    TARGET_SET: 'humidifier/number/target/set',
    POWER_SET: 'humidifier/switch/power/set'
  }
};

// ── Gauge kreslení ──────────────────────────────────────────
function drawGauge(value) {
  const canvas = document.getElementById('gauge-humidity');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const w = canvas.width;
  const h = canvas.height;
  const cx = w / 2;
  const cy = h - 10;
  const r = cy - 20;

  ctx.clearRect(0, 0, w, h);

  // Background arc (dark)
  ctx.beginPath();
  ctx.arc(cx, cy, r, Math.PI, 0);
  ctx.lineWidth = 18;
  ctx.strokeStyle = '#1e3352';
  ctx.lineCap = 'round';
  ctx.stroke();

  // Value arc
  if (value > 0) {
    const minVal = 0, maxVal = 100;
    const clamped = Math.max(minVal, Math.min(value, maxVal));
    const pct = clamped / maxVal;
    const endAngle = Math.PI + (pct * Math.PI);

    let color = '#5DADE2'; // Blue fallback
    if (clamped < 30) color = '#F39C12'; // Yellow-ish warning (too dry)
    if (clamped > 65) color = '#E74C3C'; // Red warning (too humid)

    ctx.beginPath();
    ctx.arc(cx, cy, r, Math.PI, endAngle);
    ctx.lineWidth = 18;
    ctx.strokeStyle = color;
    ctx.lineCap = 'round';
    ctx.stroke();

    // Glow
    ctx.shadowBlur = 15;
    ctx.shadowColor = color;
    ctx.stroke();
    ctx.shadowBlur = 0;
  }
}

// ── Chart.js inicializace ───────────────────────────────────
function initChart() {
  const ctx = document.getElementById('chart-humidity').getContext('2d');

  // Gradient
  const grad = ctx.createLinearGradient(0, 0, 0, 190);
  grad.addColorStop(0, 'rgba(93, 173, 226, 0.4)');
  grad.addColorStop(1, 'rgba(93, 173, 226, 0.0)');

  humChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [], // Time strings
      datasets: [{
        label: 'Vlhkost (%)',
        data: [],
        borderColor: '#5DADE2',
        backgroundColor: grad,
        borderWidth: 2,
        pointRadius: 0,
        pointHitRadius: 10,
        fill: true,
        tension: 0.4 // Smooth curves
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { display: false },
        tooltip: {
          mode: 'index',
          intersect: false,
          backgroundColor: '#111f35',
          titleColor: '#e8edf5',
          bodyColor: '#5DADE2',
          borderColor: '#1e3352',
          borderWidth: 1
        }
      },
      scales: {
        x: {
          grid: { display: false, color: '#1e3352' },
          ticks: { color: '#6b8099', maxTicksLimit: 6 }
        },
        y: {
          min: 20, max: 80, // Default range, will scale if needed
          grid: { color: '#1e3352' },
          ticks: { color: '#6b8099' }
        }
      },
      animation: { duration: 0 } // Disable animation for live updates
    }
  });

  // Init gauge
  drawGauge(0);
}

// Přidání bodu do grafu
function updateChartData(val) {
  if (!humChart) return;
  const now = new Date();
  const timeStr = now.getHours().toString().padStart(2, '0') + ':' +
                  now.getMinutes().toString().padStart(2, '0') + ':' +
                  now.getSeconds().toString().padStart(2, '0');

  const data = humChart.data.datasets[0].data;
  const labels = humChart.data.labels;

  data.push(val);
  labels.push(timeStr);

  // Keep last 30 points (~5 mins at 10s intervals)
  if (data.length > 30) {
    data.shift();
    labels.shift();
  }

  // Adjust Y axis max if needed
  if (val > humChart.options.scales.y.max - 5) {
      humChart.options.scales.y.max = Math.ceil(val / 10) * 10 + 10;
  }
  if (val < humChart.options.scales.y.min + 5) {
      humChart.options.scales.y.min = Math.floor(val / 10) * 10 - 10;
  }

  humChart.update();
}

// ── Format helpers ──────────────────────────────────────────
function formatUptime(seconds) {
  const d = Math.floor(seconds / (3600*24));
  const h = Math.floor(seconds % (3600*24) / 3600);
  const m = Math.floor(seconds % 3600 / 60);
  if (d > 0) return `${d}d ${h}h`;
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m`;
}

// ── UI Updates (From MQTT) ──────────────────────────────────
let espOnline = false;

function updateModeUI(mode) {
  DOM.control.btnModes.forEach(b => b.classList.remove('active'));
  const btn = Array.from(DOM.control.btnModes).find(b => b.dataset.mode === mode);
  if(btn) btn.classList.add('active');
}

function handleMessage(topic, payload) {
  const str = payload.toString();
  DOM.diag.last.textContent = new Date().toLocaleTimeString();

  switch (topic) {
    case TOPICS.SUB.STATUS:
      espOnline = (str === 'online');
      DOM.header.statusDot.classList.toggle('online', espOnline);
      DOM.header.statusText.textContent = espOnline ? 'ESP32 Připojeno' : 'ESP32 Offline';
      DOM.diag.broker.textContent = espOnline ? 'Connected' : 'ESP LWT Offline';
      if(!espOnline) {
         DOM.sensors.humiditySub.textContent = 'Zařízení je offline. Poslední známá data.';
         DOM.sensors.temperatureSub.textContent = 'Offline';
      } else {
         DOM.sensors.humiditySub.textContent = 'Živá data';
         DOM.sensors.temperatureSub.textContent = '';
      }
      break;

    case TOPICS.SUB.HUMIDITY:
      const hum = parseFloat(str);
      DOM.sensors.humidityVal.innerHTML = `${hum.toFixed(1)}<span>%</span>`;
      drawGauge(hum);
      updateChartData(hum);
      break;

    case TOPICS.SUB.TEMPERATURE:
      DOM.sensors.temperatureVal.innerHTML = `${parseFloat(str).toFixed(1)}<span>°C</span>`;
      break;

    case TOPICS.SUB.TARGET:
      DOM.control.targetVal.innerHTML = `${Math.round(parseFloat(str))}<span>%</span>`;
      break;

    case TOPICS.SUB.POWER:
      // Status display update
      if (str === 'ON') {
        DOM.control.statusContainer.style.background = 'rgba(46, 204, 113, 0.15)';
        DOM.control.statusContainer.style.color = '#2ECC71';
        DOM.control.statusIcon.textContent = '▶';
        DOM.control.statusLabel.textContent = 'Zvlhčování aktivní';
        // Auto select ON if we were pushed to ON manually
        updateModeUI('ON');
      } else {
        DOM.control.statusContainer.style.background = 'var(--surface2)';
        DOM.control.statusContainer.style.color = 'var(--text-dim)';
        DOM.control.statusIcon.textContent = '⏸';
        DOM.control.statusLabel.textContent = 'Standby';
        // If it sends OFF, update UI
        updateModeUI('OFF');
      }
      break;

    case TOPICS.SUB.RSSI:
      DOM.diag.rssi.textContent = `${str} dBm`;
      break;
      
    case TOPICS.SUB.UPTIME:
      DOM.diag.uptime.textContent = formatUptime(parseInt(str, 10));
      break;
  }
}

// ── UI Actions (To MQTT) ────────────────────────────────────
function publishCmd(topic, payload) {
  if (!mqttClient || !mqttClient.connected) return;
  mqttClient.publish(topic, payload, { retain: false });
}

DOM.control.btnMinus.addEventListener('click', () => {
  let cur = parseInt(DOM.control.targetVal.textContent, 10);
  if (cur > 30) publishCmd(TOPICS.PUB.TARGET_SET, (cur - 1).toString());
});

DOM.control.btnPlus.addEventListener('click', () => {
  let cur = parseInt(DOM.control.targetVal.textContent, 10);
  if (cur < 80) publishCmd(TOPICS.PUB.TARGET_SET, (cur + 1).toString());
});

DOM.control.btnModes.forEach(btn => {
  btn.addEventListener('click', (e) => {
    const mode = e.target.dataset.mode;
    updateModeUI(mode);
    if(mode === 'AUTO') {
        // Zde by měla být podpora pre Auto, posíláme "AUTO" na switch topic (implementováno v Arduinu)
        publishCmd(TOPICS.PUB.POWER_SET, 'AUTO');
    } else {
        publishCmd(TOPICS.PUB.POWER_SET, mode);
    }
  });
});

// ── Login / MQTT Connection ─────────────────────────────────

function initMQTT(url, usr, pwd) {
  DOM.login.errorMsg.classList.add('hidden');
  DOM.login.btnConnect.textContent = 'Připojování...';

  const opts = {
    username: usr,
    password: pwd,
    keepalive: 60,
    clientId: 'webclient_' + Math.random().toString(16).substr(2, 8)
  };

  try {
    mqttClient = mqtt.connect(url, opts);

    mqttClient.on('connect', () => {
      // Hide login, show app
      DOM.login.overlay.classList.add('hidden');
      DOM.app.classList.remove('hidden');
      
      DOM.diag.broker.textContent = 'Connected (WSS)';
      DOM.header.statusDot.classList.remove('online'); // Wait for LWT
      DOM.header.statusText.textContent = 'Čekám na data...';

      // Subscribe to all relevant topics
      Object.values(TOPICS.SUB).forEach(t => mqttClient.subscribe(t));
      
      // Select AUTO mode initially on UI side
      updateModeUI('AUTO');
      publishCmd(TOPICS.PUB.POWER_SET, 'AUTO');
    });

    mqttClient.on('message', (topic, payload) => {
      handleMessage(topic, payload);
    });

    mqttClient.on('error', (err) => {
      console.error(err);
      DOM.login.errorMsg.textContent = 'Chyba připojení: Nespravné heslo nebo WSS port neprístupný.';
      DOM.login.errorMsg.classList.remove('hidden');
      DOM.login.btnConnect.textContent = 'Připojit';
      mqttClient.end();
    });

    mqttClient.on('close', () => {
      if(!DOM.app.classList.contains('hidden')) {
          DOM.header.statusDot.classList.remove('online');
          DOM.header.statusText.textContent = 'Broker odpojen';
          DOM.diag.broker.textContent = 'Disconnected';
      }
    });

  } catch (e) {
    DOM.login.errorMsg.textContent = 'Neplatná URL brokera.';
    DOM.login.errorMsg.classList.remove('hidden');
    DOM.login.btnConnect.textContent = 'Připojit';
  }
}

DOM.login.btnConnect.addEventListener('click', () => {
  const url = DOM.login.urlInput.value.trim() || DOM.login.urlInput.placeholder;
  const u = DOM.login.userInput.value.trim() || DOM.login.userInput.placeholder;
  const p = DOM.login.passInput.value.trim();
  if(!p) {
      DOM.login.errorMsg.textContent = 'Prosím zadejte heslo.';
      DOM.login.errorMsg.classList.remove('hidden');
      return;
  }
  
  // Store params temporarily (in real app, consider localStorage for url/user, NEVER pass info)
  localStorage.setItem('mqtt_url', url);
  localStorage.setItem('mqtt_user', u);
  
  initChart();
  initMQTT(url, u, p);
});

DOM.header.btnDisconnect.addEventListener('click', () => {
  if (mqttClient) mqttClient.end();
  DOM.app.classList.add('hidden');
  DOM.login.overlay.classList.remove('hidden');
  DOM.login.btnConnect.textContent = 'Připojit';
  DOM.login.passInput.value = '';
});

// Load saved config
window.addEventListener('DOMContentLoaded', () => {
  const savedUrl = localStorage.getItem('mqtt_url');
  const savedUsr = localStorage.getItem('mqtt_user');
  if(savedUrl) DOM.login.urlInput.value = savedUrl;
  if(savedUsr) DOM.login.userInput.value = savedUsr;
});
