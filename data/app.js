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

const SLOTS = 5;   // 固定 5 个轮换槽位

function renderPresets() {
  const tb = $('#presetBody'); tb.innerHTML = '';
  for (let i = 0; i < SLOTS; i++) {
    const tr = document.createElement('tr');
    if (i < state.presets.length) {
      const p = state.presets[i];
      tr.className = 'filled';
      const opts = state.effects.map((n,j)=>`<option value="${j}" ${j==p.effect?'selected':''}>${n}</option>`).join('');
      tr.innerHTML = `
        <td><input type="text" value="${p.name}" data-i="${i}" data-k="name"></td>
        <td><select data-i="${i}" data-k="effect">${opts}</select></td>
        <td><input type="color" value="${rgb2hex(p.r,p.g,p.b)}" data-i="${i}" data-k="color"></td>
        <td><input type="number" min="0" max="255" value="${p.speed}" data-i="${i}" data-k="speed"></td>
        <td><input type="number" min="0" max="255" value="${p.brightness}" data-i="${i}" data-k="bright"></td>
        <td><button class="prev" data-i="${i}">预览</button></td>
        <td><button class="del" data-i="${i}">✕</button></td>`;
    } else {
      tr.className = 'empty';
      tr.innerHTML = `<td colspan="7"><button class="addslot">＋</button></td>`;
    }
    tb.appendChild(tr);
  }
  tb.querySelectorAll('.del').forEach(b => b.onclick = () => {
    collectPresets();                       // 删除前保住其它行的编辑
    state.presets.splice(+b.dataset.i, 1);
    renderPresets();
  });
  tb.querySelectorAll('.addslot').forEach(b => b.onclick = () => {
    collectPresets();
    state.presets.push({ name:'新预设', effect:0, r:255, g:0, b:0, speed:128, brightness:200 });
    renderPresets();
  });
  tb.querySelectorAll('.prev').forEach(b => b.onclick = () => api('/api/apply', readRow(+b.dataset.i)));
  fillPresetSelects();
}

// 用预设列表填充三个"默认预设"下拉(装饰/转向/刹车)
function presetOptions(sel) {
  return state.presets.map((p,i) => `<option value="${i}" ${i==sel?'selected':''}>${p.name}</option>`).join('');
}
function fillPresetSelects() {
  $('#defaultPreset').innerHTML = presetOptions(state.defaultPreset);
}

// 读取某一行当前输入(含未保存编辑)
function readRow(i) {
  const q = k => $(`#presetBody [data-i="${i}"][data-k="${k}"]`);
  const c = hex2rgb(q('color').value);
  return { name:q('name').value, effect:+q('effect').value, r:c.r, g:c.g, b:c.b,
           speed:+q('speed').value, brightness:+q('bright').value };
}

function load(s) {
  state = s;
  $('#status').textContent = (s.wifi.ap ? 'AP配网 ' : 'WiFi ') + s.wifi.ip;
  // 装饰灯
  $('#ledPin').value = s.led.pin;
  $('#ledCount').value = s.led.count;
  $('#rotate').checked = s.rotateOnBoot;
  // 系统
  $('#ssid').value = s.wifi.ssid || '';
  $('#apSsid').value = s.wifi.apSsid || '';
  $('#apFallback').checked = s.wifi.apFallback;
  // 预设表 (内部会填充各"默认预设"下拉)
  renderPresets();
  // 转向 (左/右各自独立)
  const rc = s.rc, L = rc.left, R = rc.right;
  $('#rcLEn').checked = L.enabled;   $('#rcLLedPin').value = L.ledPin;
  $('#rcLCount').value = L.ledCount; $('#rcLChan').value = L.channelPin;
  $('#rcLMode').value = L.mode;      $('#rcLHold').value = L.holdMs;
  $('#rcLTrig').value = L.triggerUs; $('#rcLReverse').checked = L.reverse;
  $('#rcREn').checked = R.enabled;   $('#rcRLedPin').value = R.ledPin;
  $('#rcRCount').value = R.ledCount; $('#rcRChan').value = R.channelPin;
  $('#rcRMode').value = R.mode;      $('#rcRHold').value = R.holdMs;
  $('#rcRTrig').value = R.triggerUs; $('#rcRReverse').checked = R.reverse;
  // 刹车
  $('#rcBrakeEnabled').checked = rc.brakeEnabled;
  $('#rcBrakeReverse').checked = rc.brakeReverse;
  $('#rcBrakeLedPin').value = rc.brakeLedPin;
  $('#rcBrakeCount').value = rc.brakeLedCount;
  $('#rcBrakePin').value = rc.brakePin;
  $('#rcCenter').value = rc.centerUs;
  $('#rcBrake').value = rc.brakeUs;
}

async function refresh(){ load(await api('/api/state')); }

// 收集预设列表(只读已填充的行, 空槽跳过; 含速度/亮度)
function collectPresets() {
  $$('#presetBody tr.filled').forEach(tr => {
    const i = +tr.querySelector('[data-k=name]').dataset.i;
    Object.assign(state.presets[i], readRow(i));
  });
  return state.presets;
}
$('#savePresets').onclick = async () => {
  const presets = collectPresets();
  load(await api('/api/presets', { presets }));
  alert('已保存');
};
// 装饰灯 (轮换/默认预设 + 数据脚/数量; 后者触发重启)
$('#saveDeco').onclick = async () => {
  await api('/api/rotate', { rotateOnBoot:$('#rotate').checked, defaultPreset:+$('#defaultPreset').value });
  await api('/api/led', { pin:+$('#ledPin').value, count:+$('#ledCount').value });
  alert('已保存, 设备重启生效中…');
};

// 转向左/右 (保存后设备重启重建灯带)
$('#saveLeft').onclick = async () => {
  await api('/api/rc', { left:{
    enabled:$('#rcLEn').checked, ledPin:+$('#rcLLedPin').value, ledCount:+$('#rcLCount').value,
    channelPin:+$('#rcLChan').value, mode:+$('#rcLMode').value, holdMs:+$('#rcLHold').value,
    triggerUs:+$('#rcLTrig').value, reverse:$('#rcLReverse').checked }});
  alert('已保存, 设备重启生效中…');
};
$('#saveRight').onclick = async () => {
  await api('/api/rc', { right:{
    enabled:$('#rcREn').checked, ledPin:+$('#rcRLedPin').value, ledCount:+$('#rcRCount').value,
    channelPin:+$('#rcRChan').value, mode:+$('#rcRMode').value, holdMs:+$('#rcRHold').value,
    triggerUs:+$('#rcRTrig').value, reverse:$('#rcRReverse').checked }});
  alert('已保存, 设备重启生效中…');
};

// 刹车 (保存后设备重启重建灯带)
$('#saveBrake').onclick = async () => {
  await api('/api/rc', {
    brakeEnabled:$('#rcBrakeEnabled').checked, brakeReverse:$('#rcBrakeReverse').checked,
    brakeLedPin:+$('#rcBrakeLedPin').value, brakeLedCount:+$('#rcBrakeCount').value,
    brakePin:+$('#rcBrakePin').value, centerUs:+$('#rcCenter').value,
    brakeUs:+$('#rcBrake').value });
  alert('已保存, 设备重启生效中…');
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
$('#reboot').onclick = () => {
  if (confirm('重启?')) { fetch('/api/reboot', {method:'POST'}); alert('设备重启中…'); }
};
$('#factory').onclick = () => {
  if (confirm('恢复出厂并清除所有设置?')) { fetch('/api/factory', {method:'POST'}); alert('恢复出厂中, 设备将开 AP…'); }
};

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
