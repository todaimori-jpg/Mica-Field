// SPDX-License-Identifier: AGPL-3.0-only
import createMica from './engine.mjs';
class MicaProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.counter = 0;
    this.recording = false;
    this.recordFrames = 0;
    this.recordBuffer = new Int16Array(16384);
    try {
      this.module = createMica({ wasmBinary: options.processorOptions.binary });
      this.module._mica_init(sampleRate);
      this.port.onmessage = e => this.command(e.data);
      this.port.postMessage({ type: 'ready' });
    } catch (error) {
      this.port.postMessage({ type: 'error', message: error.message });
    }
  }
  command(msg) {
    const m = this.module;
    if (!m) return;
    if (msg.type === 'param') m._mica_param(msg.id, msg.value);
    if (msg.type === 'route') m._mica_route(msg.slot, ...msg.route);
    if (msg.type === 'note') m._mica_note(msg.on ? 1 : 0, msg.note, msg.velocity || 0.81);
    if (msg.type === 'sequence') m._mica_sequence(msg.on ? 1 : 0);
    if (msg.type === 'panic') m._mica_panic();
    if (msg.type === 'patch') {
      m._mica_panic();
      msg.values.forEach((v, i) => m._mica_param(i, v));
      msg.routes.forEach((route, slot) => m._mica_route(slot, ...route));
      msg.clips.forEach((clip, slot) => this.installClip(slot, clip));
    }
    if (msg.type === 'clip') this.installClip(msg.slot, msg.clip);
    if (msg.type === 'record') {
      if (msg.on) {
        this.recordFrames = 0;
        this.recording = true;
      } else {
        this.recording = false;
        this.flushRecord();
        this.port.postMessage({ type: 'recorded' });
      }
    }
  }
  installClip(slot, clip) {
    const m = this.module;
    if (!clip) return m._mica_clip_clear(slot);
    const pointer = m._mica_clip_buffer(slot, clip.samples.length);
    if (!pointer) throw new Error('The sample could not fit in the audio engine.');
    m.HEAPF32.set(clip.samples, pointer >>> 2);
    m._mica_clip_ready(slot);
  }
  flushRecord() {
    if (!this.recordFrames) return;
    const pcm = this.recordBuffer.slice(0, this.recordFrames * 2);
    this.port.postMessage({ type: 'pcm', frames: this.recordFrames, buffer: pcm.buffer }, [pcm.buffer]);
    this.recordFrames = 0;
  }
  process(inputs, outputs) {
    const output = outputs[0];
    if (!this.module || !output || !output[0]) return true;
    const m = this.module;
    let peak = 0;
    for (let offset = 0; offset < output[0].length; offset += 512) {
      const frames = Math.min(512, output[0].length - offset);
      m._mica_render(frames);
      const left = m.HEAPF32.subarray(m._mica_left() >>> 2, (m._mica_left() >>> 2) + frames);
      const right = m.HEAPF32.subarray(m._mica_right() >>> 2, (m._mica_right() >>> 2) + frames);
      output[0].set(left, offset);
      if (output[1]) output[1].set(right, offset);
      for (let i = 0; i < frames; i++) {
        peak = Math.max(peak, Math.abs(left[i]), Math.abs(right[i]));
        if (this.recording) {
          const at = this.recordFrames++ * 2;
          this.recordBuffer[at] = Math.round(Math.max(-1, Math.min(1, left[i])) * 32767);
          this.recordBuffer[at + 1] = Math.round(Math.max(-1, Math.min(1, right[i])) * 32767);
          if (this.recordFrames === this.recordBuffer.length / 2) this.flushRecord();
        }
      }
    }
    if (++this.counter % 20 === 0) {
      const count = Math.min(96, output[0].length);
      const scope = new Float32Array(count);
      for (let i = 0; i < count; i++) scope[i] = output[0][Math.floor(i * output[0].length / count)];
      const start = m._mica_sources() >>> 2;
      const sources = m.HEAPF32.slice(start, start + 15);
      this.port.postMessage({ type: 'meter', peak, voices: m._mica_voices(), step: m._mica_step(), scope, sources });
    }
    return true;
  }
}
registerProcessor('mica-field', MicaProcessor);
