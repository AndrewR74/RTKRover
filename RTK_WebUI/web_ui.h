const char WEB_UI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>RTK Rover</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <link href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" rel="stylesheet"/>

  <style>
    /* ===== Base ===== */
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: #f5f7fa;
      color: #212529;
    }
    h2,h6 { margin: 0; }
    h6 { font-size: 0.9rem; color: #6c757d; }
    .mono { font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; }

    /* ===== Layout ===== */
    .container {
      max-width: 960px;
      margin: 0 auto;
      padding: 16px;
    }
    .row {
      display: flex;
      flex-wrap: wrap;
    }
    .col-12 { width: 100%; }
    .col-4  { width: 33.333%; }
    .col-6  { width: 50%; }
    .col-8  { width: 66.666%; }

    @media (min-width: 768px) {
      .col-md-4  { width: 33.333%; }
      .col-md-6  { width: 50%; }
      .col-md-8  { width: 66.666%; }
    }

    .g-2 { gap: 8px; }
    .g-3 { gap: 12px; }
    .mt-1 { margin-top: 4px; }
    .mt-2 { margin-top: 8px; }
    .mt-3 { margin-top: 16px; }
    .mt-4 { margin-top: 24px; }
    .mb-2 { margin-bottom: 8px; }
    .mb-3 { margin-bottom: 16px; }
    .my-4 { margin-top: 24px; margin-bottom: 24px; }

    .d-flex { display: flex; }
    .align-items-center { align-items: center; }
    .justify-content-between { justify-content: space-between; }
    .text-center { text-align: center; }
    .text-muted { color: #6c757d; }
    .small { font-size: 0.8rem; }
    .w-100 { width: 100%; }

    /* ===== Cards ===== */
    .card {
      background: #fff;
      border-radius: 12px;
      box-shadow: 0 1px 3px rgba(0,0,0,0.08);
      overflow: hidden;
    }
    .card-header {
      padding: 10px 14px;
      font-weight: 600;
      background: #f1f3f5;
      border-bottom: 1px solid #e5e7ea;
    }
    .card-body {
      padding: 14px;
    }

    /* ===== Badges ===== */
    .badge {
      display: inline-block;
      padding: 4px 10px;
      border-radius: 999px;
      font-size: 0.75rem;
      font-weight: 600;
      color: #fff;
    }
    .text-bg-secondary { background:#6c757d; }
    .text-bg-success   { background:#28a745; }
    .text-bg-warning   { background:#ffc107; color:#212529; }
    .text-bg-primary   { background:#0d6efd; }
    .text-bg-info      { background:#0dcaf0; }

    /* ===== Forms ===== */
    label { display: block; font-size: 0.75rem; margin-bottom: 2px; }
    input, select {
      width: 100%;
      padding: 8px;
      border-radius: 6px;
      border: 1px solid #ced4da;
      font-size: 0.9rem;
    }
    input:focus, select:focus {
      outline: none;
      border-color: #0d6efd;
    }

    /* ===== Buttons ===== */
    .btn {
      padding: 8px 12px;
      border-radius: 6px;
      border: 1px solid transparent;
      cursor: pointer;
      font-size: 0.9rem;
    }
    .btn-primary {
      background: #0d6efd;
      color: #fff;
    }
    .btn-outline-primary {
      background: transparent;
      border-color: #0d6efd;
      color: #0d6efd;
    }
    .btn-outline-secondary {
      background: transparent;
      border-color: #6c757d;
      color: #6c757d;
    }
    .btn-outline-danger {
      background: transparent;
      border-color: #dc3545;
      color: #dc3545;
    }
    .btn:hover { opacity: 0.9; }

    /* ===== Alerts ===== */
    .alert {
      margin: 12px;
      padding: 10px;
      border-radius: 6px;
    }
    .alert-success {
      background: #d4edda;
      color: #155724;
    }
    .alert-danger {
      background: #f8d7da;
      color: #721c24;
    }

    /* ===== Switch ===== */
    .form-check {
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .form-check-input {
      width: 40px;
      height: 20px;
    }
    .form-check-label {
      font-size: 0.85rem;
    }

    /* ===== Map / Sky ===== */
    #map { height: 360px; border-radius: 12px; }
    #skyplot { background:#111; border-radius:50%; }
  </style>

</head>
<body>
<div class="container my-4">

  <div class="d-flex align-items-center justify-content-between mb-3">
    <h2 class="m-0">RTK Rover</h2>
    <span id="fixBadge" class="badge text-bg-secondary">NO FIX</span>
  </div>

  <div class="card mb-3">
    <div class="card-header fw-bold">Live Status</div>
    <div class="card-body">
      <div id="status">Loading…</div>

      <div class="row mt-3 g-3">
        <div class="col-12 col-md-6 text-center">
          <h6 class="mb-2">Compass</h6>
          <div id="compass" style="width:150px;height:150px;border:3px solid #555;border-radius:50%;margin:auto;position:relative;background:white;">
            <div id="needle" style="width:4px;height:65px;background:red;position:absolute;top:10px;left:50%;transform-origin:50% 90%;"></div>
            <div style="position:absolute;top:6px;left:50%;transform:translateX(-50%);font-size:12px;">N</div>
            <div style="position:absolute;bottom:6px;left:50%;transform:translateX(-50%);font-size:12px;">S</div>
            <div style="position:absolute;left:6px;top:50%;transform:translateY(-50%);font-size:12px;">W</div>
            <div style="position:absolute;right:6px;top:50%;transform:translateY(-50%);font-size:12px;">E</div>
          </div>
          <div class="mt-2 mono"><span id="headingVal">---</span>°</div>
        </div>

        <div class="col-12 col-md-6 text-center">
          <h6 class="mb-2">Sky Plot</h6>
          <canvas id="skyplot" width="170" height="170"></canvas>
          <div class="small text-muted mt-2">Green = used in fix, Orange = tracked</div>
        </div>
      </div>

      <div class="form-check form-switch mt-3">
        <input class="form-check-input" type="checkbox" id="autoCenter" checked>
        <label class="form-check-label" for="autoCenter">Auto-center map</label>
      </div>

      <div id="map" class="mt-3" style="display: none"></div>
    </div>
  </div>

  <div class="card mb-3">
    <div class="card-header fw-bold">Configuration</div>
    <div class="card-body">
      <form id="cfgForm">

        <h6 class="text-muted mt-1">Wi-Fi</h6>
        <div class="row g-2 align-items-end">
          <div class="col-12 col-md-8">
            <label class="form-label">SSID</label>
            <select name="ssid" id="ssidSel" class="form-select"></select>
          </div>
          <div class="col-12 col-md-4">
            <button type="button" class="btn btn-outline-primary w-100" onclick="scan()">Rescan</button>
          </div>
        </div>

        <div class="mt-2">
          <label class="form-label">Wi-Fi Password</label>
          <input name="wpass" type="text" class="form-control" placeholder="(leave blank if open)">
        </div>

        <h6 class="text-muted mt-4">NTRIP</h6>
        <div class="mt-2">
          <label class="form-label">Caster Host</label>
          <input name="nhost" class="form-control" placeholder="rtn.dot.ny.gov">
        </div>

        <div class="row g-2 mt-1">
          <div class="col-4">
            <label class="form-label">Port</label>
            <input name="nport" class="form-control" placeholder="8080">
          </div>
          <div class="col-8">
            <label class="form-label">Mountpoint</label>
            <input name="nmount" class="form-control" placeholder="net_msm_vrs">
          </div>
        </div>

        <div class="mt-2">
          <label class="form-label">Username</label>
          <input name="nuser" class="form-control">
        </div>
        <div class="mt-2">
          <label class="form-label">Password</label>
          <input name="npass" type="text" class="form-control">
        </div>

        <button class="btn btn-primary w-100 mt-3">Save & Reboot</button>
      </form>

      <div class="d-grid gap-2 mt-3">
        <button class="btn btn-outline-secondary" onclick="fetch('/reboot',{method:'POST'})">Reboot</button>
        <button class="btn btn-outline-danger" onclick="fetch('/reset',{method:'POST'}).then(()=>alert('Reset; device rebooting'))">Factory Reset</button>
      </div>
    </div>
  </div>

</div>

<div id="saveAlert" style="display:none;" class="alert"></div>


<script>
let leafletLoaded = false;
</script>

<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"
        defer
        onload="initMap();">
</script>

<script>
document.getElementById('cfgForm').addEventListener('submit', function(e) {
  e.preventDefault(); // stop page navigation

  const form = e.target;
  const data = new URLSearchParams(new FormData(form));

  fetch('/save', {
    method: 'POST',
    body: data
  })
  .then(r => r.text())
  .then(txt => {
    showSaveAlert(true, "Configuration saved successfully.");

    // Optional: tell device to retry Wi-Fi
    fetch('/wifi/retry').catch(()=>{});
  })
  .catch(err => {
    showSaveAlert(false, "Failed to save configuration.");
    console.error(err);
  });
});

function showSaveAlert(success, msg) {
  const a = document.getElementById('saveAlert');
  a.style.display = 'block';
  a.innerText = msg;

  // Bootstrap-aware styling (falls back gracefully)
  a.className = success
    ? 'alert alert-success'
    : 'alert alert-danger';

  // Auto-hide after 5 seconds
  setTimeout(() => a.style.display = 'none', 5000);
}
</script>


<script>
let map, marker, circle, pathLine;
let track = [];
const sky = document.getElementById("skyplot");
const ctx = sky.getContext("2d");

function badgeForFix(fix){
  const b = document.getElementById("fixBadge");
  b.textContent = fix || "NO FIX";
  b.className = "badge " + (
    (fix||"").includes("RTK FIX") ? "text-bg-success" :
    (fix||"").includes("FLOAT")   ? "text-bg-warning" :
    (fix||"").includes("GPS")     ? "text-bg-primary" :
    (fix||"").includes("DGPS")    ? "text-bg-info" :
                                    "text-bg-secondary"
  );
}

function fixColor(fix){
  if ((fix||"").includes("RTK FIX")) return "#28a745";
  if ((fix||"").includes("FLOAT"))   return "#ffc107";
  if ((fix||"").includes("GPS"))     return "#0d6efd";
  if ((fix||"").includes("DGPS"))    return "#0dcaf0";
  return "#dc3545";
}

function initMap() {

  if (!window.L) {
    console.log("Leaflet not available — disabling map");
    
    return;
  }

  document.getElementById("map").style.display = null;

  map = L.map('map').setView([41.1176, -74.0075], 18);
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 20, attribution: '&copy; OpenStreetMap'
  }).addTo(map);

  marker = L.circleMarker([0,0], {radius:8, color:"#000", fillColor:"#dc3545", fillOpacity:1}).addTo(map);
  circle = L.circle([0,0], {radius:3, color:"#dc3545", fillColor:"#dc3545", fillOpacity:0.12}).addTo(map);

  pathLine = L.polyline(track, {weight:3, color:"#00ffff", opacity:0.9}).addTo(map);
}

function updateCompass(heading, fix){
  if (heading === null || isNaN(heading)) return;
  document.getElementById("needle").style.transform = `rotate(${heading}deg)`;
  document.getElementById("needle").style.background = fixColor(fix);
  document.getElementById("headingVal").textContent = heading.toFixed(1);
}

function drawSkyPlot(sats){
  ctx.clearRect(0,0,170,170);

  // rings + crosshair
  ctx.strokeStyle = "#444";
  ctx.beginPath();
  ctx.arc(85,85,80,0,Math.PI*2);
  ctx.arc(85,85,55,0,Math.PI*2);
  ctx.arc(85,85,30,0,Math.PI*2);
  ctx.stroke();
  ctx.beginPath();
  ctx.moveTo(85,5); ctx.lineTo(85,165);
  ctx.moveTo(5,85); ctx.lineTo(165,85);
  ctx.stroke();

  if (!sats || !Array.isArray(sats)) return;

  sats.forEach(s=>{
    const el = Number(s.el);
    const az = Number(s.az);
    const id = Number(s.id);
    const used = !!s.used;

    if (isNaN(el) || isNaN(az) || el < 0 || az < 0) return;

    const r = (90 - el) / 90 * 80;
    const rad = (az - 90) * Math.PI / 180;

    const x = 85 + r * Math.cos(rad);
    const y = 85 + r * Math.sin(rad);

    ctx.fillStyle = used ? "#00ff00" : "#ffaa00";
    ctx.beginPath();
    ctx.arc(x,y,4,0,Math.PI*2);
    ctx.fill();

    ctx.fillStyle = "#ddd";
    ctx.font = "10px monospace";
    ctx.fillText(id, x+6, y+4);
  });
}

function scan(){
  fetch('/scan').then(r=>r.json()).then(arr=>{
    const sel = document.getElementById('ssidSel');
    sel.innerHTML = "";
    arr.forEach(s=>{
      const o = document.createElement('option');
      o.value = o.text = s;
      sel.appendChild(o);
    });
  });
}

function loadConfig(){
  fetch('/config').then(r=>r.json()).then(c=>{
    // set defaults in form if available
    if (c.ssid) {
      // select after scan() populates options
      const trySelect = () => {
        const sel = document.getElementById('ssidSel');
        for (let i=0;i<sel.options.length;i++){
          if (sel.options[i].value === c.ssid) { sel.selectedIndex = i; break; }
        }
      };
      setTimeout(trySelect, 350);
    }

    document.querySelector('input[name="wpass"]').value = c.wpass || "";

    document.querySelector('input[name="nhost"]').value  = c.nhost || "";
    document.querySelector('input[name="nport"]').value  = c.nport || "";
    document.querySelector('input[name="nmount"]').value = c.nmount || "";
    document.querySelector('input[name="nuser"]').value  = c.nuser || "";
    document.querySelector('input[name="npass"]').value  = c.npass  || "";
  });
}

function poll(){
  fetch('/status').then(r=>r.json()).then(j=>{
    badgeForFix(j.fix);

    document.getElementById('status').innerHTML = `
      <div class="row g-2">
        <div class="col-12 col-md-6">
          <div><b>Fix:</b> ${j.fix}</div>
          <div><b>Sats:</b> ${j.sats} &nbsp; <b>HDOP:</b> ${j.hdop}</div>
          <div><b>Speed:</b> ${j.speed_kn} kn &nbsp; <b>Heading:</b> ${j.heading_deg}°</div>
          <div><b>RTCM:</b> ${j.rtcmtime}s ago</div>
        </div>
        <div class="col-12 col-md-6 mono">
          <div><b>Lat:</b> ${j.lat}</div>
          <div><b>Lon:</b> ${j.lon}</div>
          <div><b>Wi-Fi:</b> ${j.wifi} (${j.rssi} dBm)</div>
          <div><b>IP:</b> ${j.ip}</div>
          <div><b>NTRIP:</b> ${j.ntrip}</div>
        </div>
      </div>
    `;

    // map updates
    if (map && j.lat && j.lon) {
      const lat = Number(j.lat), lon = Number(j.lon);
      if (!isNaN(lat) && !isNaN(lon) && (lat !== 0 || lon !== 0)) {
        marker.setLatLng([lat, lon]);
        marker.setStyle({ fillColor: fixColor(j.fix), color:"#000" });

        const acc = Math.max(0.8, Number(j.hdop) * 1.5); // simple visualization heuristic
        circle.setLatLng([lat, lon]);
        circle.setRadius(acc);
        circle.setStyle({ color: fixColor(j.fix), fillColor: fixColor(j.fix) });

        track.push([lat, lon]);
        if (track.length > 500) track.shift();
        pathLine.setLatLngs(track);

        if (document.getElementById("autoCenter").checked) {
          map.setView([lat, lon], map.getZoom(), { animate: false });
        }
      }
    }

    updateCompass(Number(j.heading_deg), j.fix);
    drawSkyPlot(j.sats_detail);
  }).catch(()=>{});
}

scan();
loadConfig();
setInterval(poll, 1000);
</script>

</body>
</html>
)rawliteral";