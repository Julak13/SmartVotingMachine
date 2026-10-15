#pragma once
// =====================================================================
//  web_assets.h  -  Admin dashboard (single page) stored in PROGMEM.
//  Served via server.send_P() so flashing firmware deploys the whole UI
//  with no separate filesystem upload. Dark, responsive, SaaS-style.
// =====================================================================
#include <Arduino.h>

static const char DASHBOARD_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<meta name="theme-color" content="#0b1020">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="apple-mobile-web-app-title" content="Voting">
<link rel="manifest" href="/manifest.json">
<link rel="apple-touch-icon" href="/icon.png">
<link rel="icon" type="image/png" href="/icon.png">
<title>Voting Machine | Admin</title>
<style>
:root{
  --bg:#0b1020; --bg2:#121a2e; --card:#161f38; --card2:#1d2945;
  --line:#26314f; --txt:#e7ecf6; --mut:#8a97b5; --acc:#5b8cff;
  --acc2:#22d3a6; --warn:#ffb020; --bad:#ff5d6c; --ok:#22d3a6;
  --r:18px; --sh:0 10px 30px rgba(0,0,0,.35);
}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',system-ui,-apple-system,Roboto,sans-serif;
  background:radial-gradient(1200px 600px at 80% -10%,#1a2546 0,var(--bg) 60%);
  color:var(--txt);min-height:100vh;-webkit-font-smoothing:antialiased}
.hidden{display:none!important}
button{font-family:inherit;cursor:pointer;border:none}
input,select{font-family:inherit}

/* ---------- login ---------- */
#login{min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
.login-card{background:var(--card);border:1px solid var(--line);border-radius:var(--r);
  padding:34px 28px;width:100%;max-width:380px;box-shadow:var(--sh);
  animation:rise .5s ease}
.brand{display:flex;align-items:center;gap:12px;margin-bottom:6px}
.logo{width:42px;height:42px;border-radius:12px;background:linear-gradient(135deg,var(--acc),var(--acc2));
  display:flex;align-items:center;justify-content:center;font-size:22px}
.brand h1{font-size:19px}
.sub{color:var(--mut);font-size:13px;margin:2px 0 22px}
label{display:block;font-size:12px;color:var(--mut);margin:14px 0 6px}
.fld{width:100%;background:var(--bg2);border:1px solid var(--line);color:var(--txt);
  padding:13px 14px;border-radius:12px;font-size:15px;outline:none;transition:.2s}
.fld:focus{border-color:var(--acc);box-shadow:0 0 0 3px rgba(91,140,255,.15)}
.btn{width:100%;padding:13px;border-radius:12px;background:linear-gradient(135deg,var(--acc),#3b6dff);
  color:#fff;font-weight:600;font-size:15px;margin-top:20px;transition:transform .12s,filter .2s}
.btn:hover{filter:brightness(1.08)} .btn:active{transform:scale(.98)}
.err{color:var(--bad);font-size:13px;margin-top:12px;min-height:16px}

/* ---------- app shell ---------- */
#app{max-width:1080px;margin:0 auto;padding:18px 16px 90px}
.top{display:flex;align-items:center;justify-content:space-between;margin:6px 2px 18px}
.top .brand h1{font-size:17px}
.pill{font-size:12px;padding:6px 12px;border-radius:999px;border:1px solid var(--line);
  background:var(--card);color:var(--mut)}
.pill.on{color:var(--ok);border-color:rgba(34,211,166,.4)}
.pill.off{color:var(--bad);border-color:rgba(255,93,108,.4)}
.logout{background:var(--card);border:1px solid var(--line);color:var(--mut);
  padding:8px 14px;border-radius:10px;font-size:13px}

/* ---------- tabs ---------- */
.tabs{position:fixed;bottom:0;left:0;right:0;display:flex;background:rgba(13,19,38,.92);
  backdrop-filter:blur(10px);border-top:1px solid var(--line);z-index:20}
.tabs button{flex:1;padding:11px 4px 13px;background:none;color:var(--mut);font-size:11px;
  display:flex;flex-direction:column;align-items:center;gap:3px;transition:.2s}
.tabs button .ic{font-size:19px}
.tabs button.active{color:var(--acc)}
.tabs button{min-width:0;white-space:nowrap;overflow:hidden}
@media(max-width:400px){.tabs button{font-size:10px;padding:10px 2px 12px}}
@media(min-width:760px){
  .tabs{position:static;border:1px solid var(--line);border-radius:14px;margin-bottom:18px;
    background:var(--card)}
  .tabs button{flex-direction:row;gap:8px;font-size:13px;padding:13px}
  #app{padding-bottom:30px}
}

/* ---------- cards ---------- */
.grid{display:grid;gap:14px}
.stats{grid-template-columns:repeat(2,1fr)}
@media(min-width:680px){.stats{grid-template-columns:repeat(4,1fr)}}
.card{background:var(--card);border:1px solid var(--line);border-radius:var(--r);
  padding:18px;box-shadow:var(--sh);animation:rise .4s ease}
.stat .k{color:var(--mut);font-size:12px;text-transform:uppercase;letter-spacing:.5px}
.stat .v{font-size:30px;font-weight:700;margin-top:8px}
.stat .v.acc{color:var(--acc)} .stat .v.ok{color:var(--ok)} .stat .v.warn{color:var(--warn)}
h2.sec{font-size:15px;margin:24px 4px 12px;color:var(--txt)}
.muted{color:var(--mut);font-size:13px}

/* list rows */
.row{display:flex;align-items:center;justify-content:space-between;gap:10px;
  padding:13px 14px;border:1px solid var(--line);border-radius:14px;background:var(--card2);
  margin-bottom:10px;animation:rise .3s ease}
.row .meta{font-size:12px;color:var(--mut);margin-top:3px}
.tag{font-size:11px;padding:3px 9px;border-radius:999px}
.tag.voted{background:rgba(34,211,166,.15);color:var(--ok)}
.tag.pend{background:rgba(255,176,32,.15);color:var(--warn)}
.iconbtn{background:var(--bg2);border:1px solid var(--line);color:var(--mut);
  width:36px;height:36px;border-radius:10px;font-size:15px;transition:.2s}
.iconbtn:hover{color:var(--bad);border-color:rgba(255,93,108,.4)}
.iconbtn.edit:hover{color:var(--acc);border-color:var(--acc)}

/* forms inline */
.inline{display:flex;gap:10px;flex-wrap:wrap}
.inline .fld{flex:1;min-width:160px}
.btn.sm{width:auto;padding:12px 20px;margin:0}
.btn.green{background:linear-gradient(135deg,var(--acc2),#16b894)}
.btn.red{background:linear-gradient(135deg,#ff6b78,#e0414f)}
.btn.ghost{background:var(--card2);border:1px solid var(--line);color:var(--txt)}

/* results bars */
.res{margin-bottom:16px}
.res .lbl{display:flex;justify-content:space-between;font-size:14px;margin-bottom:7px}
.res .lbl b{color:var(--acc)}
.bar{height:12px;border-radius:8px;background:var(--bg2);overflow:hidden}
.bar > span{display:block;height:100%;border-radius:8px;
  background:linear-gradient(90deg,var(--acc),var(--acc2));transition:width .6s ease}

/* modal */
.modal{position:fixed;inset:0;background:rgba(4,8,18,.7);display:flex;align-items:center;
  justify-content:center;padding:20px;z-index:40}
.modal .card{max-width:420px;width:100%}
.steps{margin:16px 0}
.stp{display:flex;gap:12px;align-items:flex-start;padding:9px 0;color:var(--mut)}
.stp .dot{width:22px;height:22px;border-radius:50%;border:2px solid var(--line);flex:none;
  display:flex;align-items:center;justify-content:center;font-size:12px}
.stp.done{color:var(--txt)} .stp.done .dot{background:var(--ok);border-color:var(--ok);color:#04221a}
.stp.cur{color:var(--txt)} .stp.cur .dot{border-color:var(--acc);color:var(--acc)}
.enroll-msg{text-align:center;padding:10px;border-radius:12px;background:var(--bg2);
  margin:6px 0 14px;font-size:14px}
.spinner{width:18px;height:18px;border:3px solid var(--line);border-top-color:var(--acc);
  border-radius:50%;animation:spin .8s linear infinite;display:inline-block;vertical-align:middle}
.toast{position:fixed;bottom:80px;left:50%;transform:translateX(-50%);background:var(--card2);
  border:1px solid var(--line);padding:12px 20px;border-radius:12px;box-shadow:var(--sh);
  z-index:60;animation:rise .3s ease;font-size:14px;max-width:90%}
@media(min-width:760px){.toast{bottom:24px}}

/* settings */
.btnrow{display:flex;gap:10px;flex-wrap:wrap;margin-top:16px}
.chk{display:flex;align-items:center;gap:10px;margin-top:14px;font-size:14px;color:var(--txt)}
.chk input{width:18px;height:18px;accent-color:var(--acc);flex:none}
.state{display:flex;align-items:flex-start;gap:9px;font-size:14px;margin-top:14px;line-height:1.45}
.dotc{width:10px;height:10px;border-radius:50%;background:var(--mut);flex:none;margin-top:5px}
.dotc.ok{background:var(--ok)} .dotc.warn{background:var(--warn)} .dotc.bad{background:var(--bad)}
.note{font-size:13px;color:var(--mut);margin-top:10px;line-height:1.55}
.netlist{margin-top:8px;max-height:230px;overflow:auto}
.net{display:flex;justify-content:space-between;gap:10px;padding:10px 12px;border:1px solid var(--line);
  border-radius:10px;background:var(--bg2);margin-bottom:6px;cursor:pointer;font-size:14px}
.net:hover{border-color:var(--acc)}
.net .s{color:var(--mut);font-size:12px;flex:none}
ol.how{margin:4px 0 0 20px;font-size:14px;line-height:1.75}
ol.how b{color:var(--acc)}
.kv{display:grid;grid-template-columns:auto 1fr;gap:8px 16px;font-size:14px}
.kv .k{color:var(--mut)}
.kv .bad{color:var(--bad)} .kv .good{color:var(--ok)} .kv .warn{color:var(--warn)}

@keyframes rise{from{opacity:0;transform:translateY(8px)}to{opacity:1;transform:none}}
@keyframes spin{to{transform:rotate(360deg)}}
</style>
</head>
<body>

<!-- ============ LOGIN ============ -->
<section id="login">
  <div class="login-card">
    <div class="brand"><div class="logo">&#x1F5F3;</div><h1>Voting Machine</h1></div>
    <div class="sub">Administrator dashboard &middot; offline</div>
    <label>Admin password</label>
    <input id="pw" class="fld" type="password" placeholder="Enter password"
      onkeydown="if(event.key==='Enter')doLogin()">
    <button class="btn" onclick="doLogin()">Sign in</button>
    <div id="loginErr" class="err"></div>
  </div>
</section>

<!-- ============ APP ============ -->
<main id="app" class="hidden">
  <div class="top">
    <div class="brand"><div class="logo">&#x1F5F3;</div><h1>Voting Machine</h1></div>
    <div style="display:flex;gap:10px;align-items:center">
      <span id="statusPill" class="pill">--</span>
      <button class="logout" onclick="logout()">Logout</button>
    </div>
  </div>

  <nav class="tabs">
    <button data-t="overview" class="active" onclick="tab('overview')"><span class="ic">&#x1F4CA;</span>Overview</button>
    <button data-t="voters"   onclick="tab('voters')"><span class="ic">&#x1F465;</span>Voters</button>
    <button data-t="cands"    onclick="tab('cands')"><span class="ic">&#x1F464;</span>Candidates</button>
    <button data-t="results"  onclick="tab('results')"><span class="ic">&#x1F4C8;</span>Results</button>
    <button data-t="control"  onclick="tab('control')"><span class="ic">&#x2699;</span>Control</button>
    <button data-t="settings" onclick="tab('settings')"><span class="ic">&#x1F527;</span>Settings</button>
  </nav>

  <!-- Overview -->
  <section id="v-overview">
    <div class="grid stats">
      <div class="card stat"><div class="k">Registered</div><div id="oReg" class="v acc">0</div></div>
      <div class="card stat"><div class="k">Votes Cast</div><div id="oCast" class="v ok">0</div></div>
      <div class="card stat"><div class="k">Remaining</div><div id="oRem" class="v warn">0</div></div>
      <div class="card stat"><div class="k">Candidates</div><div id="oCand" class="v">0</div></div>
    </div>
    <h2 class="sec">Election status</h2>
    <div class="card"><div id="oStatus" style="font-size:18px;font-weight:600">--</div>
      <div class="muted" id="oStatusHint" style="margin-top:6px"></div></div>
  </section>

  <!-- Voters -->
  <section id="v-voters" class="hidden">
    <h2 class="sec">Enroll a new voter</h2>
    <div class="card">
      <div class="inline">
        <input id="vName" class="fld" placeholder="Voter full name" maxlength="32">
        <button class="btn sm green" onclick="startEnroll()">+ Enroll</button>
      </div>
      <div class="muted" style="margin-top:10px">Enrollment captures the fingerprint and NFC card on the device. Follow the on-screen steps.</div>
    </div>
    <h2 class="sec">Registered voters (<span id="vCount">0</span>)</h2>
    <div id="voterList"></div>
  </section>

  <!-- Candidates -->
  <section id="v-cands" class="hidden">
    <h2 class="sec">Add candidate</h2>
    <div class="card">
      <div class="inline">
        <input id="cName" class="fld" placeholder="Candidate name" maxlength="32">
        <button class="btn sm" onclick="addCand()">+ Add</button>
      </div>
    </div>
    <h2 class="sec">Candidates (<span id="cCount">0</span>)</h2>
    <div id="candList"></div>
  </section>

  <!-- Results -->
  <section id="v-results" class="hidden">
    <h2 class="sec">Live results</h2>
    <div class="card"><div class="muted">Total votes</div>
      <div id="rTotal" style="font-size:30px;font-weight:700;color:var(--acc)">0</div></div>
    <h2 class="sec">Breakdown</h2>
    <div id="resList" class="card"></div>
    <h2 class="sec">Telegram</h2>
    <div class="card">
      <button class="btn sm green" onclick="tgSend()">&#x2708; Send results to Telegram</button>
      <div class="state"><span id="rTgDot" class="dotc"></span><span id="rTg">--</span></div>
    </div>
  </section>

  <!-- Control -->
  <section id="v-control" class="hidden">
    <h2 class="sec">Election control</h2>
    <div class="card">
      <div id="ctlStatus" style="font-size:16px;font-weight:600;margin-bottom:14px">--</div>
      <div class="inline">
        <button class="btn sm green" onclick="election('start')">&#x25B6; Start</button>
        <button class="btn sm ghost" onclick="election('stop')">&#x23F8; Stop</button>
        <button class="btn sm red" onclick="confirmReset()">&#x21BB; Reset</button>
      </div>
      <div class="muted" style="margin-top:12px">Start opens the polls. Stop disables voting. Reset clears all vote counts and voted flags (voters &amp; candidates are kept).</div>
    </div>
  </section>

  <!-- Settings -->
  <section id="v-settings" class="hidden">
    <h2 class="sec">Telegram results</h2>
    <div class="card">
      <div class="muted">Sends election results to a Telegram chat. The machine needs a Wi-Fi network with internet (set it below). Voting itself never needs the internet.</div>
      <label>Bot token</label>
      <input id="sTok" class="fld" placeholder="123456789:AAH..." autocomplete="off" spellcheck="false">
      <label>Chat ID</label>
      <div class="inline">
        <input id="sChat" class="fld" placeholder="Tap Detect after pressing START in your bot" autocomplete="off">
        <button class="btn sm ghost" onclick="detectChat()">Detect</button>
      </div>
      <div id="sDetect" class="note"></div>
      <label class="chk"><input type="checkbox" id="sAuto"> Send results automatically when the election is stopped</label>
      <label class="chk"><input type="checkbox" id="sStart"> Send a message when the polls open</label>
      <div class="btnrow">
        <button class="btn sm" onclick="saveTg()">Save</button>
        <button class="btn sm ghost" onclick="tgTest()">Send test message</button>
        <button class="btn sm green" onclick="tgSend()">Send results now</button>
      </div>
      <div class="state"><span id="tgDot" class="dotc"></span><span id="tgState">--</span></div>
    </div>

    <h2 class="sec">How to set up Telegram (one time)</h2>
    <div class="card">
      <ol class="how">
        <li>Open Telegram and search for <b>@BotFather</b>.</li>
        <li>Send <b>/newbot</b>, then choose a name and a username that ends in "bot".</li>
        <li>BotFather replies with a <b>token</b>. Paste it into <b>Bot token</b> above and tap <b>Save</b>.</li>
        <li>Open your new bot in Telegram and press <b>START</b>. (For a group: add the bot to the group and send any message there.)</li>
        <li>Tap <b>Detect</b>. The chat ID fills in by itself. Then tap <b>Send test message</b>.</li>
      </ol>
      <div class="note">Make sure the machine is connected to an internet Wi-Fi first (next section).</div>
    </div>

    <h2 class="sec">Internet Wi-Fi</h2>
    <div class="card">
      <div class="muted">The router the machine uses to reach Telegram. The VotingMachine network stays on for this dashboard.</div>
      <label>Network name</label>
      <div class="inline">
        <input id="sSsid" class="fld" placeholder="Wi-Fi name" autocomplete="off" spellcheck="false">
        <button class="btn sm ghost" onclick="scanWifi()">Scan</button>
      </div>
      <div id="netList" class="netlist"></div>
      <label>Password</label>
      <input id="sPass" class="fld" type="password" placeholder="Wi-Fi password" autocomplete="new-password">
      <div class="btnrow">
        <button class="btn sm" onclick="saveWifi()">Save &amp; connect</button>
        <button class="btn sm ghost" onclick="forgetWifi()">Disconnect</button>
      </div>
      <div class="state"><span id="wDot" class="dotc"></span><span id="wState">--</span></div>
      <div class="note">When the machine joins your router, your phone may drop off the VotingMachine Wi-Fi for a few seconds and reconnect. That is normal.</div>
    </div>

    <h2 class="sec">Use it like an app</h2>
    <div class="card">
      <div class="note" style="margin-top:0">Add this dashboard to your phone's home screen.<br>
        <b>Android (Chrome):</b> menu &#8942; &rarr; <i>Add to Home screen</i>.<br>
        <b>iPhone (Safari):</b> Share &rarr; <i>Add to Home Screen</i>.<br>
        Open it while your phone is connected to the <b>VotingMachine</b> Wi-Fi.</div>
    </div>

    <h2 class="sec">Device</h2>
    <div class="card"><div id="devInfo" class="kv"><span class="k">Loading...</span><span></span></div></div>
  </section>
</main>

<!-- Enrollment modal -->
<div id="enrollModal" class="modal hidden">
  <div class="card">
    <h2 style="font-size:17px;margin-bottom:4px">Enrolling: <span id="enName"></span></h2>
    <div class="muted">Have the voter follow these steps</div>
    <div class="enroll-msg"><span id="enSpin" class="spinner"></span> <span id="enMsg">Starting...</span></div>
    <div class="steps">
      <div class="stp" data-s="1"><div class="dot">1</div><div>Place finger on the sensor</div></div>
      <div class="stp" data-s="2"><div class="dot">2</div><div>Remove finger</div></div>
      <div class="stp" data-s="3"><div class="dot">3</div><div>Place the same finger again</div></div>
      <div class="stp" data-s="4"><div class="dot">4</div><div>Saving fingerprint template</div></div>
      <div class="stp" data-s="5"><div class="dot">5</div><div>Tap the NFC card</div></div>
    </div>
    <button id="enRetry" class="btn hidden" onclick="retryEnroll()">Try again</button>
    <button id="enClose" class="btn ghost hidden" onclick="closeEnroll()">Close</button>
    <button id="enCancel" class="btn red" onclick="cancelEnroll()">Cancel</button>
  </div>
</div>

<script>
// Browser storage can be blocked (private mode, some in-app browsers); the
// dashboard must still work - it just asks for the password each visit.
function lsGet(k){ try{ return localStorage.getItem(k); }catch(e){ return null; } }
function lsSet(k,v){ try{ localStorage.setItem(k,v); }catch(e){} }
function lsDel(k){ try{ localStorage.removeItem(k); }catch(e){} }
let TOKEN = lsGet('vm_token') || '';
let enrollTimer = null, refreshTimer = null;

function api(path, opts){
  opts = opts || {};
  const sep = path.includes('?') ? '&' : '?';
  return fetch(path + sep + 't=' + encodeURIComponent(TOKEN), opts)
    .then(r => { if(r.status===401){ logout(); throw new Error('unauth'); } return r; });
}
function jget(p){ return api(p).then(r=>r.json()); }
function jpost(p,body){ return api(p,{method:'POST',headers:{'Content-Type':'application/json'},
  body:JSON.stringify(body||{})}).then(r=>r.json()); }

function toast(m){ const t=document.createElement('div');t.className='toast';t.textContent=m;
  document.body.appendChild(t);setTimeout(()=>t.remove(),2200); }

/* ---- auth ---- */
function doLogin(){
  const pw=document.getElementById('pw').value;
  fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({password:pw})}).then(r=>r.json()).then(d=>{
    if(d.ok){ TOKEN=d.token; lsSet('vm_token',TOKEN); enterApp(); }
    else document.getElementById('loginErr').textContent='Incorrect password';
  }).catch(()=>document.getElementById('loginErr').textContent='Connection error');
}
function logout(){ TOKEN=''; lsDel('vm_token');
  clearInterval(refreshTimer);
  document.getElementById('app').classList.add('hidden');
  document.getElementById('login').classList.remove('hidden'); }

function enterApp(){
  document.getElementById('login').classList.add('hidden');
  document.getElementById('app').classList.remove('hidden');
  tab('overview'); refreshAll();
  refreshTimer=setInterval(refreshAll,3000);
}

/* ---- tabs ---- */
let curTab='overview';
function tab(name){
  curTab=name;
  document.querySelectorAll('.tabs button').forEach(b=>b.classList.toggle('active',b.dataset.t===name));
  ['overview','voters','cands','results','control','settings'].forEach(s=>
    document.getElementById('v-'+s).classList.toggle('hidden',s!==name));
  if(name==='settings') loadSettings();
  refreshAll();
}

/* ---- data refresh ---- */
function refreshAll(){
  jget('/api/overview').then(renderOverview).catch(()=>{});
  jget('/api/voters').then(renderVoters).catch(()=>{});
  jget('/api/candidates').then(renderCands).catch(()=>{});
  jget('/api/results').then(renderResults).catch(()=>{});
  if(curTab==='settings'||curTab==='results') refreshUplink();
  if(curTab==='settings') jget('/api/device').then(renderDevice).catch(()=>{});
}

function statusText(s){return s==='running'?'Polls Open':s==='stopped'?'Polls Closed':'Not Started';}
function renderOverview(d){
  document.getElementById('oReg').textContent=d.registered;
  document.getElementById('oCast').textContent=d.cast;
  document.getElementById('oRem').textContent=d.remaining;
  document.getElementById('oCand').textContent=d.candidates;
  const pill=document.getElementById('statusPill');
  pill.textContent=statusText(d.status);
  pill.className='pill '+(d.status==='running'?'on':'off');
  document.getElementById('oStatus').textContent=statusText(d.status);
  document.getElementById('oStatusHint').textContent=
    d.status==='running'?'Voters can authenticate and cast ballots.':
    d.status==='stopped'?'Voting is disabled.':'Open the polls from the Control tab.';
  document.getElementById('ctlStatus').textContent='Current status: '+statusText(d.status);
}
function renderVoters(d){
  document.getElementById('vCount').textContent=d.voters.length;
  const el=document.getElementById('voterList');
  el.innerHTML = d.voters.length? '' : '<div class="muted" style="padding:6px">No voters enrolled yet.</div>';
  d.voters.forEach(v=>{
    const row=document.createElement('div');row.className='row';
    row.innerHTML=`<div><div>${esc(v.name)}</div>
      <div class="meta">FID ${v.fid} &middot; UID ${esc(v.uid)}</div></div>
      <div style="display:flex;gap:10px;align-items:center">
      <span class="tag ${v.voted?'voted':'pend'}">${v.voted?'Voted':'Not voted'}</span>
      <button class="iconbtn" title="Delete" onclick="delVoter(${v.fid})">&#x1F5D1;</button></div>`;
    el.appendChild(row);
  });
}
function renderCands(d){
  document.getElementById('cCount').textContent=d.candidates.length;
  const el=document.getElementById('candList');
  el.innerHTML = d.candidates.length? '' : '<div class="muted" style="padding:6px">No candidates added yet.</div>';
  d.candidates.forEach(c=>{
    const row=document.createElement('div');row.className='row';
    row.innerHTML=`<div><div>${esc(c.name)}</div><div class="meta">ID ${c.id} &middot; ${c.votes} votes</div></div>
      <div style="display:flex;gap:8px">
      <button class="iconbtn edit" title="Edit" onclick="editCand(${c.id},'${esc(c.name)}')">&#x270E;</button>
      <button class="iconbtn" title="Delete" onclick="delCand(${c.id})">&#x1F5D1;</button></div>`;
    el.appendChild(row);
  });
}
function renderResults(d){
  document.getElementById('rTotal').textContent=d.total;
  const el=document.getElementById('resList');
  if(!d.candidates.length){el.innerHTML='<div class="muted">No candidates.</div>';return;}
  el.innerHTML='';
  d.candidates.forEach(c=>{
    const pct = d.total? Math.round(c.votes*1000/d.total)/10 : 0;
    const w=document.createElement('div');w.className='res';
    w.innerHTML=`<div class="lbl"><span>${esc(c.name)}</span><b>${c.votes} &middot; ${pct}%</b></div>
      <div class="bar"><span style="width:${pct}%"></span></div>`;
    el.appendChild(w);
  });
}
function esc(s){return String(s).replace(/[&<>"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[m]));}

/* ---- voter mgmt ---- */
function delVoter(fid){ if(!confirm('Delete this voter?'))return;
  jpost('/api/voters/delete',{fid}).then(d=>{toast(d.ok?'Voter deleted':d.error||'Failed');refreshAll();}); }

/* ---- candidate mgmt ---- */
function addCand(){ const n=document.getElementById('cName').value.trim(); if(!n)return;
  jpost('/api/candidates',{name:n}).then(d=>{toast(d.ok?'Candidate added':d.error||'Failed');
    document.getElementById('cName').value='';refreshAll();}); }
function editCand(id,cur){ const n=prompt('Edit candidate name:',cur); if(n===null)return;
  jpost('/api/candidates/edit',{id,name:n.trim()}).then(d=>{toast(d.ok?'Updated':d.error||'Failed');refreshAll();}); }
function delCand(id){ if(!confirm('Delete this candidate?'))return;
  jpost('/api/candidates/delete',{id}).then(d=>{toast(d.ok?'Deleted':d.error||'Failed');refreshAll();}); }

/* ---- election control ---- */
function election(a){ jpost('/api/election/'+a).then(d=>{toast(d.ok?('Election '+a):d.error||'Failed');refreshAll();}); }
function confirmReset(){ if(confirm('Reset election? All vote counts and voted flags will be cleared.')) election('reset'); }

/* ---- enrollment ---- */
let lastEnrollName='';
function startEnroll(){
  const name=document.getElementById('vName').value.trim();
  if(!name){toast('Enter a name first');return;}
  beginEnroll(name);
}
function retryEnroll(){ if(lastEnrollName) beginEnroll(lastEnrollName); }
function beginEnroll(name){
  jpost('/api/voters/enroll',{name}).then(d=>{
    if(!d.ok){toast(d.error||'Failed');return;}
    lastEnrollName=name;
    document.getElementById('vName').value='';
    document.getElementById('enName').textContent=name;
    document.getElementById('enMsg').textContent='Starting...';
    document.querySelectorAll('.stp').forEach(s=>s.classList.remove('done','cur'));
    document.getElementById('enRetry').classList.add('hidden');
    document.getElementById('enClose').classList.add('hidden');
    document.getElementById('enCancel').classList.remove('hidden');
    document.getElementById('enSpin').classList.remove('hidden');
    document.getElementById('enrollModal').classList.remove('hidden');
    pollEnroll();
  });
}
function pollEnroll(){
  clearInterval(enrollTimer);
  enrollTimer=setInterval(()=>{
    jget('/api/voters/enroll/status').then(s=>{
      document.getElementById('enMsg').textContent=s.message||'';
      for(let i=1;i<=5;i++){
        const st=document.querySelector('.stp[data-s="'+i+'"]');
        st.classList.toggle('done', s.step>i || (s.done&&s.success));
        st.classList.toggle('cur', s.step===i && !s.done);
      }
      if(s.done){
        clearInterval(enrollTimer);
        document.getElementById('enSpin').classList.add('hidden');
        document.getElementById('enClose').classList.remove('hidden');
        document.getElementById('enCancel').classList.add('hidden');
        document.getElementById('enRetry').classList.toggle('hidden', s.success);
        toast(s.success?'Voter enrolled':'Enrollment failed');
        refreshAll();
      }
    }).catch(()=>{});
  },800);
}
function cancelEnroll(){ jpost('/api/voters/enroll/cancel'); }
function closeEnroll(){ clearInterval(enrollTimer);
  document.getElementById('enrollModal').classList.add('hidden'); }

/* ---- settings: Telegram + internet Wi-Fi ---- */
const $=id=>document.getElementById(id);
let scanPending=false, detectPending=false;

function loadSettings(){
  jget('/api/settings').then(s=>{
    $('sTok').value='';
    $('sTok').placeholder = s.tgTokenSet ? ('Saved ('+(s.tgTokenHint||'hidden')+') - leave blank to keep') : '123456789:AAH...';
    $('sChat').value = s.tgChat||'';
    $('sAuto').checked = !!s.autoFinal;
    $('sStart').checked = !!s.notifyStart;
    $('sSsid').value = s.wifiSsid||'';
    $('sPass').value='';
    $('sPass').placeholder = s.wifiPassSet ? 'Saved - leave blank to keep' : 'Wi-Fi password';
  }).catch(()=>{});
}
function saveTg(){
  const body={tgChat:$('sChat').value.trim(), autoFinal:$('sAuto').checked, notifyStart:$('sStart').checked};
  const tok=$('sTok').value.trim(); if(tok) body.tgToken=tok;
  return jpost('/api/settings',body).then(d=>{
    toast(d.ok?'Telegram settings saved':(d.error||'Failed'));
    if(d.ok) loadSettings();
    return d.ok;
  });
}
function detectChat(){
  // Save a freshly pasted token first, so Detect uses it.
  const pre = $('sTok').value.trim() ? saveTg() : Promise.resolve(true);
  pre.then(ok=>{
    if(!ok) return;
    jpost('/api/telegram/detect').then(d=>{
      if(!d.ok){toast(d.error||'Failed');return;}
      detectPending=true; $('sDetect').textContent='Looking for your chat...';
      setTimeout(refreshUplink,1500); setTimeout(refreshUplink,5000);
    });
  });
}
function tgTest(){ jpost('/api/telegram/test').then(d=>{
  toast(d.ok?'Sending test message...':(d.error||'Failed'));
  if(d.ok){ setTimeout(refreshUplink,1500); setTimeout(refreshUplink,6000); } }); }
function tgSend(){ jpost('/api/telegram/send').then(d=>{
  toast(d.ok?'Sending results...':(d.error||'Failed'));
  if(d.ok){ setTimeout(refreshUplink,1500); setTimeout(refreshUplink,6000); } }); }

function saveWifi(){
  const ssid=$('sSsid').value.trim();
  if(!ssid){toast('Enter or scan a Wi-Fi name');return;}
  jpost('/api/settings',{wifiSsid:ssid, wifiPass:$('sPass').value}).then(d=>{
    toast(d.ok?'Saved - connecting...':(d.error||'Failed'));
    if(d.ok){ loadSettings(); setTimeout(refreshUplink,2000); }
  });
}
function forgetWifi(){
  if(!confirm('Disconnect from the internet Wi-Fi? Telegram will stop working until you set it again.'))return;
  jpost('/api/settings',{wifiSsid:''}).then(d=>{
    toast(d.ok?'Disconnected':(d.error||'Failed')); if(d.ok){ loadSettings(); refreshUplink(); } });
}
function scanWifi(){
  jpost('/api/wifi/scan').then(d=>{
    if(!d.ok){toast(d.error||'Failed');return;}
    scanPending=true; $('netList').innerHTML='<div class="note">Scanning (a few seconds)...</div>';
    setTimeout(refreshUplink,3000); setTimeout(refreshUplink,6000);
  });
}
function pickNet(i){ const n=lastNets[i]; if(!n)return; $('sSsid').value=n.ssid;
  $('netList').innerHTML=''; $('sPass').value=''; $('sPass').focus(); }

let lastNets=[];
function sig(r){ return r>-60?'strong':r>-70?'good':r>-80?'fair':'weak'; }
function ago(s){ return s<60? s+'s ago' : s<3600? Math.floor(s/60)+' min ago' : Math.floor(s/3600)+' h ago'; }
function dot(id,cls){ $(id).className='dotc'+(cls?' '+cls:''); }

function refreshUplink(){ jget('/api/uplink/status').then(renderUplink).catch(()=>{}); }
function renderUplink(s){
  const w=s.wifi, t=s.telegram;
  let wt, wc='';
  if(w.state==='online'){ wt='Connected to '+w.ssid+' ('+w.ip+', '+sig(w.rssi)+' signal)'; wc='ok'; }
  else if(w.state==='connecting'){ wt='Connecting to '+w.ssid+'...'; wc='warn'; }
  else if(w.state==='failed'){ wt='Could not connect to '+w.ssid+': '+w.reason+'. Retrying in '+Math.ceil((w.retryIn||0)/60)+' min.'; wc='bad'; }
  else { wt='Not set up - the machine has no internet connection.'; }
  $('wState').textContent=wt; dot('wDot',wc);

  let tt, tc='';
  if(!t.ready){ tt='Not set up yet - see the steps below.'; }
  else if(t.busy){ tt='Sending...'; tc='warn'; }
  else if(t.lastAgo<0){ tt='Ready. Nothing sent yet.'; tc='ok'; }
  else { tt=t.lastMsg+' ('+ago(t.lastAgo)+')'; tc=t.lastOk?'ok':'bad'; }
  if(t.pendingFinal) tt+=' Final results are queued and will be retried.';
  $('tgState').textContent=tt; dot('tgDot',tc);
  $('rTg').textContent=t.ready? tt : 'Telegram is not set up. Go to Settings.'; dot('rTgDot',t.ready?tc:'');

  if(t.detectMsg){
    $('sDetect').textContent=t.detectMsg;
    if(detectPending && t.detectMsg.indexOf('Looking')!==0){
      detectPending=false;
      if(t.detectMsg.indexOf('Found')===0) jget('/api/settings').then(x=>{ $('sChat').value=x.tgChat||''; });
    }
  }
  if(scanPending && !s.scanBusy){
    scanPending=false; lastNets=s.networks||[];
    $('netList').innerHTML = lastNets.length ? '' : '<div class="note">No networks found. Move closer to the router and scan again.</div>';
    lastNets.forEach((n,i)=>{
      const r=document.createElement('div'); r.className='net';
      r.innerHTML='<span>'+esc(n.ssid)+(n.open?' (open)':'')+'</span><span class="s">'+sig(n.rssi)+'</span>';
      r.onclick=()=>pickNet(i); $('netList').appendChild(r);
    });
  }
}

function renderDevice(d){
  const up=d.uptime, hrs=Math.floor(up/3600), mins=Math.floor((up%3600)/60);
  const bad = /Brownout|Crash|Watchdog/.test(d.reset);
  const rows=[
    ['Firmware','v'+esc(d.fw)],
    ['Running for', hrs+' h '+mins+' min'],
    ['Last restart','<span class="'+(bad?'bad':'')+'">'+esc(d.reset)+'</span>'],
    ['Card reader', d.nfc?'<span class="good">OK</span>'+(d.nfcRecoveries?' (auto-recovered '+d.nfcRecoveries+'x)':''):'<span class="bad">Not responding</span>'],
    ['Fingerprint sensor', d.fp?'<span class="good">OK</span>':'<span class="bad">Not responding</span>'],
    ['Free memory', Math.round(d.heap/1024)+' KB (lowest '+Math.round(d.minHeap/1024)+' KB)'],
    ['Phones connected', d.apClients]
  ];
  $('devInfo').innerHTML=rows.map(r=>'<span class="k">'+r[0]+'</span><span>'+r[1]+'</span>').join('');
}

/* ---- boot ---- */
if(TOKEN){ jget('/api/overview').then(()=>enterApp()).catch(()=>logout()); }
</script>
</body>
</html>)HTML";
