const $ = s => document.querySelector(s);
const $$ = s => document.querySelectorAll(s);
let state = null;

const hex2rgb = h => ({ r:parseInt(h.slice(1,3),16), g:parseInt(h.slice(3,5),16), b:parseInt(h.slice(5,7),16) });
const rgb2hex = (r,g,b) => '#' + [r,g,b].map(x => x.toString(16).padStart(2,'0')).join('');

async function api(path, body) {
  const opt = body ? { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(body) } : {};
  const r = await fetch(path, opt);
  return r.json();
}

// 标签页切换
$$('.tab').forEach(t => t.onclick = () => {
  $$('.tab').forEach(x => x.classList.remove('active'));
  $$('.panel').forEach(x => x.classList.remove('active'));
  t.classList.add('active');
  $('#' + t.dataset.tab).classList.add('active');
});

function fillEffects(sel, names) {
  sel.innerHTML = names.map((n,i) => `<option value="${i}">${n}</option>`).join('');
}

function renderPresets() {
  const ul = $('#presetList'); ul.innerHTML = '';
  const dp = $('#defaultPreset'); dp.innerHTML = '';
  state.presets.forEach((p,i) => {
    const li = document.createElement('li');
    li.innerHTML = `<span class="sw" style="background:${rgb2hex(p.r,p.g,p.b)}"></span>
      <input type="text" value="${p.name}" data-i="${i}" data-k="name">
      <select data-i="${i}" data-k="effect">${state.effects.map((n,j)=>`<option value="${j}" ${j==p.effect?'selected':''}>${n}</option>`).join('')}</select>
      <input type="color" value="${rgb2hex(p.r,p.g,p.b)}" data-i="${i}" data-k="color">
      <button data-del="${i}">✕</button>`;
    ul.appendChild(li);
    dp.innerHTML += `<option value="${i}" ${i==state.defaultPreset?'selected':''}>${p.name}</option>`;
  });
  ul.querySelectorAll('[data-del]').forEach(b => b.onclick = () => {
    state.presets.splice(+b.dataset.del,1); renderPresets();
  });
}

function load(s) {
  state = s;
  $('#status').textContent = (s.wifi.ap ? 'AP配网 ' : 'WiFi ') + s.wifi.ip;
  fillEffects($('#fx'), s.effects);
  // 系统
  $('#ledPin').value = s.led.pin;
  $('#ledCount').value = s.led.count;
  $('#ledBright').value = s.led.brightness;
  $('#ssid').value = s.wifi.ssid || '';
  $('#apSsid').value = s.wifi.apSsid || '';
  $('#apFallback').checked = s.wifi.apFallback;
  // 预设
  $('#rotate').checked = s.rotateOnBoot;
  renderPresets();
  // RC
  const rc = s.rc;
  $('#rcEnabled').checked = rc.enabled;
  $('#rcBrakePin').value = rc.brakePin;
  $('#rcCenter').value = rc.centerUs;
  $('#rcBrake').value = rc.brakeUs;
  $('#rcLeftPin').value = rc.leftPin;
  $('#rcRightPin').value = rc.rightPin;
  $('#rcTurnTrig').value = rc.turnTriggerUs;
  $('#rcTurnHold').value = rc.turnHoldMs;
  $('#rcInvert').checked = rc.invert;
}

async function refresh(){ load(await api('/api/state')); }

// 实时预览
function livePreset() {
  const c = hex2rgb($('#color').value);
  return { effect:+$('#fx').value, r:c.r, g:c.g, b:c.b,
           speed:+$('#speed').value, brightness:+$('#bright').value };
}
$('#apply').onclick = () => api('/api/apply', livePreset());
$('#speed').oninput = $('#bright').oninput = $('#fx').onchange = $('#color').oninput =
  () => api('/api/apply', livePreset());

$('#saveAsPreset').onclick = async () => {
  if (state.presets.length >= state.maxPresets) return alert('预设已满');
  const p = livePreset(); p.name = state.effects[p.effect];
  state.presets.push(p); renderPresets();
  $$('.tab')[1].click();
};

// 收集预设列表(含行内编辑)
function collectPresets() {
  $('#presetList li').length;
  $$('#presetList li').forEach((li,i) => {
    const name = li.querySelector('[data-k=name]').value;
    const effect = +li.querySelector('[data-k=effect]').value;
    const c = hex2rgb(li.querySelector('[data-k=color]').value);
    Object.assign(state.presets[i], { name, effect, r:c.r, g:c.g, b:c.b });
  });
  return state.presets;
}
$('#savePresets').onclick = async () => {
  const presets = collectPresets();
  load(await api('/api/presets', { presets }));
  alert('已保存');
};
$('#rotate').onchange = () => api('/api/rotate',
  { rotateOnBoot:$('#rotate').checked, defaultPreset:+$('#defaultPreset').value });
$('#defaultPreset').onchange = () => api('/api/rotate',
  { rotateOnBoot:$('#rotate').checked, defaultPreset:+$('#defaultPreset').value });

// RC
$('#saveRc').onclick = async () => {
  load(await api('/api/rc', {
    enabled:$('#rcEnabled').checked, invert:$('#rcInvert').checked,
    brakePin:+$('#rcBrakePin').value, centerUs:+$('#rcCenter').value,
    brakeUs:+$('#rcBrake').value, leftPin:+$('#rcLeftPin').value,
    rightPin:+$('#rcRightPin').value, turnTriggerUs:+$('#rcTurnTrig').value,
    turnHoldMs:+$('#rcTurnHold').value }));
  alert('已保存');
};

// 系统
$('#saveLed').onclick = async () => {
  load(await api('/api/led', { pin:+$('#ledPin').value,
    count:+$('#ledCount').value, brightness:+$('#ledBright').value }));
  alert('已保存');
};
$('#saveAp').onclick = async () => {
  load(await api('/api/ap', { apSsid:$('#apSsid').value, apFallback:$('#apFallback').checked }));
  alert('已保存');
};
$('#saveWifi').onclick = async () => {
  if (!confirm('保存后设备会重启连接新 WiFi')) return;
  await api('/api/wifi', { ssid:$('#ssid').value, pass:$('#pass').value });
  alert('已保存, 设备重启中…');
};
$('#reboot').onclick = () => confirm('重启?') && api('/api/reboot');
$('#factory').onclick = () => confirm('恢复出厂并清除所有设置?') && api('/api/factory');

// OTA
$('#otaBtn').onclick = () => {
  const f = $('#fw').files[0];
  if (!f) return alert('选择 .bin 文件');
  const fd = new FormData(); fd.append('update', f);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update');
  const prog = $('#otaProg'); prog.hidden = false;
  xhr.upload.onprogress = e => prog.value = e.loaded / e.total * 100;
  xhr.onload = () => alert(xhr.status==200 ? '升级成功, 重启中…' : '升级失败');
  xhr.send(fd);
};

// RC 实时脉宽监视: 仅在「转向/刹车」页可见时轮询
const stMap = { off:'未启用', none:'无动作', brake:'刹车', left:'左转', right:'右转', hazard:'双闪' };
setInterval(async () => {
  if (!$('#rc').classList.contains('active')) return;
  try {
    const d = await api('/api/rc/live');
    $('#liveBrake').textContent = d.brake || '—';
    $('#liveLeft').textContent  = d.left  || '—';
    $('#liveRight').textContent = d.right || '—';
    $('#liveState').textContent = stMap[d.state] || d.state;
  } catch (e) {}
}, 250);

refresh();
