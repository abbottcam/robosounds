#pragma once
// The edit page served to your phone in edit mode.
// Plain HTML/CSS/JS with no external files (the board has no internet).

static const char WEB_PAGE[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>Lens Slate</title>
<style>
  :root { --bg:#f4f4f2; --card:#fff; --ink:#111; --muted:#777; --line:#ddd; --accent:#111; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#111; --card:#1c1c1c; --ink:#f2f2f2; --muted:#999; --line:#333; --accent:#f2f2f2; }
  }
  * { box-sizing:border-box; }
  body { margin:0; font:16px -apple-system, system-ui, sans-serif; background:var(--bg); color:var(--ink);
         padding:16px 16px 110px; }
  h1 { font-size:22px; margin:4px 0 16px; letter-spacing:.04em; }
  h2 { font-size:13px; letter-spacing:.12em; color:var(--muted); margin:0 0 10px; font-weight:600; }
  .card { background:var(--card); border-radius:14px; padding:14px; margin-bottom:16px; }
  input, textarea { width:100%; font:inherit; color:var(--ink); background:transparent;
                    border:1px solid var(--line); border-radius:8px; padding:9px 8px; min-width:0; }
  textarea { min-height:180px; resize:vertical; }
  .row { display:grid; gap:6px; align-items:center; margin-bottom:6px; }
  .lens { grid-template-columns: 1.2fr 1fr 1.2fr 1fr 34px; }
  .chk { display:flex; gap:8px; align-items:center; margin-top:12px; color:var(--muted); font-size:15px; }
  .chk input { width:auto; }
  .crew { grid-template-columns: 1fr 1.5fr 1.5fr 34px; }
  .hdr { font-size:12px; color:var(--muted); margin-bottom:4px; }
  .x { border:0; background:transparent; color:var(--muted); font-size:22px; width:34px; height:38px; padding:0; }
  .add { border:1px dashed var(--line); background:transparent; color:var(--ink); border-radius:8px;
         width:100%; padding:10px; font:inherit; margin-top:4px; }
  .add:disabled { opacity:.4; }
  .count { float:right; font-weight:400; }
  .bar { position:fixed; left:0; right:0; bottom:0; padding:12px 16px calc(12px + env(safe-area-inset-bottom));
         background:var(--bg); border-top:1px solid var(--line); display:flex; gap:12px; align-items:center; }
  .save { flex:1; background:var(--accent); color:var(--bg); border:0; border-radius:12px; padding:14px;
          font:600 17px -apple-system, system-ui, sans-serif; }
  #status { font-size:14px; color:var(--muted); min-width:90px; }
  .tip { font-size:13px; color:var(--muted); margin:6px 0 0; }
</style>
</head>
<body>
<h1>LENS SLATE</h1>

<div class="card">
  <h2>LENS SET NAME</h2>
  <input id="set" maxlength="40" placeholder="Zero Optiks Leica R">
  <label class="chk"><input type="checkbox" id="flip"> Flip screen upside down</label>
</div>

<div class="card">
  <h2>LENSES <span class="count" id="lensCount"></span></h2>
  <div class="row lens hdr"><span>Lens</span><span>T/S</span><span>C/F</span><span>&Oslash;</span><span></span></div>
  <div id="lenses"></div>
  <button class="add" id="addLens">+ Add lens</button>
  <button class="add" id="copyDia" type="button">Use the first &Oslash; for every lens</button>
  <p class="tip">If every lens has the same &Oslash; it's shown once, in one tall cell like a printed card.
  Leave &Oslash; empty to hide that column. Type 1/4, 1/2 or 3/4 and it becomes &frac14; &frac12; &frac34;.</p>
</div>

<div class="card">
  <h2>CREW <span class="count" id="crewCount"></span></h2>
  <div class="row crew hdr"><span>Role</span><span>Name</span><span>Phone</span><span></span></div>
  <div id="crew"></div>
  <button class="add" id="addCrew">+ Add crew member</button>
</div>

<div class="card">
  <h2>NOTES</h2>
  <textarea id="notes" maxlength="1500" placeholder="Anything you want on set..."></textarea>
</div>

<div class="bar">
  <span id="status">Loading...</span>
  <button class="save" id="save">Save to slate</button>
</div>

<script>
const MAX_LENSES = 12, MAX_CREW = 10;
const $ = id => document.getElementById(id);

// The screen font has plain ASCII plus Latin-1 (so ¼ ½ ¾ Ø é etc. work).
// iPhones type curly quotes, so turn those back into feet/inch marks.
function clean(s) {
  return (s || '')
    .replace(/[‘’′]/g, "'").replace(/[“”″]/g, '"')
    .replace(/[–—]/g, '-').replace(/…/g, '...')
    .replace(/[^\x20-\x7E\n -ÿ]/g, '');
}
function fractions(s) {
  return s.replace(/(^|[^\d])1\/4/g, '$1¼').replace(/(^|[^\d])1\/2/g, '$1½')
          .replace(/(^|[^\d])3\/4/g, '$1¾');
}

function field(value, max, placeholder, mode) {
  const i = document.createElement('input');
  i.value = value || ''; i.maxLength = max; i.placeholder = placeholder || '';
  if (mode) i.inputMode = mode;
  i.autocapitalize = 'off';
  return i;
}

function removeBtn(row) {
  const b = document.createElement('button');
  b.className = 'x'; b.textContent = '×'; b.type = 'button';
  b.onclick = () => { row.remove(); refreshCounts(); };
  return b;
}

function addLens(l = {}) {
  const r = document.createElement('div'); r.className = 'row lens';
  r.append(field(l.f, 12, '35mm'), field(l.t, 8, 'T1.8'),
           field(l.cf, 14, "1' 8\""), field(l.d, 10, '95mm'), removeBtn(r));
  $('lenses').append(r); refreshCounts();
}

function addCrew(c = {}) {
  const r = document.createElement('div'); r.className = 'row crew';
  const phone = field(c.phone, 24, '+00 000', 'tel'); phone.autocapitalize = 'off';
  const name = field(c.name, 40, 'Name'); name.autocapitalize = 'words';
  r.append(field(c.role, 16, '1st AC'), name, phone, removeBtn(r));
  $('crew').append(r); refreshCounts();
}

function refreshCounts() {
  const nl = $('lenses').children.length, nc = $('crew').children.length;
  $('lensCount').textContent = nl + ' / ' + MAX_LENSES;
  $('crewCount').textContent = nc + ' / ' + MAX_CREW;
  $('addLens').disabled = nl >= MAX_LENSES;
  $('addCrew').disabled = nc >= MAX_CREW;
}

function collect() {
  const vals = r => [...r.querySelectorAll('input')].map(i => clean(i.value).trim());
  return {
    set: clean($('set').value).trim(),
    flip: $('flip').checked,
    lenses: [...$('lenses').children].map(vals)
      .filter(v => v[0]).map(([f, t, cf, d]) => ({ f, t, cf: fractions(cf), d })),
    crew: [...$('crew').children].map(vals)
      .filter(v => v[0] || v[1]).map(([role, name, phone]) => ({ role, name, phone })),
    notes: clean($('notes').value).replace(/\r/g, '').trim(),
  };
}

$('addLens').onclick = () => addLens();
$('addCrew').onclick = () => addCrew();
$('copyDia').onclick = () => {
  const rows = [...$('lenses').children];
  if (!rows.length) return;
  const d = rows[0].querySelectorAll('input')[3].value;
  rows.forEach(r => r.querySelectorAll('input')[3].value = d);
};

$('save').onclick = async () => {
  $('save').disabled = true; $('status').textContent = 'Saving...';
  try {
    const res = await fetch('/save', { method: 'POST',
      headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(collect()) });
    if (!res.ok) throw new Error(await res.text());
    $('status').textContent = 'Saved ✓';
    $('save').textContent = 'Done - the slate is updating';
  } catch (e) {
    $('status').textContent = 'Failed';
    alert('Could not save: ' + e.message);
    $('save').disabled = false;
  }
};

fetch('/data').then(r => r.json()).then(d => {
  $('set').value = d.set || '';
  $('flip').checked = !!d.flip;
  (d.lenses || []).forEach(addLens);
  (d.crew || []).forEach(addCrew);
  $('notes').value = d.notes || '';
  refreshCounts();
  $('status').textContent = '';
}).catch(() => { $('status').textContent = 'Offline?'; refreshCounts(); });
</script>
</body>
</html>
)rawliteral";
