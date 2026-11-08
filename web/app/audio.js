// SPDX-License-Identifier: AGPL-3.0-only
import { RecordingStore } from './storage.js';
export class AudioEngine extends EventTarget {
  constructor() {
    super();
    this.context = null;
    this.node = null;
    this.pending = [];
    this.recording = false;
    this.recordStore = new RecordingStore();
    this.ready = false;
  }
  async start() {
    if (this.ready) {
      await this.context.resume();
      return;
    }
    if (this.starting) return this.starting;
    const Context = window.AudioContext || window.webkitAudioContext;
    if (!Context || !window.AudioWorkletNode) throw new Error('This browser needs AudioWorklet support. Open this page in Safari.');
    try { this.context = new Context({ latencyHint: 'interactive', sampleRate: 48000 }); }
    catch { this.context = new Context({ latencyHint: 'interactive' }); }
    const resume = this.context.resume();
    this.context.onstatechange = () => this.dispatchEvent(new Event('statechange'));
    this.starting = (async () => {
      try {
        const [binary] = await Promise.all([
          fetch(new URL('./engine.wasm', import.meta.url)).then(r => {
            if (!r.ok) throw new Error('The sound engine could not be downloaded.');
            return r.arrayBuffer();
          }),
          this.context.audioWorklet.addModule(new URL('./processor.js', import.meta.url)),
          resume
        ]);
        this.node = new AudioWorkletNode(this.context, 'mica-field', {
          numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2], processorOptions: { binary }
        });
        await new Promise((resolve, reject) => {
          const timeout = setTimeout(() => reject(new Error('The audio engine took too long to start. Tap Enable Audio again.')), 15000);
          this.node.port.onmessage = e => {
            const msg = e.data;
            if (msg.type === 'ready') { clearTimeout(timeout); resolve(); }
            if (msg.type === 'error') { clearTimeout(timeout); reject(new Error(msg.message)); }
            this.message(msg);
          };
          this.node.onprocessorerror = () => {
            clearTimeout(timeout);
            const error = new Error('The audio engine stopped. Reload the page to restart it.');
            reject(error);
            this.dispatchEvent(new CustomEvent('error', { detail: error.message }));
          };
        });
        this.node.connect(this.context.destination);
        this.ready = true;
        this.pending.forEach(msg => this.node.port.postMessage(msg));
        this.pending.length = 0;
        this.dispatchEvent(new Event('statechange'));
      } catch (error) {
        this.ready = false;
        await this.context.close().catch(() => {});
        this.node = null;
        throw error;
      } finally { this.starting = null; }
    })();
    return this.starting;
  }
  message(msg) {
    if (msg.type === 'meter') this.dispatchEvent(new CustomEvent('meter', { detail: msg }));
    if (msg.type === 'pcm') {
      this.recordStore.append(msg.buffer, msg.frames).catch(error => {
        if (this.recording) this.send({ type: 'record', on: false });
        this.recording = false;
        this.dispatchEvent(new CustomEvent('error', { detail: 'Recording storage is full. The saved chunks can still be recovered.' }));
      });
    }
    if (msg.type === 'recorded') {
      this.recording = false;
      this.recordStore.finish().then(result => {
        this.dispatchEvent(new CustomEvent('recorded', { detail: result }));
      }).catch(error => this.dispatchEvent(new CustomEvent('error', { detail: error.message })));
    }
  }
  send(msg) {
    if (this.ready) this.node.port.postMessage(msg);
    else this.pending.push(msg);
  }
  patch(state) {
    this.send({ type: 'patch', values: state.values, routes: state.routes,
      clips: state.clips.map(clip => clip ? { samples: clip.samples } : null) });
  }
  async startRecording() {
    await this.start();
    await this.recordStore.begin(this.context.sampleRate);
    this.recording = true;
    this.send({ type: 'record', on: true });
  }
  stopRecording() {
    if (this.recording) this.send({ type: 'record', on: false });
  }
  async decode(file) {
    if (!this.ready) await this.start();
    if (file.size > 160 * 1024 * 1024) throw new Error('Choose an audio file smaller than 160 MB.');
    const buffer = await this.context.decodeAudioData(await file.arrayBuffer());
    const frames = Math.min(384000, Math.floor(buffer.duration * 48000));
    if (frames < 3) throw new Error('The sample is too short.');
    const samples = new Float32Array(frames);
    const a = buffer.getChannelData(0);
    const b = buffer.getChannelData(buffer.numberOfChannels > 1 ? 1 : 0);
    for (let i = 0; i < frames; i++) {
      const pos = i * buffer.sampleRate / 48000;
      const first = Math.min(Math.floor(pos), a.length - 1);
      const second = Math.min(first + 1, a.length - 1);
      const value = (a[first] + b[first]) * 0.5;
      const next = (a[second] + b[second]) * 0.5;
      samples[i] = Math.max(-1, Math.min(1, value + (next - value) * (pos - first)));
    }
    return { name: file.name.replace(/\.[^.]+$/, '').slice(0, 120), samples };
  }
}
