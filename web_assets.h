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
    <button id="enClose" class="btn ghost hidden" onclick="closeEnroll()">Close</button>
    <button id="enCancel" class="btn red" onclick="cancelEnroll()">Cancel</button>
  </div>
</div>

<script>
let TOKEN = localStorage.getItem('vm_token') || '';
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
    if(d.ok){ TOKEN=d.token; localStorage.setItem('vm_token',TOKEN); enterApp(); }
    else document.getElementById('loginErr').textContent='Incorrect password';
  }).catch(()=>document.getElementById('loginErr').textContent='Connection error');
}
function logout(){ TOKEN=''; localStorage.removeItem('vm_token');
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
function tab(name){
  document.querySelectorAll('.tabs button').forEach(b=>b.classList.toggle('active',b.dataset.t===name));
  ['overview','voters','cands','results','control'].forEach(s=>
    document.getElementById('v-'+s).classList.toggle('hidden',s!==name));
  refreshAll();
}

/* ---- data refresh ---- */
function refreshAll(){
  jget('/api/overview').then(renderOverview).catch(()=>{});
  jget('/api/voters').then(renderVoters).catch(()=>{});
  jget('/api/candidates').then(renderCands).catch(()=>{});
  jget('/api/results').then(renderResults).catch(()=>{});
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
function startEnroll(){
  const name=document.getElementById('vName').value.trim();
  if(!name){toast('Enter a name first');return;}
  jpost('/api/voters/enroll',{name}).then(d=>{
    if(!d.ok){toast(d.error||'Failed');return;}
    document.getElementById('vName').value='';
    document.getElementById('enName').textContent=name;
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
        toast(s.success?'Voter enrolled':'Enrollment failed');
        refreshAll();
      }
    }).catch(()=>{});
  },800);
}
function cancelEnroll(){ jpost('/api/voters/enroll/cancel'); }
function closeEnroll(){ clearInterval(enrollTimer);
  document.getElementById('enrollModal').classList.add('hidden'); }

/* ---- boot ---- */
if(TOKEN){ jget('/api/overview').then(()=>enterApp()).catch(()=>logout()); }
</script>
</body>
</html>)HTML";
