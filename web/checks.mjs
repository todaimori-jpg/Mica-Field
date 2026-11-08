// SPDX-License-Identifier: AGPL-3.0-only
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { performance } from 'node:perf_hooks';
import createMica from './app/engine.mjs';
import { encodePatch, decodePatch } from './app/patch.js';
const meta = JSON.parse(await readFile(new URL('./app/meta.json', import.meta.url)));
const binary = await readFile(new URL('./app/engine.wasm', import.meta.url));
const m = createMica({ wasmBinary: binary });
const P = meta.P;
m._mica_init(48000);
assert.equal(m.HEAPF32.byteLength, 16 * 1024 * 1024);
function render(blocks) {
  let peak = 0, energy = 0;
  for (let block = 0; block < blocks; block++) {
    m._mica_render(128);
    for (const ptr of [m._mica_left(), m._mica_right()]) {
      for (const value of m.HEAPF32.subarray(ptr >>> 2, (ptr >>> 2) + 128)) {
        assert.ok(Number.isFinite(value) && Math.abs(value) <= 1, 'Output must be finite and bounded');
        peak = Math.max(peak, Math.abs(value)); energy += value * value;
      }
    }
  }
  return { peak, energy };
}
// A note immediately after startup must sound without a silent priming block.
m._mica_note(1, 60, 0.81);
assert.ok(render(750).peak > 0.001, 'First note after startup is silent');
const started = performance.now();
const results = [];
for (let mode = 0; mode < 6; mode++) {
  m._mica_panic();
  meta.factory.at(-1).values.forEach((v, i) => m._mica_param(i, v));
  for (let i = 0; i < 16; i++) m._mica_route(i, -1, -1, 0);
  for (const [id, value] of [[P.aType, mode / 5], [P.aLevel, 0.7], [P.bLevel, 0],
    [P.subLevel, 0], [P.noiseLevel, 0], [P.attack, 0], [P.sustain, 1],
    [P.f1Type, 1], [P.f2Type, 1], [P.polyphony, 0]]) m._mica_param(id, value);
  m._mica_render(128);
  m._mica_note(1, 60, 0.81);
  const result = render(750);
  assert.ok(result.peak > 0.001, `Mode ${mode} is silent`);
  results.push({ mode, peak: +result.peak.toFixed(4) });
}
// Exercise both maximum-size custom clips inside the fixed heap, then granular playback.
const sample = Float32Array.from({ length: 384000 }, (_, i) => 0.45 * Math.sin(i * 2 * Math.PI * 220 / 48000));
for (let slot = 0; slot < 2; slot++) {
  const ptr = m._mica_clip_buffer(slot, sample.length);
  assert.ok(ptr > 0); m.HEAPF32.set(sample, ptr >>> 2); m._mica_clip_ready(slot);
}
m._mica_panic(); m._mica_note(1, 60, 0.81);
assert.ok(render(750).peak > 0.001, 'Custom granular sample is silent');
m._mica_panic();
meta.factory[8].values.forEach((v, i) => m._mica_param(i, v));
m._mica_sequence(1);
const steps = new Set();
let sequencePeak = 0;
for (let i = 0; i < 15; i++) {
  sequencePeak = Math.max(sequencePeak, render(100).peak);
  steps.add(m._mica_step());
}
assert.ok(sequencePeak > 0.001, 'Sequencer is silent');
assert.ok(steps.size > 2, 'Sequencer does not advance');
m._mica_panic(); assert.equal(render(1).peak, 0, 'Panic must clear notes and tails');
const state = { name: '海 · Mica interoperability', values: meta.factory[0].values,
  routes: meta.factory[0].routes.map(r => [...r]), clips: [{ name: '8s texture', samples: sample }, null] };
const packed = encodePatch(state), decoded = decodePatch(packed, meta);
assert.equal(decoded.name, state.name);
assert.equal(decoded.clips[0].samples.length, sample.length);
assert.equal(decoded.clips[1], null);
state.values.forEach((v, i) => assert.ok(Math.abs(v - decoded.values[i]) < 1e-6));
state.routes.forEach((route, i) => {
  assert.equal(decoded.routes[i][0], route[0]);
  if (route[0] >= 0) {
    assert.equal(decoded.routes[i][1], route[1]);
    assert.ok(Math.abs(decoded.routes[i][2] - route[2]) < 1 / 32767);
  }
});
assert.ok(Math.abs(decoded.clips[0].samples[100] - sample[100]) < 1 / 32767);
assert.throws(() => decodePatch(packed.slice(0, -1), meta));
const corrupt = packed.slice(0); new DataView(corrupt).setUint32(0, 0); assert.throws(() => decodePatch(corrupt, meta));
const extra = new Uint8Array(packed.byteLength + 1); extra.set(new Uint8Array(packed)); assert.throws(() => decodePatch(extra.buffer, meta));
console.log(JSON.stringify({ modes: results, heapMB: m.HEAPF32.byteLength / 1048576,
  renderedSeconds: 18, elapsedMS: Math.round(performance.now() - started), patchBytes: packed.byteLength,
  checks: 'startup, all six modes, maximum clips, sequence, panic, patch round-trip and invalid files' }, null, 2));
