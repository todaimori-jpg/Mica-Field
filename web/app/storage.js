// SPDX-License-Identifier: AGPL-3.0-only
let opening;
export function database() {
  if (!opening) opening = new Promise((resolve, reject) => {
    const request = indexedDB.open('mica-field-ipad', 1);
    request.onupgradeneeded = () => {
      const db = request.result;
      db.createObjectStore('settings');
      db.createObjectStore('audio', { keyPath: 'index' });
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
  return opening;
}
export async function readSetting(key) {
  const db = await database();
  return new Promise((resolve, reject) => {
    const r = db.transaction('settings').objectStore('settings').get(key);
    r.onsuccess = () => resolve(r.result);
    r.onerror = () => reject(r.error);
  });
}
export async function writeSetting(key, value) {
  const db = await database();
  return new Promise((resolve, reject) => {
    const tx = db.transaction('settings', 'readwrite');
    tx.objectStore('settings').put(value, key);
    tx.oncomplete = resolve;
    tx.onerror = () => reject(tx.error);
  });
}
export class RecordingStore {
  constructor() {
    this.queue = Promise.resolve();
    this.frames = 0;
    this.index = 0;
  }
  async begin(rate) {
    const db = await database();
    await new Promise((resolve, reject) => {
      const tx = db.transaction(['settings', 'audio'], 'readwrite');
      tx.objectStore('audio').clear();
      tx.objectStore('settings').put({ rate, date: Date.now(), status: 'recording', frames: 0 }, 'take');
      tx.oncomplete = resolve;
      tx.onerror = () => reject(tx.error);
    });
    this.frames = this.index = 0;
    this.queue = Promise.resolve();
  }
  append(buffer, frames) {
    this.frames += frames;
    const index = this.index++;
    this.queue = this.queue.then(async () => {
      const db = await database();
      await new Promise((resolve, reject) => {
        const tx = db.transaction('audio', 'readwrite');
        tx.objectStore('audio').put({ index, buffer });
        tx.oncomplete = resolve;
        tx.onerror = () => reject(tx.error);
      });
    });
    return this.queue;
  }
  async finish() {
    await this.queue;
    const take = await readSetting('take');
    take.status = 'ready';
    take.frames = this.frames;
    await writeSetting('take', take);
    return this.blob();
  }
  async blob() {
    const db = await database();
    const take = await readSetting('take');
    if (!take) return null;
    const rows = await new Promise((resolve, reject) => {
      const r = db.transaction('audio').objectStore('audio').getAll();
      r.onsuccess = () => resolve(r.result);
      r.onerror = () => reject(r.error);
    });
    const bytes = rows.reduce((n, row) => n + row.buffer.byteLength, 0);
    if (!bytes) return null;
    if (bytes > 0xffffffff - 44) throw new Error('This take exceeds the WAV file size limit.');
    const header = new ArrayBuffer(44);
    const view = new DataView(header);
    const word = (at, text) => [...text].forEach((c, i) => view.setUint8(at + i, c.charCodeAt(0)));
    word(0, 'RIFF'); view.setUint32(4, 36 + bytes, true); word(8, 'WAVE');
    word(12, 'fmt '); view.setUint32(16, 16, true); view.setUint16(20, 1, true);
    view.setUint16(22, 2, true); view.setUint32(24, take.rate, true);
    view.setUint32(28, take.rate * 4, true); view.setUint16(32, 4, true);
    view.setUint16(34, 16, true); word(36, 'data'); view.setUint32(40, bytes, true);
    return { blob: new Blob([header, ...rows.map(row => row.buffer)], { type: 'audio/wav' }), take, frames: bytes / 4 };
  }
}
