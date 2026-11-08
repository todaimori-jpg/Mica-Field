// SPDX-License-Identifier: AGPL-3.0-only
export const engineNames = ['Wavetable', 'Analog', 'Harmonic', 'Modal', 'Sample', 'Granular'];
export const engineControls = [
  ['Position', 'Warp', 'Phase'], ['Waveform', 'Detune', 'Pulse Width'],
  ['Partials', 'Tilt', 'Odd / Even'], ['Material', 'Damping', 'Strike'],
  ['Start', 'Loop', 'Length'], ['Position', 'Grain Size', 'Scatter']
];
export const filterNames = ['Low-pass', 'High-pass', 'Band-pass', 'Notch', 'Bypass'];
export const waveNames = ['Sine', 'Triangle', 'Ramp', 'Square'];
export const scaleNames = ['Minor', 'Dorian', 'Major', 'Minor Penta', 'Whole Tone', 'Chromatic'];
export const sourceNames = ['VELO', 'KBD', 'WHEEL', 'ENV AMP', 'ENV 2', 'ENV 3', 'LFO 1', 'LFO 2', 'LFO 3', 'RAND', 'STEP', 'M 1', 'M 2', 'M 3', 'M 4'];
const colors = { base: '#0f1011', panel: '#27282a', raised: '#2e2f32', header: '#1d1e21',
  hair: '#3d3f42', ink: '#e4e5e7', muted: '#9b9da3', dim: '#797c83',
  cyan: '#38c7f3', orange: '#ef8857', gold: '#dabb60', violet: '#a686ed', pink: '#ec6fb6', mint: '#78d2a1' };
export const clamp = (v, low = 0, high = 1) => Math.max(low, Math.min(high, v));
export const pick = (v, count) => Math.round(clamp(v) * (count - 1));
export const noteName = n => {
  n = clamp(n, 0, 127);
  return ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'][n % 12] + (Math.floor(n / 12) - 1);
};
const sign = n => (n > 0 ? '+' : '') + n;
const sourceColor = i => i < 3 ? colors.pink : i < 6 ? colors.orange : i < 9 ? colors.gold : i === 9 ? colors.violet : i === 10 ? colors.mint : colors.cyan;
const lfo = (shape, phase) => {
  phase = ((phase % 1) + 1) % 1;
  return shape === 1 ? 1 - 4 * Math.abs(phase - 0.5) : shape === 2 ? phase * 2 - 1 : shape === 3 ? (phase < 0.5 ? 1 : -1) : Math.sin(phase * Math.PI * 2);
};
export class InstrumentView {
  constructor(canvas, state, meta) {
    this.canvas = canvas; this.ctx = canvas.getContext('2d');
    this.state = state; this.meta = meta; this.P = meta.P; this.hits = [];
  }
  request() {
    if (this.frame) return;
    this.frame = requestAnimationFrame(() => { this.frame = null; this.draw(); });
  }
  resize(width, height) {
    const ratio = Math.min(2, window.devicePixelRatio || 1);
    this.width = width; this.height = height;
    this.canvas.width = Math.round(width * ratio);
    this.canvas.height = Math.round(height * ratio);
    this.scale = width / 1240; this.ratio = ratio;
    this.request();
  }
  point(e) {
    const r = this.canvas.getBoundingClientRect();
    return { x: (e.clientX - r.left) * 1240 / r.width, y: (e.clientY - r.top) * 758 / r.height };
  }
  hit(point) {
    return [...this.hits].reverse().find(h => point.x >= h.x && point.x <= h.x + h.w && point.y >= h.y && point.y <= h.y + h.h);
  }
  add(type, id, x, y, w, h, label = '') { this.hits.push({ type, id, x, y, w, h, label }); }
  rect(x, y, w, h, color, radius = 3) {
    const c = this.ctx; c.fillStyle = color; c.beginPath(); c.roundRect(x, y, w, h, radius); c.fill();
  }
  panel(x, y, w, h, color = colors.panel) {
    this.rect(x, y, w, h, color);
    const c = this.ctx; c.strokeStyle = '#ffffff0b'; c.lineWidth = 1;
    c.beginPath(); c.roundRect(x + 0.5, y + 0.5, w - 1, h - 1, 3); c.stroke();
  }
  text(value, x, y, w, h, color = colors.muted, size = 11, align = 'center', bold = false) {
    const c = this.ctx; c.fillStyle = color; c.font = (bold ? '500 ' : '400 ') + size + 'px -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif';
    c.textAlign = align; c.textBaseline = 'middle';
    c.fillText(String(value), align === 'left' ? x : align === 'right' ? x + w : x + w / 2, y + h / 2, w);
  }
  line(x1, y1, x2, y2, color = colors.hair, width = 1) {
    const c = this.ctx; c.strokeStyle = color; c.lineWidth = width;
    c.beginPath(); c.moveTo(x1, y1); c.lineTo(x2, y2); c.stroke();
  }
  grid(x, y, w, h, columns = 10, rows = 4) {
    for (let i = 1; i < columns; i++) this.line(x + w * i / columns, y, x + w * i / columns, y + h, '#3d3f426f');
    for (let i = 0; i <= rows; i++) this.line(x, y + h * i / rows, x + w, y + h * i / rows, '#3d3f426f');
  }
  trace(x, y, w, h, color, fn, filled = false) {
    const c = this.ctx; c.beginPath();
    for (let i = 0; i < 100; i++) {
      const a = i / 99, px = x + a * w, py = y + h / 2 - clamp(fn(a), -1, 1) * h * 0.47;
      if (!i) c.moveTo(px, py); else c.lineTo(px, py);
    }
    if (filled) {
      c.save(); c.lineTo(x + w, y + h); c.lineTo(x, y + h); c.closePath();
      const gradient = c.createLinearGradient(0, y, 0, y + h);
      gradient.addColorStop(0, color + '44'); gradient.addColorStop(1, color + '00');
      c.fillStyle = gradient; c.fill(); c.restore();
      c.beginPath();
      for (let i = 0; i < 100; i++) {
        const a = i / 99, py = y + h / 2 - clamp(fn(a), -1, 1) * h * 0.47;
        if (!i) c.moveTo(x, py); else c.lineTo(x + a * w, py);
      }
    }
    c.strokeStyle = color; c.lineWidth = 1.25; c.stroke();
  }
  button(x, y, w, h, label, type, id = 0, active = false, color = colors.cyan) {
    this.panel(x, y, w, h, active ? '#26363c' : colors.header);
    this.text(label, x + 3, y, w - 6, h, active ? color : colors.muted, 11, 'center', active);
    this.add(type, id, x, y, w, h, label);
  }
  dial(x, y, value, color, radius, active = false, modulation = 0) {
    const c = this.ctx, start = Math.PI * 0.7, end = Math.PI * 2.3;
    c.lineCap = 'round'; c.lineWidth = 1.6; c.strokeStyle = colors.hair;
    c.beginPath(); c.arc(x, y, radius, start, end); c.stroke();
    const angle = start + (end - start) * clamp(value);
    c.strokeStyle = color; c.beginPath(); c.arc(x, y, radius, start, angle); c.stroke();
    if (Math.abs(modulation) > 0.002) {
      const target = start + (end - start) * clamp(value + modulation);
      c.strokeStyle = sourceColor(this.state.selectedSource);
      c.beginPath(); c.arc(x, y, radius + 3, Math.min(angle, target), Math.max(angle, target)); c.stroke();
    }
    const gradient = c.createLinearGradient(x, y - radius, x, y + radius);
    gradient.addColorStop(0, active ? '#b8b9ba' : '#686a6d'); gradient.addColorStop(1, active ? '#85888c' : '#484a4d');
    c.fillStyle = gradient; c.strokeStyle = '#0008';
    c.beginPath(); c.arc(x, y, radius - 3.2, 0, Math.PI * 2); c.fill(); c.stroke();
    this.line(x + Math.cos(angle) * radius * 0.44, y + Math.sin(angle) * radius * 0.44,
      x + Math.cos(angle) * (radius - 5.8), y + Math.sin(angle) * (radius - 5.8), active ? colors.base : colors.ink, 1.4);
  }
  knob(x, y, id, label, color = colors.cyan, radius = 16) {
    const state = this.state;
    const route = state.routes.find(r => r[0] === state.selectedSource && r[1] === id);
    const armed = state.armedSource >= 0 && this.meta.params[id].modTarget;
    this.dial(x, y, state.values[id], armed ? sourceColor(state.armedSource) : color, radius, state.dragParam === id, route ? route[2] : 0);
    this.text(label, x - 44, y + radius + 3, 88, 14, colors.muted, 11);
    this.text(this.valueText(id), x - 44, y + radius + 17, 88, 12, colors.dim, 9.5);
    this.add('param', id, x - radius - 10, y - radius - 6, radius * 2 + 20, radius * 2 + 37, label);
  }
  choiceText(id, value) {
    const P = this.P;
    if (id === P.aType || id === P.bType) return engineNames[value];
    if (id === P.f1Type || id === P.f2Type) return filterNames[value];
    if ([P.lfo1Shape, P.lfo2Shape, P.lfo3Shape].includes(id)) return waveNames[value];
    if (id === P.seqScale) return scaleNames[value];
    if (id === P.seqDivision) return ['1/4', '1/8', '1/16'][value];
    if (id === P.polyphony) return 'Poly ' + (4 + value * 4);
    return String(value + 1);
  }
  valueText(id) {
    const P = this.P, v = this.state.values[id];
    if ([P.f1Cut, P.f2Cut].includes(id)) {
      const hz = 28 * 2 ** (v * 9.48);
      return hz < 1000 ? Math.round(hz) + ' Hz' : (hz / 1000).toFixed(2) + ' kHz';
    }
    if (id >= P.attack && id <= P.env3Release && (id - P.attack) % 4 !== 2) {
      const t = 0.003 * 2 ** (v * 12);
      return t < 0.1 ? Math.round(t * 1000) + ' ms' : t.toFixed(2) + ' s';
    }
    if ([P.lfo1Rate, P.lfo2Rate, P.lfo3Rate, P.randomRate].includes(id)) return (0.018 * 2 ** (v * 10.2)).toFixed(2) + ' Hz';
    if (id === P.delayTime) return Math.round((0.027 + v * v * 1.6) * 1000) + ' ms';
    if (id === P.chorusRate) return (0.032 + v * v * 3.9).toFixed(2) + ' Hz';
    if (id === P.seqTempo) return Math.round(40 + v * 160) + ' BPM';
    if (id === P.seqRoot) return noteName(24 + pick(v, 73));
    if (id === P.seqLength) return (pick(v, 16) + 1) + ' steps';
    if (id === P.filterRoute) return v < 0.015 ? 'Series' : v > 0.985 ? 'Parallel' : Math.round(v * 100) + '% par.';
    if (id === P.voicePan) return sign(Math.round(v * 200 - 100));
    if (id < P.subLevel) {
      const offset = id % P.bType, type = pick(this.state.values[Math.floor(id / P.bType) * P.bType], 6);
      if (offset === P.aTune) return sign(pick(v, 49) - 24) + ' st';
      if (offset === P.aFine) return sign(Math.round(v * 100 - 50)) + ' ct';
      if (type === 4 && offset === P.aColor) return v >= 0.5 ? 'Loop' : 'One-shot';
      if (type === 5 && offset === P.aColor) return Math.round(24 + v * v * 290) + ' ms';
      if (type === 2 && offset === P.aShape) return String(2 + Math.floor(v * 10.999));
    }
    if (id >= P.stepBase && (id - P.stepBase) % 4 === 1) return sign(pick(v, 49) - 24) + ' st';
    const info = this.meta.params[id];
    return info.choices > 1 ? this.choiceText(id, pick(v, info.choices)) : Math.round(v * 100) + '%';
  }
  draw() {
    const c = this.ctx;
    c.setTransform(this.ratio * this.scale, 0, 0, this.ratio * this.scale, 0, 0);
    this.hits = []; this.rect(0, 0, 1240, 758, colors.base, 0);
    this.top();
    [() => this.play(), () => this.synth(), () => this.effects(), () => this.sequence()][this.state.page]();
    this.modulation(); this.lower(); this.footer();
  }
  top() {
    const s = this.state, P = this.P;
    this.panel(4, 3, 1232, 42);
    this.text('☰', 10, 8, 37, 32, colors.ink, 17); this.add('menu', 0, 8, 5, 40, 38);
    this.text('MICA FIELD', 54, 5, 160, 37, colors.cyan, 19, 'left');
    this.line(171, 16, 171, 32); this.text('IPAD INSTRUMENT', 184, 6, 130, 37, colors.dim, 9.5, 'left', true);
    this.panel(441, 10, 316, 28, colors.header);
    this.text('▥', 446, 10, 34, 28, colors.muted, 17);
    this.text(s.name + (s.dirty ? ' *' : ''), 484, 10, 209, 28, colors.ink, 14);
    this.button(696, 10, 29, 28, '‹', 'previous');
    this.button(726, 10, 26, 28, '›', 'next');
    ['PLAY', 'SYNTH', 'FX', 'SEQ'].forEach((name, i) => this.button(780 + i * 57, 12, 52, 24, name, 'page', i, s.page === i, i === 3 && s.sequence ? colors.mint : colors.cyan));
    this.text(s.recording ? '■' : '●', 1021, 11, 26, 26, s.recording ? colors.pink : colors.muted, 14);
    this.text(s.recording ? Math.floor(s.recordSeconds) + ' s' : 'REC', 1046, 11, 33, 26, s.recording ? colors.pink : colors.dim, 9.5);
    this.add('record', 0, 1018, 5, 66, 38);
    this.dial(1102, 24, s.values[P.master], colors.cyan, 15);
    this.add('param', P.master, 1083, 5, 38, 38, 'Master volume');
    this.rect(1126, 11, 3, 26, colors.base, 0);
    const height = clamp(Math.sqrt(s.peak) * 1.36) * 26;
    this.rect(1126, 37 - height, 3, height, colors.cyan, 0);
    this.text(s.audio ? 'AUDIO ON' : 'ENABLE', 1144, 9, 80, 30, s.audio ? colors.cyan : colors.muted, 10, 'center', true);
    this.add('audio', 0, 1140, 4, 92, 40);
  }
  synth() {
    const s = this.state, P = this.P, layer = s.layer * P.bType, v = s.values;
    this.panel(4, 49, 760, 321);
    for (let i = 0; i < 2; i++) {
      const x = 6 + i * 296, active = s.layer === i;
      this.panel(x, 51, 294, 26, active ? colors.raised : colors.header);
      if (active) this.rect(x, 51, 294, 1.2, colors.cyan, 0);
      this.text('ENGINE ' + (i + 1), x + 34, 52, 76, 24, active ? colors.ink : colors.muted, 11, 'left', true);
      this.add('layer', i, x, 51, 294, 26);
      this.text('⏻', x + 4, 51, 28, 26, v[i * P.bType + P.aLevel] ? colors.cyan : colors.dim, 14);
      this.add('power', i * P.bType + P.aLevel, x + 4, 51, 28, 26);
      this.text(engineNames[pick(v[i * P.bType], 6)] + '   ⌄', x + 114, 54, 164, 20, active ? colors.ink : colors.muted, 12, 'left');
      this.add('choice', i * P.bType, x + 110, 51, 180, 26, 'Engine type');
    }
    const type = pick(v[layer], 6);
    this.text('TUNE', 25, 86, 57, 18, colors.ink, 10.5, 'center', true);
    this.line(97, 95, 97, 260);
    this.knob(53, 136, layer + P.aTune, 'Coarse'); this.knob(53, 211, layer + P.aFine, 'Fine');
    const family = type === 0 ? ['Sine', 'Silk', 'Reed', 'Vowel', 'Alloy', 'Fracture'][pick(v[layer + P.aShape], 6)] : engineNames[type];
    this.text(family.toUpperCase(), 113, 87, 196, 19, colors.cyan, 10.5, 'left', true);
    if (type >= 4) this.button(377, 84, 201, 22, (s.clips[s.layer]?.name || 'Weathered Tape') + '  ↓', 'sample', s.layer);
    this.engineGraph(112, 110, 466, 154, type, layer);
    this.line(12, 275, 593, 275);
    this.text('SHAPE', 16, 277, 270, 15, colors.muted, 9.5, 'center', true);
    this.text('OUTPUT', 292, 277, 298, 15, colors.muted, 9.5, 'center', true); this.line(288, 279, 288, 360);
    [62, 150, 237].forEach((x, i) => this.knob(x, 319, layer + P.aShape + i, engineControls[type][i]));
    this.knob(340, 319, layer + P.aWidth, 'Stereo'); this.knob(441, 319, layer + P.aLevel, 'Volume'); this.knob(543, 319, layer + P.aSend, 'Filter Mix');
    this.panel(598, 51, 164, 317, colors.raised); this.panel(599, 51, 162, 26, colors.header);
    this.text('UTILITY ENGINE', 606, 52, 149, 24, colors.ink, 11, 'center', true);
    this.text('SUB', 605, 89, 73, 16, colors.muted, 10.5); this.text('NOISE', 681, 89, 73, 16, colors.muted, 10.5);
    this.trace(615, 107, 54, 30, colors.cyan, x => Math.sin(x * Math.PI * 2));
    this.trace(690, 107, 53, 30, colors.cyan, x => Math.sin(x * 131) * Math.cos(x * 57) * 0.76);
    this.knob(642, 163, P.subLevel, 'Volume', colors.cyan, 14); this.knob(719, 163, P.noiseLevel, 'Volume', colors.cyan, 14);
    this.knob(642, 230, P.subShape, 'Shape', colors.cyan, 14); this.knob(719, 230, P.noiseColor, 'Color', colors.cyan, 14);
    this.line(608, 274, 752, 274); this.knob(681, 314, P.utilitySend, 'Filter Mix', colors.cyan, 15);
    this.filter(0, 768, 49); this.filter(1, 768, 211);
    this.panel(1132, 49, 104, 321);
    this.text('FILTER ROUTING', 1137, 54, 94, 18, colors.ink, 10.5, 'center', true);
    [[P.filterRoute, 'Routing', 93], [P.filterBlend, 'F1 / F2 Blend', 164], [P.ampVelocity, 'Amp Mod', 232], [P.voicePan, 'Voice Pan', 306]]
      .forEach(([id, label, y]) => this.knob(1184, y, id, label, colors.cyan, 14));
    [141, 209, 279].forEach(y => this.line(1141, y, 1227, y));
  }
  engineGraph(x, y, w, h, type, offset) {
    const v = this.state.values, P = this.P, a = v[offset + P.aShape], b = v[offset + P.aColor], m = v[offset + P.aMotion];
    this.rect(x, y, w, h, '#202123');
    if (type === 0) {
      const tables = this.meta.waves;
      const wave = (frame, phase) => {
        phase = ((phase % 1) + 1) % 1;
        if (!tables) return Math.sin(phase * Math.PI * 2);
        const row = tables[clamp(frame, 0, 5)], pos = phase * (row.length - 1), index = Math.floor(pos);
        return row[index] + (row[Math.min(row.length - 1, index + 1)] - row[index]) * (pos - index);
      };
      for (let i = 14; i >= 0; i--) {
        const z = i / 14, position = clamp((a + z * 0.53 + this.state.sources[6] * 0.025) * 5, 0, 5), frame = Math.floor(position), blend = position - frame;
        this.ctx.globalAlpha = (1 - z) * 0.67 + 0.18;
        this.trace(x + 34 + 139 * z, y + 72 - 57 * z, w - 205 - 17 * z, 65 - 18 * z, colors.cyan, t => {
          const phase = t + b * 0.115 * Math.sin(Math.PI * 2 * t + m) + z * 0.026;
          return wave(frame, phase) * (1 - blend) + wave(frame + 1, phase) * blend;
        }, i % 2 === 0);
      }
      this.ctx.globalAlpha = 1;
      this.text('01', x + 5, y + h - 17, 29, 14, colors.dim, 9);
      this.text('FRAME ' + (pick(a, 6) + 1), x + w - 86, y + h - 18, 82, 16, colors.cyan, 9.5); return;
    }
    x += 12; y += 16; w -= 24; h -= 32; this.grid(x, y, w, h, 12, 4);
    if (type >= 4) {
      const preview = this.state.clips[this.state.layer]?.preview;
      const start = (type === 4 ? a * 0.91 : a) * w;
      this.rect(x + start, y, Math.min(w - start, w * (type === 5 ? 0.06 + b * 0.16 : (1 - a * 0.91) * (0.11 + m * 0.89))), h, '#38c7f322', 0);
      for (let i = 0; i < 144; i++) {
        const value = preview ? preview[i] : Math.abs(Math.sin(i * 0.49) * Math.cos(i * 0.11)) * 0.7;
        const height = value * h * 0.48, at = x + (i + 0.5) * w / 144;
        this.line(at, y + h / 2 - height, at, y + h / 2 + height, '#38c7f399', 1.4);
      }
      this.line(x + start, y, x + start, y + h, colors.cyan);
      this.text(type === 5 ? 'GRAIN WINDOW' : b >= 0.5 ? 'LOOP REGION' : 'ONE SHOT', x + 4, y, 104, 16, colors.cyan, 9, 'left'); return;
    }
    if (type === 1) {
      for (let i = 2; i >= 0; i--) this.trace(x + 4, y + 14, w - 8, h - 28, colors.cyan, t => {
        const phase = (t * (2 + b * i * 0.2) + i * 0.105) % 1, shape = a * 3;
        const sine = Math.sin(phase * Math.PI * 2), triangle = 1 - 4 * Math.abs(phase - 0.5), saw = phase * 2 - 1, pulse = phase < 0.12 + m * 0.76 ? 0.8 : -0.8;
        return shape < 1 ? sine + (triangle - sine) * shape : shape < 2 ? triangle + (saw - triangle) * (shape - 1) : saw + (pulse - saw) * (shape - 2);
      }, i === 0);
      return;
    }
    const modal = type === 3, count = modal ? 6 : 2 + Math.floor(a * 10.999);
    for (let i = 0; i < count; i++) {
      const n = i + 1, u = modal ? (n / 7.4) ** (1.17 + a * 0.49) : n / 13;
      const height = h * (0.86 - b * 0.22) / n ** (modal ? 0.49 : 1.34 - b * 0.95);
      this.rect(x + u * w - 7, y + h - height, 14, height, '#38c7f344', 2);
      this.line(x + u * w - 7, y + h - height, x + u * w + 7, y + h - height, colors.cyan);
    }
  }
  filter(index, x, y) {
    const P = this.P, offset = index * (P.f2Type - P.f1Type), v = this.state.values;
    const mode = pick(v[P.f1Type + offset], 5), cutoff = v[P.f1Cut + offset], resonance = v[P.f1Res + offset];
    this.panel(x, y, 360, 158);
    this.text('⏻', x + 6, y + 3, 23, 22, mode === 4 ? colors.dim : colors.cyan, 14);
    this.add('filterPower', P.f1Type + offset, x + 6, y + 3, 23, 22);
    this.text('FILTER ' + (index + 1), x + 28, y + 4, 82, 20, colors.ink, 11, 'left', true);
    this.text(filterNames[mode].toUpperCase() + '  ⌄', x + 120, y + 4, 219, 20, colors.ink, 10.5, 'center', true);
    this.add('choice', P.f1Type + offset, x + 120, y + 4, 234, 22, 'Filter mode');
    this.knob(x + 34, y + 56, P.f1Cut + offset, 'Cutoff', colors.cyan, 14);
    this.knob(x + 86, y + 56, P.f1Res + offset, 'Resonance', colors.cyan, 14);
    this.knob(x + 34, y + 114, P.f1Drive + offset, 'Drive', colors.cyan, 13);
    this.knob(x + 325, y + 56, P.f1Level + offset, 'Volume', colors.cyan, 14);
    this.grid(x + 119, y + 37, 173, 101, 8, 4);
    this.trace(x + 119, y + 37, 173, 101, mode === 4 ? colors.dim : colors.cyan, f => {
      if (mode === 4) return 0.23;
      const n = (f - cutoff) * 8.2, turn = 1 / (1 + Math.exp(3 * n)), peak = Math.exp(-n * n * 2.6) * resonance * 0.89;
      return mode === 1 ? 0.55 - turn * 1.42 + peak : mode === 2 ? -0.84 + Math.exp(-n * n * 0.58) * (1.28 + resonance * 0.35)
        : mode === 3 ? 0.5 - Math.exp(-n * n * 1.2) * 1.22 : turn * 1.4 - 0.85 + peak;
    }, true);
    this.text('FREQ', x + 119, y + 140, 42, 12, colors.dim, 8.5, 'left', true);
  }
  effects() {
    const P = this.P, v = this.state.values;
    this.panel(4, 49, 1232, 321); this.panel(7, 52, 142, 315, colors.header);
    this.text('SIGNAL PATH', 10, 58, 133, 28, colors.ink, 11, 'center', true);
    const names = ['ORBIT CHORUS', 'DRIFT DELAY', 'SPACE REVERB'], mixes = [P.chorusMix, P.delayMix, P.roomMix];
    for (let i = 0; i < 3; i++) {
      const y = 89 + i * 85, x = 154 + i * 360, wet = mixes[i];
      this.line(77, y - 6, 77, y + 79); this.panel(19, y + 6, 116, 57, colors.raised);
      this.text(['INSERT A', 'INSERT B', 'SEND'][i], 23, y + 8, 108, 21, colors.muted, 10, 'center', true);
      this.text(names[i], 23, y + 29, 108, 24, colors.cyan, 10.5);
      this.panel(x, 51, 357, 317, colors.panel);
      this.text('⏻', x + 9, 54, 24, 24, v[wet] ? colors.cyan : colors.dim, 14);
      this.add('power', wet, x + 9, 54, 24, 24);
      this.text(names[i], x + 38, 55, 178, 24, colors.ink, 12, 'left', true);
      this.text(['STEREO', 'PING-PONG', 'DIFFUSE + OCTAVE'][i], x + 209, 56, 135, 24, colors.dim, 9.5, 'right', true);
      this.grid(x + 21, 100, 315, 140, 12, 4);
      if (i === 0) {
        this.trace(x + 21, 100, 315, 140, colors.cyan, t => Math.sin(t * 1.38 * Math.PI * 2) * v[P.chorusDepth], true);
        this.knob(x + 70, 292, P.chorusRate, 'Rate'); this.knob(x + 178, 292, P.chorusDepth, 'Depth'); this.knob(x + 286, 292, wet, 'Dry / Wet');
      } else if (i === 1) {
        for (let j = 0; j < 11; j++) {
          const px = x + 21 + 315 * (0.06 + j * (0.053 + v[P.delayTime] * 0.075));
          if (px >= x + 336) break;
          this.line(px, 231, px, 231 - 140 * 0.74 * (v[P.delayFeedback] * 0.8 + 0.17) ** j, colors.cyan, 2);
        }
        this.knob(x + 70, 292, P.delayTime, 'Time'); this.knob(x + 178, 292, P.delayFeedback, 'Feedback'); this.knob(x + 286, 292, wet, 'Dry / Wet');
      } else {
        this.trace(x + 21, 100, 315, 140, colors.cyan, t => {
          const tail = Math.exp(-t * (7 - 4.3 * v[P.roomSize]));
          return Math.sin(t * 297) * Math.cos(t * 97) * tail * (0.68 + 0.3 * Math.sin(t * 45)) + v[P.shimmer] * tail * 0.21 * Math.sin(t * 190);
        }, true);
        [[P.roomSize, 'Size', 53], [P.roomDamp, 'Damping', 136], [P.shimmer, 'Shimmer', 220], [wet, 'Send Mix', 303]].forEach(([id, label, at]) => this.knob(x + at, 292, id, label));
      }
      this.text(['MODULATED DELAY', 'CROSS-FEEDBACK', 'OCTAVE FEEDBACK NETWORK'][i], x + 20, 350, 314, 11, colors.dim, 8.5, 'center', true);
    }
  }
  sequence() {
    const P = this.P, s = this.state, v = s.values;
    this.panel(4, 49, 1232, 321); this.panel(6, 51, 1228, 74, colors.raised);
    this.button(19, 62, 29, 28, s.sequence ? '■' : '▶', 'sequence', 0, s.sequence, colors.mint);
    this.text('SEQUENCER', 56, 60, 117, 25, colors.ink, 12, 'left', true);
    this.text('INTERNAL CLOCK', 56, 87, 115, 15, colors.dim, 9.5, 'left', true);
    this.button(185, 65, 105, 28, 'GENERATE', 'generate', 0, false, colors.mint);
    [P.seqScale, P.seqDivision].forEach((id, i) => {
      const x = 311 + i * 129;
      this.text(i ? 'DIVISION' : 'SCALE', x, 57, 110, 16, colors.dim, 9, 'center', true);
      this.button(x, 75, 110, 28, this.valueText(id) + ' ⌄', 'choice', id);
    });
    [[P.seqTempo, 'Tempo', 607], [P.seqLength, 'Length', 704], [P.seqRoot, 'Root', 800], [P.seqSwing, 'Swing', 898], [P.seqGate, 'Gate', 996]]
      .forEach(([id, name, x]) => this.knob(x, 75, id, name, colors.cyan, 12));
    this.text(s.sequence ? 'RUNNING' : 'STOPPED', 1076, 63, 143, 26, s.sequence ? colors.mint : colors.muted, 11, 'center', true);
    this.text('TAP PLAY TO START', 1076, 89, 143, 18, colors.dim, 9);
    const rows = [[151, 29], [182, 71], [255, 46], [303, 54]], labels = ['GATE', 'NOTE', 'VELOCITY', 'CHANCE'];
    const length = pick(v[P.seqLength], 16) + 1, root = 24 + pick(v[P.seqRoot], 73);
    rows.forEach(([y, h], i) => { this.panel(10, y, 89, h, colors.header); this.text(labels[i], 14, y, 81, h, colors.muted, 10, 'center', true); });
    for (let i = 0; i < 16; i++) {
      const x = 103 + i * 69.9, inside = i < length, enabled = v[P.stepBase + i * 4] > 0.5 && inside;
      const current = s.sequence && s.step === i, color = current ? colors.mint : enabled ? colors.cyan : '#38c7f355';
      this.text(String(i + 1).padStart(2, '0'), x, 128, 67.9, 21, current ? colors.mint : inside ? colors.ink : colors.dim, 10.5);
      rows.forEach(([y, h], part) => {
        const id = P.stepBase + i * 4 + part, value = v[id];
        this.panel(x, y, 67.3, h, current ? '#284042' : inside && enabled ? colors.raised : colors.header);
        if (part === 0) {
          this.ctx.fillStyle = enabled ? color : colors.hair; this.ctx.beginPath(); this.ctx.arc(x + 33.6, y + h / 2, 4.5, 0, Math.PI * 2); this.ctx.fill();
          this.add('toggle', id, x, y, 67.3, h);
        } else {
          if (part === 1) {
            const at = y + h - 13 - value * 46;
            this.line(x + 9, at, x + 58, at, color); this.panel(x + 18.6, at - 8, 30, 16, colors.header);
            this.text(noteName(root + pick(value, 49) - 24), x + 18.6, at - 8, 30, 16, color, 10);
          } else {
            const at = y + h - 6 - (h - 12) * value;
            this.line(x + 8, at, x + 59.3, at, color, 1.35); this.text(Math.round(value * 100), x, y, 67.3, h, enabled ? colors.ink : colors.dim, 10);
          }
          this.add('param', id, x, y, 67.3, h, 'Step ' + (i + 1) + ' ' + labels[part].toLowerCase());
        }
      });
    }
  }
  play() {
    const s = this.state, preset = this.meta.factory[s.preset];
    this.panel(4, 49, 1232, 321); this.panel(8, 53, 847, 313, colors.header);
    this.text(preset?.family || 'USER PATCH', 32, 71, 760, 23, colors.cyan, 11, 'left', true);
    this.text(s.name, 29, 91, 775, 55, colors.ink, 37, 'left');
    this.text(preset?.description || 'Play a note or start the sequencer.', 33, 147, 752, 25, colors.muted, 13, 'left');
    this.grid(31, 187, 802, 130, 20, 6);
    if (s.animate) {
      for (let i = 4; i >= 0; i--) {
        this.ctx.globalAlpha = 0.89 - i * 0.16;
        this.trace(31 + i * 4, 187 + i * 3.7, 802 - i * 8, 130 - i * 7.4, colors.cyan, t =>
          clamp((s.scope[Math.floor(t * (s.scope.length - 1))] || 0) * (3.6 - i * 0.45) + i * 0.05 * Math.sin(Math.PI * 8 * t + s.time), -1, 1), i === 0);
      }
      this.ctx.globalAlpha = 1;
    } else this.text('VISUALIZER PAUSED', 31, 187, 802, 130, colors.dim, 11);
    this.text('STEREO FIELD / OUTPUT SCOPE', 32, 325, 300, 20, colors.dim, 9.5, 'left', true);
    this.button(704, 326, 129, 25, s.animate ? 'PAUSE VISUALS' : 'RESUME VISUALS', 'visual');
    this.panel(859, 53, 373, 313, colors.raised);
    this.text('PERFORMANCE MACROS', 866, 65, 359, 26, colors.ink, 12, 'center', true);
    for (let i = 0; i < 4; i++) this.macro(953 + (i % 2) * 181, 140 + Math.floor(i / 2) * 109, i, 31);
    this.button(882, 329, 103, 24, 'KEYBOARD', 'lower', 0, s.lower === 0);
    this.button(993, 329, 103, 24, 'HOLD', 'hold', 0, s.hold, colors.mint);
    this.button(1104, 329, 104, 24, 'PANIC', 'panic');
  }
  modulation() {
    const s = this.state, width = 1232 / 15;
    for (let i = 0; i < 15; i++) {
      const x = 4 + i * width, color = sourceColor(i);
      this.panel(x, 374, width - 2, 99, i === s.armedSource ? '#34313d' : i === s.selectedSource ? colors.raised : colors.panel);
      if (i === s.selectedSource) this.rect(x, 374, width - 2, 1.7, color, 0);
      this.text(sourceNames[i], x + 5, 379, width - 10, 20, color, 10.5, 'center', true);
      const value = s.sources[i] || 0;
      if (i < 6 || i > 10) {
        const amount = i > 10 ? s.values[this.P.macro1 + i - 11] : value;
        const y = 453 - Math.max(0.05, amount) * 45;
        this.rect(x + 8, y, width - 17, 453 - y, color + '22', 0); this.line(x + 8, y, x + width - 9, y, color);
      } else {
        this.trace(x + 8, 408, width - 17, 45, color, t => i < 9
          ? lfo(pick(s.values[this.P.lfo1Shape + (i - 6) * 2], 4), t + s.time * 0.18) * 0.65
          : clamp([-0.58, 0.16, -0.13, 0.57][Math.min(3, Math.floor(t * 4))] + value * 0.27, -0.88, 0.88));
      }
      const count = s.routes.filter(r => r[0] === i).length;
      if (count) this.text(count, x + width - 17, 456, 12, 12, colors.dim, 9);
      this.add('source', i, x, 374, width - 2, 99, sourceNames[i]);
    }
  }
  macro(x, y, index, radius = 22) {
    const id = this.P.macro1 + index;
    this.dial(x, y, this.state.values[id], colors.cyan, radius, this.state.dragParam === id);
    this.text(['Timbre', 'Motion', 'Distance', 'Space'][index], x - 44, y + radius + 4, 88, 15, colors.muted, 11);
    this.text(this.valueText(id), x - 44, y + radius + 19, 88, 12, colors.dim, 9.5);
    this.add('param', id, x - radius - 12, y - radius - 4, radius * 2 + 24, radius * 2 + 34, 'Macro ' + (index + 1));
  }
  lower() {
    const s = this.state, P = this.P, accents = [colors.pink, colors.orange, colors.gold, colors.violet, colors.mint];
    this.panel(4, 477, 1025, 249);
    ['KEYBOARD', 'ENVELOPES', 'LFO', 'RANDOM', 'ROUTES'].forEach((name, i) => {
      this.button(5 + i * 205, 478, 203, 25, name, 'lower', i, s.lower === i, accents[i]);
      this.rect(5 + i * 205, 478, 203, 1.25, accents[i], 0);
    });
    if (s.lower === 0) this.keyboard();
    if (s.lower === 1 || s.lower === 2) {
      const width = 1019 / 3;
      for (let i = 0; i < 3; i++) {
        const x = 11 + i * width, y = 511;
        if (i) this.line(x - 4, y + 13, x - 4, y + 194);
        const envelope = s.lower === 1, id = envelope ? P.attack + i * 4 : P.lfo1Rate + i * 2;
        const color = envelope ? colors.orange : colors.gold;
        this.text(envelope ? (i ? 'ENV ' + (i + 1) : 'ENV AMP') : 'LFO ' + (i + 1), x + 11, y + 4, 155, 23, color, 11, 'left', true);
        this.grid(x + 11, y + 39, width - 30, 79, 8, 4);
        if (envelope) {
          const v = s.values, a = 0.05 + v[id] * 0.19, d = 0.06 + v[id + 1] * 0.21, r = 0.06 + v[id + 3] * 0.28, sustain = v[id + 2];
          this.trace(x + 11, y + 39, width - 30, 79, color, t => {
            let value;
            if (t < a) { const p = t / a; value = p * p * (3 - 2 * p); }
            else if (t < a + d) { const p = (t - a) / d; value = 1 + (sustain - 1) * p * (2 - p); }
            else if (t < 1 - r) value = sustain;
            else { const p = (1 - t) / r; value = sustain * p * p; }
            return value * 2 - 1;
          }, true);
          ['Attack', 'Decay', 'Sustain', 'Release'].forEach((name, j) => this.knob(x + (j + 0.5) * (width - 8) / 4, y + 150, id + j, name, color, 15));
        } else {
          this.trace(x + 11, y + 39, width - 30, 79, color, t => lfo(pick(s.values[id + 1], 4), t * 1.9999) * 0.83, true);
          this.knob(x + 70, y + 151, id, 'Rate', color, 15);
          this.button(x + 123, y + 139, width - 152, 27, this.choiceText(id + 1, pick(s.values[id + 1], 4)) + ' ⌄', 'choice', id + 1);
        }
      }
    }
    if (s.lower === 3) {
      this.text('RANDOM / SAMPLE & HOLD', 23, 519, 461, 19, colors.violet, 11.5, 'left', true);
      this.grid(23, 549, 615, 116, 11, 4);
      this.trace(23, 549, 615, 116, colors.violet, t => [-0.45, 0.23, -0.66, 0.44, 0.7, -0.36, s.sources[9] || 0][Math.min(6, Math.floor(t * 7))], true);
      this.text('A BIPOLAR VALUE AT EACH CLOCK TICK', 23, 675, 400, 21, colors.dim, 10, 'left', true);
      this.knob(735, 593, P.randomRate, 'Rate', colors.violet, 19);
      this.text('CURRENT VALUE', 814, 555, 173, 23, colors.muted, 10);
      this.text((s.sources[9] || 0).toFixed(2), 814, 581, 173, 33, colors.violet, 22);
    }
    if (s.lower === 4) this.routes();
    this.panel(1033, 477, 203, 249, colors.raised); this.panel(1034, 478, 201, 25, colors.header);
    this.rect(1034, 478, 201, 1.3, colors.cyan, 0); this.text('MACROS', 1036, 479, 197, 23, colors.ink, 11, 'center', true);
    for (let i = 0; i < 4; i++) this.macro(1085 + (i % 2) * 100, 546 + Math.floor(i / 2) * 100, i);
  }
  keyboard() {
    const s = this.state;
    this.button(24, 516, 35, 23, '−', 'octave', -1); this.text('C' + s.octave, 62, 516, 42, 23, colors.ink, 11);
    this.button(107, 516, 35, 23, '+', 'octave', 1);
    this.text('TOUCH KEYS TO PLAY / HOLD LATCHES NOTES / TAP A HELD NOTE TO RELEASE', 159, 516, 715, 23, colors.muted, 10, 'left');
    this.button(884, 516, 60, 23, 'HOLD', 'hold', 0, s.hold, colors.mint);
    this.button(951, 516, 62, 23, 'PANIC', 'panic');
    const x = 23, y = 552, w = 987, h = 156, keyWidth = w / 15, first = (s.octave + 1) * 12;
    const black = note => [1, 3, 6, 8, 10].includes(note % 12);
    let white = 0;
    for (let note = first; note <= first + 24; note++) {
      if (black(note)) continue;
      const at = x + white++ * keyWidth, active = s.notes.has(note);
      const gradient = this.ctx.createLinearGradient(0, y, 0, y + h);
      gradient.addColorStop(0, active ? '#2999ba' : '#909395'); gradient.addColorStop(1, active ? colors.cyan : '#c5c7c8');
      this.rect(at, y, keyWidth - 1, h, gradient, 2.4);
      if (note % 12 === 0) this.text(noteName(note), at, y + h - 21, keyWidth - 1, 21, '#232629', 10.5);
      this.add('key', note, at, y, keyWidth - 1, h, noteName(note));
    }
    white = 0;
    for (let note = first; note <= first + 24; note++) {
      if (!black(note)) { white++; continue; }
      const at = x + (white - 0.31) * keyWidth;
      this.rect(at, y, keyWidth * 0.61, h * 0.6, s.notes.has(note) ? colors.cyan : '#141619', 2.4);
      this.add('key', note, at, y, keyWidth * 0.61, h * 0.6, noteName(note));
    }
  }
  routes() {
    const s = this.state;
    this.text('MODULATION ROUTES', 20, 511, 188, 21, colors.ink, 11, 'left', true);
    this.text('TAP A SOURCE, THEN A KNOB / DRAG DEPTH TO ADJUST', 232, 511, 500, 21, colors.dim, 9.5, 'left', true);
    for (let i = 0; i < 4; i++) {
      const slot = i + s.routeOffset, r = s.routes[slot], y = 540 + i * 38, selected = s.selectedRoute === slot;
      this.panel(19, y, 814, 32, selected && r[0] >= 0 ? '#34363b' : colors.header);
      this.text(String(slot + 1).padStart(2, '0'), 27, y, 33, 32, colors.dim, 10);
      if (r[0] < 0) { this.text('Empty', 68, y, 175, 32, colors.dim, 11, 'left'); continue; }
      const color = sourceColor(r[0]);
      this.rect(19, y, 2, 32, color, 0);
      this.text(sourceNames[r[0]], 67, y, 99, 32, color, 10.5, 'left', true);
      this.text('→', 172, y, 43, 32, colors.dim, 13);
      this.text(this.meta.params[r[1]].name, 231, y, 360, 32, colors.ink, 12, 'left');
      this.text(sign(Math.round(r[2] * 100)) + '%', 714, y, 73, 32, color, 11);
      this.add('routeSelect', slot, 19, y, 814, 32);
      this.button(796, y + 3, 30, 26, '×', 'routeClear', slot);
    }
    this.panel(847, 540, 165, 147, colors.raised);
    const r = s.routes[s.selectedRoute];
    if (r[0] >= 0) {
      const color = sourceColor(r[0]);
      this.text('ROUTE ' + String(s.selectedRoute + 1).padStart(2, '0'), 847, 540, 165, 25, color, 10.5);
      this.dial(929.5, 605, 0.5 + r[2] * 0.5, color, 25, s.dragRoute === s.selectedRoute);
      this.add('routeDepth', s.selectedRoute, 897.5, 573, 64, 90, 'Modulation depth');
      this.text('Depth', 847, 636, 165, 20, colors.ink, 11);
      this.text(Math.round(r[2] * 100) + '%', 847, 657, 165, 17, color, 10.5);
    } else this.text('Select a route', 847, 540, 165, 147, colors.dim, 11);
    this.button(20, 692, 28, 20, '‹', 'routeScroll', -1); this.text((s.routeOffset / 4 + 1) + ' / 4', 48, 692, 55, 20, colors.dim, 10.5);
    this.button(105, 692, 28, 20, '›', 'routeScroll', 1);
  }
  footer() {
    const s = this.state, P = this.P;
    this.panel(4, 730, 1232, 24, colors.header);
    const hint = s.armedSource >= 0 ? sourceNames[s.armedSource] + ' armed / Tap a knob to assign it' : s.hint || 'Drag knobs vertically / Tap a modulation source and then a knob';
    this.text(hint, 17, 731, 716, 22, colors.muted, 10.5, 'left');
    this.button(751, 732, 63, 19, 'HOLD', 'hold', 0, s.hold, colors.mint);
    this.button(819, 732, 60, 19, 'PANIC', 'panic');
    this.text(s.voices + ' / ' + (4 + pick(s.values[P.polyphony], 3) * 4) + ' voices', 886, 731, 91, 22, colors.muted, 10.5);
    this.button(980, 732, 77, 19, this.valueText(P.polyphony) + ' ⌄', 'choice', P.polyphony);
    this.text('16 MB ENGINE', 1066, 731, 104, 22, colors.dim, 9.5);
    this.text('IPAD 0.1', 1180, 731, 45, 22, colors.dim, 9);
  }
}
