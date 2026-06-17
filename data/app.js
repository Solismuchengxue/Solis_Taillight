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

// ---------- 中英文 i18n ----------
const I18N = {
  zh: {
    appTitle:"Solis 尾灯", tabLive:"灯效", tabConfig:"配置", tabSys:"系统",
    presetLegend:"预设 (最多 5 个)", thName:"名称", thEffect:"灯效", thColor:"颜色",
    thSpeed:"速度<br>(0-255)", thBright:"亮度<br>(0-255)", thPreview:"预览",
    presetHint:"改完点该行「预览」下发到灯条(不保存)；点「保存预设列表」才写入设备。空行点＋添加(最多5个)。",
    savePresets:"保存预设列表",
    decoLegend:"装饰灯", ledDataPin:"灯条数据脚 GPIO", ledCount:"灯珠数量",
    defaultPreset:"默认预设", rotateOnBoot:"上电轮换灯效",
    leftLegend:"左转", rightLegend:"右转", enableLeft:"启用左转", enableRight:"启用右转",
    triggerPin:"触发脚 GPIO", triggerMode:"触发模式", modeMomentary:"点动", modeMaintained:"两点",
    holdMs:"保持时长 (ms)", triggerUs:"触发阈值 (us)", effectReverse:"灯效反向",
    turnHint:"转向固定为<b>琥珀色流水</b>(交通标准, 仅点动)。左右各自独立灯带/通道, 按一下流水「保持时长」后自停。左右<b>同时触发=双闪并锁存</b>, 再次同时触发才取消。流水方向不对就勾「灯效反向」。改数据脚/启用后设备重启重建灯带。",
    brakeLegend:"刹车 (复用油门通道, 实时)", enableBrake:"启用刹车", throttlePin:"油门脚 GPIO",
    centerUs:"中位 (us)", brakeUs:"刹车阈值 (us)", brakeReverse:"刹车方向反向 (杆量高于中位判刹车)",
    brakeHint:"刹车灯固定为<b>红色常亮</b>(交通标准)。默认油门<b>低于</b>「中位 − 阈值」点亮; 若你的杆量1000是前进, 勾选「方向反向」改成<b>高于</b>「中位 + 阈值」点亮。",
    hazardLegend:"双闪", hazardTrigger:"触发方式", hzBoth:"左右同时触发", hzDtap:"双击任意转向触发", hzOff:"关闭",
    hazardHint:"双闪为<b>锁存</b>: 触发一次进入双闪并保持, 再次触发才取消。需启用左右转。",
    saveConfig:"保存配置 (重启生效)",
    monitorLegend:"实时脉宽监视 (标定用)", monThrottle:"油门", monLeft:"左", monRight:"右", monState:"当前判定",
    monitorHint:"拨动遥控器/按转向键, 看脉宽数值, 据此设上面各项阈值。",
    apLegend:"AP 配网热点", apSsidLabel:"热点名称 (SSID)", apFallback:"找不到 WiFi 时打开 AP",
    apHint:"未配过 WiFi 时始终会开 AP；此开关仅控制「已配 WiFi 但连不上」时是否回退。改名下次开 AP 生效。",
    saveAp:"保存 AP 设置", wifiLegend:"WiFi", ssidLabel:"SSID", passLabel:"密码", saveWifi:"保存并重启",
    otaLegend:"固件 OTA", otaBtn:"上传固件", maintLegend:"维护", reboot:"重启", factory:"恢复出厂",
    apMode:"AP配网", wifiMode:"WiFi", newPreset:"新预设",
    st_off:"未启用", st_none:"无动作", st_brake:"刹车", st_left:"左转", st_right:"右转", st_hazard:"双闪",
    alSaved:"已保存", alSavedReboot:"已保存, 设备重启生效中…", cfWifi:"保存后设备会重启连接新 WiFi",
    alWifiSaved:"已保存, 设备重启中…", cfReboot:"重启?", alRebooting:"设备重启中…",
    cfFactory:"恢复出厂并清除所有设置?", alFactory:"恢复出厂中, 设备将开 AP…",
    alOtaSelect:"选择 .bin 文件", alOtaOk:"升级成功, 重启中…", alOtaFail:"升级失败"
  },
  en: {
    appTitle:"Solis Taillight", tabLive:"Effects", tabConfig:"Config", tabSys:"System",
    presetLegend:"Presets (max 5)", thName:"Name", thEffect:"Effect", thColor:"Color",
    thSpeed:"Speed<br>(0-255)", thBright:"Bright<br>(0-255)", thPreview:"Preview",
    presetHint:"Click a row's Preview to push live (not saved); Save presets writes to the device. Click ＋ on an empty row to add (max 5).",
    savePresets:"Save presets",
    decoLegend:"Decoration", ledDataPin:"LED data GPIO", ledCount:"LED count",
    defaultPreset:"Default preset", rotateOnBoot:"Cycle effect on boot",
    leftLegend:"Left turn", rightLegend:"Right turn", enableLeft:"Enable left", enableRight:"Enable right",
    triggerPin:"Trigger GPIO", triggerMode:"Trigger mode", modeMomentary:"Momentary", modeMaintained:"Maintained",
    holdMs:"Hold (ms)", triggerUs:"Threshold (us)", effectReverse:"Reverse direction",
    turnHint:"Turn is fixed <b>amber flow</b> (traffic standard, momentary only). Left/right use independent strips & channels; one tap flows for Hold then stops. Pressing both together toggles <b>latched hazard</b> (press both again to cancel). Tick Reverse if the flow goes the wrong way. Changing data pin/enable reboots to rebuild strips.",
    brakeLegend:"Brake (throttle channel, realtime)", enableBrake:"Enable brake", throttlePin:"Throttle GPIO",
    centerUs:"Center (us)", brakeUs:"Brake threshold (us)", brakeReverse:"Reverse brake dir (brake = above center)",
    brakeHint:"Brake is fixed <b>solid red</b> (traffic standard). Lit when throttle is <b>below</b> Center − threshold; if your stick 1000 means forward, tick Reverse to light when <b>above</b> Center + threshold.",
    hazardLegend:"Hazard", hazardTrigger:"Trigger", hzBoth:"Both turns at once", hzDtap:"Double-tap either turn", hzOff:"Off",
    hazardHint:"Hazard is <b>latched</b>: trigger once to enter and hold, trigger again to cancel. Requires left/right enabled.",
    saveConfig:"Save config (reboots)",
    monitorLegend:"Live pulse monitor (calibration)", monThrottle:"Throttle", monLeft:"Left", monRight:"Right", monState:"State",
    monitorHint:"Move the sticks / press turn keys, watch the pulse values, and set the thresholds above.",
    apLegend:"AP hotspot", apSsidLabel:"Hotspot name (SSID)", apFallback:"Open AP when WiFi not found",
    apHint:"AP always opens when no WiFi is configured; this switch only controls fallback when WiFi is set but unreachable. Rename applies on next AP start.",
    saveAp:"Save AP", wifiLegend:"WiFi", ssidLabel:"SSID", passLabel:"Password", saveWifi:"Save & reboot",
    otaLegend:"Firmware OTA", otaBtn:"Upload firmware", maintLegend:"Maintenance", reboot:"Reboot", factory:"Factory reset",
    apMode:"AP", wifiMode:"WiFi", newPreset:"New preset",
    st_off:"Off", st_none:"None", st_brake:"Brake", st_left:"Left", st_right:"Right", st_hazard:"Hazard",
    alSaved:"Saved", alSavedReboot:"Saved, device rebooting…", cfWifi:"Device will reboot to join the new WiFi",
    alWifiSaved:"Saved, rebooting…", cfReboot:"Reboot?", alRebooting:"Rebooting…",
    cfFactory:"Factory reset and erase all settings?", alFactory:"Factory resetting, AP will open…",
    alOtaSelect:"Select a .bin file", alOtaOk:"Upgrade OK, rebooting…", alOtaFail:"Upgrade failed"
  }
};
let LANG = localStorage.getItem('lang') || 'zh';
function t(k){ const d = I18N[LANG] || I18N.zh; return (k in d) ? d[k] : (I18N.zh[k] || k); }
function applyLang(){
  document.documentElement.lang = LANG;
  document.querySelectorAll('[data-i18n]').forEach(el => { el.innerHTML = t(el.dataset.i18n); });
  $('#langBtn').textContent = LANG === 'zh' ? 'EN' : '中';
  if (state) $('#status').textContent = (state.wifi.ap ? t('apMode') : t('wifiMode')) + ' ' + state.wifi.ip;
}
$('#langBtn').onclick = () => {
  LANG = LANG === 'zh' ? 'en' : 'zh';
  localStorage.setItem('lang', LANG);
  applyLang();
};

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
    state.presets.push({ name:t('newPreset'), effect:0, r:255, g:0, b:0, speed:128, brightness:200 });
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
  $('#status').textContent = (s.wifi.ap ? t('apMode') : t('wifiMode')) + ' ' + s.wifi.ip;
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
  $('#rcLHold').value = L.holdMs;
  $('#rcLTrig').value = L.triggerUs; $('#rcLReverse').checked = L.reverse;
  $('#rcREn').checked = R.enabled;   $('#rcRLedPin').value = R.ledPin;
  $('#rcRCount').value = R.ledCount; $('#rcRChan').value = R.channelPin;
  $('#rcRHold').value = R.holdMs;
  $('#rcRTrig').value = R.triggerUs; $('#rcRReverse').checked = R.reverse;
  $('#rcHazardMode').value = rc.hazardMode;
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
  alert(t('alSaved'));
};
// 配置页统一保存 (装饰灯 + 左/右转 + 刹车, 一次性, 保存后重启重建灯带)
$('#saveConfig').onclick = async () => {
  await api('/api/rotate', { rotateOnBoot:$('#rotate').checked, defaultPreset:+$('#defaultPreset').value });
  await api('/api/led', { pin:+$('#ledPin').value, count:+$('#ledCount').value });
  await api('/api/rc', {
    brakeEnabled:$('#rcBrakeEnabled').checked, brakeReverse:$('#rcBrakeReverse').checked,
    brakeLedPin:+$('#rcBrakeLedPin').value, brakeLedCount:+$('#rcBrakeCount').value,
    brakePin:+$('#rcBrakePin').value, centerUs:+$('#rcCenter').value, brakeUs:+$('#rcBrake').value,
    hazardMode:+$('#rcHazardMode').value,
    left:{ enabled:$('#rcLEn').checked, ledPin:+$('#rcLLedPin').value, ledCount:+$('#rcLCount').value,
           channelPin:+$('#rcLChan').value, holdMs:+$('#rcLHold').value,
           triggerUs:+$('#rcLTrig').value, reverse:$('#rcLReverse').checked },
    right:{ enabled:$('#rcREn').checked, ledPin:+$('#rcRLedPin').value, ledCount:+$('#rcRCount').value,
            channelPin:+$('#rcRChan').value, holdMs:+$('#rcRHold').value,
            triggerUs:+$('#rcRTrig').value, reverse:$('#rcRReverse').checked }
  });
  alert(t('alSavedReboot'));
};
$('#saveAp').onclick = async () => {
  load(await api('/api/ap', { apSsid:$('#apSsid').value, apFallback:$('#apFallback').checked }));
  alert(t('alSaved'));
};
$('#saveWifi').onclick = async () => {
  if (!confirm(t('cfWifi'))) return;
  await api('/api/wifi', { ssid:$('#ssid').value, pass:$('#pass').value });
  alert(t('alWifiSaved'));
};
$('#reboot').onclick = () => {
  if (confirm(t('cfReboot'))) { fetch('/api/reboot', {method:'POST'}); alert(t('alRebooting')); }
};
$('#factory').onclick = () => {
  if (confirm(t('cfFactory'))) { fetch('/api/factory', {method:'POST'}); alert(t('alFactory')); }
};

// OTA
$('#otaBtn').onclick = () => {
  const f = $('#fw').files[0];
  if (!f) return alert(t('alOtaSelect'));
  const fd = new FormData(); fd.append('update', f);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update');
  const prog = $('#otaProg'); prog.hidden = false;
  xhr.upload.onprogress = e => prog.value = e.loaded / e.total * 100;
  xhr.onload = () => alert(xhr.status==200 ? t('alOtaOk') : t('alOtaFail'));
  xhr.send(fd);
};

// RC 实时脉宽监视: 仅在「配置」页可见时轮询
setInterval(async () => {
  if (!$('#rc').classList.contains('active')) return;
  try {
    const d = await api('/api/rc/live');
    $('#liveBrake').textContent = d.brake || '—';
    $('#liveLeft').textContent  = d.left  || '—';
    $('#liveRight').textContent = d.right || '—';
    $('#liveState').textContent = t('st_' + d.state);
  } catch (e) {}
}, 250);

applyLang();
refresh();
