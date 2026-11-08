// SPDX-License-Identifier: AGPL-3.0-only
import { AudioEngine } from './audio.js';
import { InstrumentView, clamp, pick, noteName, sourceNames } from './view.js';
import { decodePatch, encodePatch, saveFile } from './patch.js';
import { readSetting, writeSetting } from './storage.js';

const $ = id => document.getElementById(id);
const audio = new AudioEngine();
let meta, view, P, sampleSlot = 0, saveTimer, statusTimer, recordStarted = 0, recordBusy = false;
let lastTake = null;
const pointers = new Map(), physical = new Map(), latched = new Set(), computer = new Map();
const state = {
  name: 'Harbor After Rain', preset: 0, values: [], routes: [], clips: [null, null], dirty: false,
  page: 1, lower: 0, layer: 0, selectedSource: 6, armedSource: -1, selectedRoute: 0,
  routeOffset: 0, octave: 4, hold: false, notes: new Set(), quality: 0,
  audio: false, recording: false, recordSeconds: 0, voices: 0, peak: 0,
  step: -1, sequence: false, animate: true, sources: new Float32Array(15),
  scope: new Float32Array(96), time: 0, hint: '', dragParam: -1, dragRoute: -1
};
function status(message, seconds = 5) {
  $('status').textContent = message;
  clearTimeout(statusTimer);
  if (seconds) statusTimer = setTimeout(() => { $('status').textContent = state.audio ? 'Audio ready / Landscape layout / Patches save locally' : 'Tap Enable audio to begin'; }, seconds * 1000);
}
function request() {
  view?.request();
  $('holdButton').setAttribute('aria-pressed', String(state.hold));
  $('recordButton').setAttribute('aria-pressed', String(state.recording));
  $('recordButton').textContent = state.recording ? '■ Stop' : '● REC';
  $('recordButton').disabled = recordBusy;
  $('audioToggle').setAttribute('aria-label', state.audio ? 'Resume audio' : 'Enable audio');
}
function persist() {
  state.dirty = true;
  clearTimeout(saveTimer);
  saveTimer = setTimeout(() => writeSetting('patch', encodePatch(state)).catch(() => status('Use Save patch to keep a portable copy. Browser storage is unavailable.')), 900);
  request();
}
function preview(clip) {
  if (!clip) return null;
  const result = new Float32Array(144);
  for (let i = 0; i < result.length; i++) {
    for (let j = Math.floor(i * clip.samples.length / 144); j < Math.floor((i + 1) * clip.samples.length / 144); j++)
      result[i] = Math.max(result[i], Math.abs(clip.samples[j]));
  }
  return { ...clip, preview: result };
}
function applyPatch(patch) {
  panic();
  Object.assign(state, patch);
  state.clips = state.clips.map(preview);
  state.quality = state.values[P.polyphony];
  state.selectedRoute = 0; state.routeOffset = 0; state.armedSource = -1;
  $('presets').value = String(state.preset);
  audio.patch(state);
  request();
}
function selectFactory(index) {
  index = (index + meta.factory.length) % meta.factory.length;
  const factory = meta.factory[index];
  const values = [...factory.values];
  values[P.polyphony] = state.quality;
  applyPatch({ name: factory.name, preset: index, values, routes: factory.routes.map(r => [...r]), clips: [null, null], dirty: false });
  status(factory.description);
  clearTimeout(saveTimer);
  writeSetting('patch', encodePatch(state)).catch(() => {});
}
function setParam(id, value) {
  const info = meta.params[id];
  value = clamp(value);
  if (info.choices > 1) value = pick(value, info.choices) / (info.choices - 1);
  state.values[id] = value;
  if (id === P.polyphony) state.quality = value;
  audio.send({ type: 'param', id, value });
  state.hint = info.name + ' / ' + view.valueText(id);
  persist();
}
function setRoute(slot, route) {
  state.routes[slot] = route;
  audio.send({ type: 'route', slot, route });
  persist();
}
function assign(source, target) {
  if (!meta.params[target].modTarget) return status('Choose a knob with a colored outline.');
  let slot = state.routes.findIndex(r => r[0] === source && r[1] === target);
  if (slot < 0) slot = state.routes.findIndex(r => r[0] < 0);
  if (slot < 0) return status('All 16 modulation routes are in use.');
  if (state.routes[slot][0] < 0) setRoute(slot, [source, target, 0.25]);
  state.selectedRoute = slot; state.selectedSource = source;
  state.routeOffset = Math.floor(slot / 4) * 4; state.armedSource = -1; state.lower = 4;
  status(sourceNames[source] + ' → ' + meta.params[target].name + ' / Drag Depth to adjust.');
  request();
}
function noteOn(note) {
  if (note < 0 || note > 127) return;
  physical.set(note, (physical.get(note) || 0) + 1);
  if (state.hold) {
    if (latched.has(note)) {
      latched.delete(note); state.notes.delete(note);
      audio.send({ type: 'note', on: false, note }); request(); return;
    }
    latched.add(note);
  }
  if (!state.notes.has(note)) audio.send({ type: 'note', on: true, note, velocity: 0.81 });
  state.notes.add(note);
  request();
}
function noteOff(note) {
  if (note === undefined) return;
  const count = Math.max(0, (physical.get(note) || 0) - 1);
  if (count) physical.set(note, count); else physical.delete(note);
  if (!count && !latched.has(note)) {
    state.notes.delete(note); audio.send({ type: 'note', on: false, note });
  }
  request();
}
function hold() {
  state.hold = !state.hold;
  if (!state.hold) {
    for (const note of latched) if (!physical.has(note)) {
      state.notes.delete(note); audio.send({ type: 'note', on: false, note });
    }
    latched.clear();
  }
  status(state.hold ? 'Hold on / Tap a held note again to release it.' : 'Hold off');
  request();
}
function panic() {
  physical.clear(); latched.clear(); computer.clear(); state.notes.clear();
  state.sequence = false; state.step = -1;
  audio.send({ type: 'panic' });
  request();
}
async function enable() {
  $('enableAudio').disabled = true;
  $('enableAudio').textContent = 'Starting…';
  try {
    await audio.start();
    state.audio = true;
    $('boot').classList.add('hidden');
    status('Audio ready / Touch the keyboard to play.');
  } catch (error) {
    $('bootText').textContent = error.message;
    $('boot').classList.remove('hidden');
    status(error.message, 0);
  } finally {
    $('enableAudio').disabled = false;
    $('enableAudio').textContent = 'Enable audio';
    request();
  }
}
function sheet(title, content) {
  $('sheetTitle').textContent = title;
  $('sheetBody').replaceChildren();
  content($('sheetBody'));
  if (!$('sheet').open) $('sheet').showModal();
}
function paragraph(parent, text) {
  const p = document.createElement('p'); p.textContent = text; parent.append(p); return p;
}
function actionButton(parent, label, action) {
  const button = document.createElement('button'); button.textContent = label;
  button.onclick = () => Promise.resolve(action()).catch(error => { if (error.name !== 'AbortError') status(error.message, 0); });
  parent.append(button); return button;
}
function closeSheet() { $('sheet').close(); }
function choices(id) {
  const info = meta.params[id];
  sheet(info.name, parent => {
    for (let i = 0; i < info.choices; i++) {
      const button = actionButton(parent, view.choiceText(id, i), () => {
        setParam(id, i / (info.choices - 1));
        if (id === P.aType || id === P.bType) state.layer = id / P.bType;
        closeSheet(); request();
      });
      button.setAttribute('aria-pressed', String(pick(state.values[id], info.choices) === i));
    }
  });
}
function sampleMenu(slot) {
  sheet('Engine ' + (slot + 1) + ' audio', parent => {
    paragraph(parent, 'The first eight seconds are stored as a mono sample. C4 plays at the source pitch. Your audio stays on this device.');
    actionButton(parent, 'Load audio from Files', () => { sampleSlot = slot; closeSheet(); $('sampleFile').click(); });
    actionButton(parent, 'Use Weathered Tape', () => {
      state.clips[slot] = null; audio.send({ type: 'clip', slot, clip: null }); persist(); closeSheet();
    });
  });
}
function savePatch() {
  sheet('Save portable patch', parent => {
    const input = document.createElement('input');
    input.type = 'text'; input.value = state.name; input.maxLength = 120; input.setAttribute('aria-label', 'Patch name');
    parent.append(input);
    paragraph(parent, 'This .mica file includes the imported samples and opens in the desktop instrument too.');
    actionButton(parent, 'Save patch to Files', async () => {
      state.name = input.value.trim().slice(0, 120) || 'Untitled';
      const name = state.name.replace(/[<>:"/\\|?*\u0000-\u001f]/g, '-') + '.mica';
      const blob = new Blob([encodePatch(state)], { type: 'application/octet-stream' });
      await saveFile(blob, name);
      state.dirty = false;
      await writeSetting('patch', encodePatch(state)).catch(() => {});
      closeSheet(); status('Patch export opened. Choose Save to Files.'); request();
    });
  });
}
function menu() {
  sheet('Mica Field', parent => {
    actionButton(parent, 'Open patch from Files', () => { closeSheet(); $('patchFile').click(); });
    actionButton(parent, 'Save patch to Files', savePatch);
    actionButton(parent, 'Voice limit · ' + (4 + pick(state.quality, 3) * 4), () => choices(P.polyphony));
    actionButton(parent, state.animate ? 'Pause reactive visuals' : 'Resume reactive visuals', () => {
      state.animate = !state.animate; closeSheet(); request();
    });
    actionButton(parent, 'How to use on iPad', () => sheet('Playing on iPad', body => {
      paragraph(body, 'Use landscape orientation for the full editor. Tap Enable audio, then touch the Keyboard panel. Hold latches notes into a drone.');
      paragraph(body, 'Drag knobs vertically. Tap a colored modulation source, then a knob. Set its depth in Routes.');
      paragraph(body, 'REC captures stereo audio. Tap Stop, then Save WAV and choose Save to Files. Keep the instrument open while recording.');
      paragraph(body, 'In Safari, open Share and choose Add to Home Screen. The instrument is cached for offline use after the first complete load.');
      paragraph(body, 'Web edition · Shared desktop DSP · 4, 8 or 12 voices · Stereo 16-bit WAV.');
    }));
    actionButton(parent, 'Panic / clear notes and tails', () => { panic(); closeSheet(); });
    const source = document.createElement('a');
    source.href = 'https://github.com/todaimori-jpg/Mica-Field/tree/main/web';
    source.textContent = 'Source code · AGPL-3.0';
    source.target = '_blank'; source.rel = 'noopener'; parent.append(source);
  });
}
function generate() {
  const scale = pick(state.values[P.seqScale], 6);
  const intervals = [[0, 2, 3, 5, 7, 8, 10], [0, 2, 3, 5, 7, 9, 10], [0, 2, 4, 5, 7, 9, 11],
    [0, 3, 5, 7, 10], [0, 2, 4, 6, 8, 10], [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]][scale];
  for (let i = 0; i < 16; i++) {
    const degree = i ? Math.floor(Math.random() * 15) - 4 : 0;
    const within = (degree % intervals.length + intervals.length) % intervals.length;
    const pitch = 12 * ((degree - within) / intervals.length) + intervals[within];
    const at = P.stepBase + i * 4;
    setParam(at, i === 0 || Math.random() < 0.66 ? 1 : 0);
    setParam(at + 1, clamp((pitch + 24) / 48));
    setParam(at + 2, 0.38 + Math.random() * 0.5);
    setParam(at + 3, i === 0 ? 1 : 0.48 + Math.random() * 0.52);
  }
  status('New pattern generated in ' + view.choiceText(P.seqScale, scale));
}
async function sequence() {
  if (!audio.ready) {
    await enable();
    if (!audio.ready) return;
  }
  state.sequence = !state.sequence;
  audio.send({ type: 'sequence', on: state.sequence });
  request();
}
async function exportTake() {
  if (!lastTake) lastTake = await audio.recordStore.blob();
  if (!lastTake) return status('Record a take first.');
  const date = new Date(lastTake.take.date).toISOString().replace(/[:.]/g, '-').slice(0, 19);
  await saveFile(lastTake.blob, 'Mica-Field-' + date + '.wav');
  status('WAV export opened. Choose Save to Files.');
}
async function beginRecording() {
  recordBusy = true; request();
  try {
    if (!audio.ready) await enable();
    if (!audio.ready) return;
    await audio.startRecording();
    lastTake = null; $('saveTake').hidden = true;
    state.recording = true; recordStarted = performance.now();
    status('Recording stereo WAV / Tap Stop to finish.', 0);
  } finally { recordBusy = false; request(); }
}
async function record() {
  if (recordBusy) return;
  if (state.recording) {
    recordBusy = true; request(); audio.stopRecording();
    return;
  }
  const previous = await readSetting('take').catch(() => null);
  if (previous) {
    sheet('Previous take', parent => {
      paragraph(parent, 'The previous recording is still stored on this device.');
      actionButton(parent, 'Save previous WAV to Files', exportTake);
      actionButton(parent, 'Replace it with a new recording', () => { closeSheet(); return beginRecording(); });
    });
  } else await beginRecording();
}
const recall = new Map();
async function perform(hit) {
  const id = hit.id;
  switch (hit.type) {
    case 'menu': menu(); break;
    case 'audio': await enable(); break;
    case 'page': state.page = id; break;
    case 'lower': state.lower = id; if (id === 4) state.armedSource = -1; break;
    case 'layer': state.layer = id; break;
    case 'choice': choices(id); break;
    case 'previous': selectFactory(Math.max(0, state.preset) - 1); break;
    case 'next': selectFactory(state.preset + 1); break;
    case 'param': if (state.armedSource >= 0) assign(state.armedSource, id); else status(meta.params[id].name + ' / ' + view.valueText(id)); break;
    case 'power':
    case 'filterPower': {
      const filter = hit.type === 'filterPower', value = state.values[id];
      const on = filter ? pick(value, 5) !== 4 : value > 0.001;
      if (on) recall.set(id, value);
      setParam(id, on ? (filter ? 1 : 0) : recall.get(id) ?? meta.params[id].initial);
      break;
    }
    case 'source':
      state.armedSource = state.armedSource === id ? -1 : id; state.selectedSource = id;
      if (id >= 3 && id <= 5) state.lower = 1;
      if (id >= 6 && id <= 8) state.lower = 2;
      if (id === 9) state.lower = 3;
      if (id < 3) state.lower = 0;
      break;
    case 'toggle': setParam(id, state.values[id] > 0.5 ? 0 : 1); break;
    case 'sequence': await sequence(); break;
    case 'generate': generate(); break;
    case 'hold': hold(); break;
    case 'panic': panic(); break;
    case 'record': await record(); break;
    case 'sample': sampleMenu(id); break;
    case 'octave': state.octave = clamp(state.octave + id, 1, 7); break;
    case 'visual': state.animate = !state.animate; break;
    case 'routeSelect': state.selectedRoute = id; state.selectedSource = state.routes[id][0]; break;
    case 'routeClear': setRoute(id, [-1, -1, 0]); break;
    case 'routeScroll': state.routeOffset = clamp(state.routeOffset + id * 4, 0, 12); break;
  }
  request();
}
function bindPointers() {
  const canvas = $('instrument');
  canvas.addEventListener('pointerdown', e => {
    e.preventDefault(); canvas.focus(); canvas.setPointerCapture(e.pointerId);
    const point = view.point(e), hit = view.hit(point);
    if (!hit) return;
    const gesture = { hit, point, moved: false, note: undefined, value: hit.type === 'param' ? state.values[hit.id] : hit.type === 'routeDepth' ? state.routes[hit.id][2] : 0 };
    pointers.set(e.pointerId, gesture);
    if (hit.type === 'key') { gesture.note = hit.id; noteOn(hit.id); }
    if (hit.type === 'param') state.dragParam = hit.id;
    if (hit.type === 'routeDepth') state.dragRoute = hit.id;
    request();
  });
  canvas.addEventListener('pointermove', e => {
    const gesture = pointers.get(e.pointerId);
    if (!gesture) return;
    e.preventDefault();
    const point = view.point(e), dx = point.x - gesture.point.x, dy = point.y - gesture.point.y;
    if (Math.hypot(dx, dy) > 3) gesture.moved = true;
    const hit = gesture.hit;
    if (hit.type === 'key') {
      const next = view.hit(point);
      if (next?.type === 'key' && next.id !== gesture.note) {
        noteOff(gesture.note); gesture.note = next.id; noteOn(next.id);
      }
    }
    if (gesture.moved && hit.type === 'param') setParam(hit.id, gesture.value + (dx * 0.26 - dy) / (e.shiftKey ? 710 : 126));
    if (gesture.moved && hit.type === 'routeDepth') {
      const route = [...state.routes[hit.id]];
      route[2] = clamp(gesture.value + (dx * 0.26 - dy) / 63, -1, 1);
      setRoute(hit.id, route);
    }
    request();
  });
  const finish = (e, cancelled) => {
    const gesture = pointers.get(e.pointerId);
    if (!gesture) return;
    pointers.delete(e.pointerId);
    if (gesture.note !== undefined) noteOff(gesture.note);
    const target = view.hit(view.point(e));
    if (!cancelled && gesture.hit.type === 'source' && gesture.moved && target?.type === 'param')
      assign(gesture.hit.id, target.id);
    else if (!cancelled && !gesture.moved && target?.type === gesture.hit.type && target.id === gesture.hit.id)
      perform(target).catch(error => status(error.message, 0));
    state.dragParam = state.dragRoute = -1; request();
  };
  canvas.addEventListener('pointerup', e => finish(e, false));
  canvas.addEventListener('pointercancel', e => finish(e, true));
  canvas.addEventListener('dblclick', e => {
    const hit = view.hit(view.point(e));
    if (hit?.type === 'param') setParam(hit.id, meta.params[hit.id].initial);
  });
  canvas.addEventListener('wheel', e => {
    const hit = view.hit(view.point(e));
    if (hit?.type !== 'param') return;
    e.preventDefault();
    const count = meta.params[hit.id].choices;
    const delta = count > 1 ? 1 / (count - 1) : e.shiftKey ? 0.003 : 0.015;
    setParam(hit.id, state.values[hit.id] - Math.sign(e.deltaY) * delta);
  }, { passive: false });
}
const keyMap = "AWSEDFTGYHUJKOLP;'";
window.addEventListener('keydown', e => {
  if (!meta || $('sheet').open || e.ctrlKey || e.metaKey || e.altKey || e.repeat) return;
  const key = e.key.toUpperCase();
  if (key === 'ESCAPE') return panic();
  if (key === ' ' && state.page === 3) { e.preventDefault(); sequence(); return; }
  if (key === 'Z' || key === 'X') { state.octave = clamp(state.octave + (key === 'X' ? 1 : -1), 1, 7); request(); return; }
  const index = keyMap.indexOf(key);
  if (index >= 0 && !computer.has(key)) {
    const note = (state.octave + 1) * 12 + index; computer.set(key, note); noteOn(note);
  }
});
window.addEventListener('keyup', e => {
  const key = e.key.toUpperCase();
  if (computer.has(key)) { noteOff(computer.get(key)); computer.delete(key); }
});
window.addEventListener('blur', () => { for (const note of computer.values()) noteOff(note); computer.clear(); });
document.addEventListener('visibilitychange', () => {
  if (document.hidden && state.recording) {
    recordBusy = true; audio.stopRecording(); request();
  }
});
$('closeSheet').onclick = closeSheet;
$('menuButton').onclick = menu;
$('enableAudio').onclick = enable;
$('audioToggle').onclick = enable;
$('keyboardButton').onclick = () => { state.lower = 0; request(); };
$('holdButton').onclick = hold;
$('panicButton').onclick = panic;
$('recordButton').onclick = () => record().catch(error => status(error.message, 0));
$('saveTake').onclick = () => exportTake().catch(error => { if (error.name !== 'AbortError') status(error.message, 0); });
$('presets').onchange = e => selectFactory(Number(e.target.value));
$('sampleFile').onchange = async e => {
  const file = e.target.files[0];
  if (!file) return;
  status('Reading sample…', 0);
  try {
    const clip = preview(await audio.decode(file));
    state.clips[sampleSlot] = clip;
    audio.send({ type: 'clip', slot: sampleSlot, clip: { samples: clip.samples } });
    persist(); status('Sample loaded / First eight seconds / C4 is source pitch.');
  } catch (error) { status(error.message, 0); }
  e.target.value = '';
};
$('patchFile').onchange = async e => {
  const file = e.target.files[0];
  if (!file) return;
  try {
    if (file.size > 2 * 1024 * 1024) throw new Error('Choose a Mica Field patch under 2 MB.');
    applyPatch(decodePatch(await file.arrayBuffer(), meta));
    await writeSetting('patch', encodePatch(state)).catch(() => {});
    status('Patch loaded with its samples and routes.');
  } catch (error) { status(error.message, 0); }
  e.target.value = '';
};
audio.addEventListener('statechange', () => {
  state.audio = audio.ready && audio.context?.state === 'running';
  if (!state.audio && audio.ready) status('Tap ENABLE to resume audio.');
  request();
});
audio.addEventListener('meter', e => {
  const meter = e.detail;
  const before = state.peak;
  state.peak = Math.max(meter.peak, state.peak * 0.84);
  state.voices = meter.voices; state.step = meter.step;
  state.sources = meter.sources; state.scope = meter.scope;
  if (state.animate && (state.voices || state.peak > 0.0005)) state.time += 0.053;
  if (state.recording) state.recordSeconds = (performance.now() - recordStarted) / 1000;
  $('signal').dataset.peak = String(meter.peak);
  $('signal').textContent = state.recording ? Math.floor(state.recordSeconds) + ' s recorded' : state.voices + ' voices';
  if (state.voices || state.peak > 0.0005 || before > 0.0005 || state.sequence || state.recording) request();
});
audio.addEventListener('recorded', e => {
  recordBusy = false; state.recording = false; lastTake = e.detail;
  if (lastTake) {
    $('saveTake').hidden = false;
    sheet('Recording ready', parent => {
      paragraph(parent, (lastTake.frames / lastTake.take.rate).toFixed(1) + ' seconds · Stereo 16-bit WAV · ' + (lastTake.blob.size / 1048576).toFixed(1) + ' MB');
      actionButton(parent, 'Save WAV to Files', async () => { await exportTake(); closeSheet(); });
    });
    status('Recording ready / Tap Save WAV, then Save to Files.', 0);
  } else status('The take is empty. Play a note while recording.');
  request();
});
audio.addEventListener('error', e => { recordBusy = false; state.recording = false; status(e.detail, 0); request(); });
function resize() {
  if (!view) return;
  const app = $('app'), footer = $('touchBar');
  const width = Math.min(1240, app.clientWidth);
  const available = Math.max(100, app.clientHeight - footer.offsetHeight - 2);
  const w = Math.min(width, available * 1240 / 758), h = w * 758 / 1240;
  $('stage').style.width = w + 'px'; $('stage').style.height = h + 'px';
  view.resize(w, h);
}
window.addEventListener('resize', resize);
window.visualViewport?.addEventListener('resize', resize);
try {
  const response = await fetch('./meta.json');
  if (!response.ok) throw new Error('The preset bank could not load.');
  meta = await response.json(); P = meta.P;
  Object.assign(state, { values: [...meta.factory[0].values], routes: meta.factory[0].routes.map(r => [...r]) });
  state.values[P.polyphony] = 0;
  const saved = await readSetting('patch').catch(() => null);
  if (saved) {
    try { Object.assign(state, decodePatch(saved, meta)); state.clips = state.clips.map(preview); state.quality = state.values[P.polyphony]; }
    catch { /* Keep the factory patch when saved browser state is incomplete. */ }
  }
  for (let i = 0; i < meta.factory.length; i++) {
    const option = document.createElement('option'); option.value = i; option.textContent = meta.factory[i].name; $('presets').append(option);
  }
  $('presets').value = String(state.preset);
  view = new InstrumentView($('instrument'), state, meta);
  audio.patch(state);
  bindPointers(); resize();
  status('Tap Enable audio, then touch the keyboard.', 0);
  const take = await readSetting('take').catch(() => null);
  if (take) $('saveTake').hidden = false;
  if ('serviceWorker' in navigator) navigator.serviceWorker.register('./sw.js').catch(() => {});
} catch (error) {
  $('bootText').textContent = error.message;
  $('enableAudio').disabled = true;
  status(error.message, 0);
}
