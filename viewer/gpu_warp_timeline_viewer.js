const fileInput = document.getElementById("csvFile");
const smFilter = document.getElementById("smFilter");
const warpFilter = document.getElementById("warpFilter");
const eventFilter = document.getElementById("eventFilter");
const stallFilter = document.getElementById("stallFilter");
const summary = document.getElementById("summary");
const timeline = document.getElementById("timeline");

let rows = [];

function parseCsv(text) {
  const lines = text.trim().split(/\r?\n/);
  const header = lines.shift().split(",");
  return lines.filter(Boolean).map((line) => {
    const cells = [];
    let current = "";
    let quoted = false;
    for (let i = 0; i < line.length; i++) {
      const ch = line[i];
      if (ch === '"') quoted = !quoted;
      else if (ch === "," && !quoted) {
        cells.push(current);
        current = "";
      } else {
        current += ch;
      }
    }
    cells.push(current);
    const row = {};
    header.forEach((name, index) => row[name] = cells[index] || "");
    return row;
  });
}

function updateEvents() {
  const events = [...new Set(rows.map((row) => row.event).filter(Boolean))].sort();
  eventFilter.innerHTML = '<option value="">All events</option>' +
    events.map((event) => `<option value="${event}">${event}</option>`).join("");
}

function render() {
  const filtered = rows.filter((row) => {
    if (smFilter.value && row.sm !== smFilter.value.trim()) return false;
    if (warpFilter.value && row.warp !== warpFilter.value.trim()) return false;
    if (eventFilter.value && row.event !== eventFilter.value) return false;
    if (stallFilter.value && !row.stall_reason.includes(stallFilter.value.trim())) return false;
    return true;
  });
  const cycles = [...new Set(filtered.map((row) => Number(row.cycle)))].sort((a, b) => a - b);
  const warps = [...new Set(filtered.map((row) => `SM${row.sm}/W${row.warp}`))].sort();
  const byKey = new Map();
  for (const row of filtered) {
    const key = `SM${row.sm}/W${row.warp}:${row.cycle}`;
    if (!byKey.has(key)) byKey.set(key, []);
    byKey.get(key).push(row);
  }

  summary.textContent = `${filtered.length} events, ${warps.length} warp rows, ${cycles.length} cycles`;
  if (filtered.length === 0) {
    timeline.innerHTML = "";
    return;
  }

  let html = "<table><thead><tr><th>SM/Warp</th>";
  for (const cycle of cycles) html += `<th>${cycle}</th>`;
  html += "</tr></thead><tbody>";
  for (const warp of warps) {
    html += `<tr><td>${warp}</td>`;
    for (const cycle of cycles) {
      const events = byKey.get(`${warp}:${cycle}`) || [];
      html += "<td>" + events.map((row) =>
        `<span class="event ${row.event}" title="pc=${row.pc} mask=${row.active_mask} ${row.stall_reason}">${row.event}</span>`
      ).join("<br>") + "</td>";
    }
    html += "</tr>";
  }
  html += "</tbody></table>";
  timeline.innerHTML = html;
}

fileInput.addEventListener("change", async () => {
  const file = fileInput.files[0];
  if (!file) return;
  rows = parseCsv(await file.text());
  updateEvents();
  render();
});

for (const control of [smFilter, warpFilter, eventFilter, stallFilter]) {
  control.addEventListener("input", render);
  control.addEventListener("change", render);
}
