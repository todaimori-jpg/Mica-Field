// SPDX-License-Identifier: AGPL-3.0-only
const MAGIC = 0x3143494d;
export function encodePatch(state) {
  const encoder = new TextEncoder();
  const strings = [state.name, ...state.clips.filter(Boolean).map(c => c.name)]
    .map(s => encoder.encode(s.slice(0, 120)));
  const size = 8 + strings.reduce((n, s) => n + s.length + 1, 0)
    + state.values.length * 4 + 16 * 4 + 8
    + state.clips.reduce((n, clip) => n + (clip ? clip.samples.length * 2 : 0), 0);
  const buffer = new ArrayBuffer(size);
  const view = new DataView(buffer);
  let at = 0, stringIndex = 0;
  const int = v => { view.setUint32(at, v >>> 0, true); at += 4; };
  const text = () => {
    const bytes = strings[stringIndex++];
    new Uint8Array(buffer, at, bytes.length).set(bytes);
    at += bytes.length;
    view.setUint8(at++, 0);
  };
  int(MAGIC); int(1); text();
  state.values.forEach(v => { view.setFloat32(at, v, true); at += 4; });
  state.routes.forEach(([source, target, amount]) => {
    int(source < 0 ? 0 : (source + 1) | (target << 5) | (Math.round((Math.max(-1, Math.min(1, amount)) + 1) * 32767) << 13));
  });
  state.clips.forEach(clip => {
    int(clip ? clip.samples.length : 0);
    if (!clip) return;
    text();
    for (const v of clip.samples) { view.setInt16(at, Math.round(Math.max(-1, Math.min(1, v)) * 32767), true); at += 2; }
  });
  return buffer;
}
export function decodePatch(buffer, meta) {
  if (buffer.byteLength < 16 || buffer.byteLength > 2 * 1024 * 1024) throw new Error('Choose a compatible .mica patch.');
  const view = new DataView(buffer);
  let at = 0;
  const requireBytes = count => { if (at + count > buffer.byteLength) throw new Error('The patch file is incomplete.'); };
  const int = () => { requireBytes(4); const n = view.getUint32(at, true); at += 4; return n; };
  const text = () => {
    const begin = at;
    while (at < buffer.byteLength && view.getUint8(at) !== 0) at++;
    if (at === buffer.byteLength) throw new Error('The patch name is incomplete.');
    const value = new TextDecoder('utf-8', { fatal: true }).decode(new Uint8Array(buffer, begin, at - begin));
    at++;
    if (value.length > 120) throw new Error('The patch name is too long.');
    return value;
  };
  if (int() !== MAGIC || int() !== 1) throw new Error('Choose a Mica Field .mica patch.');
  const name = text();
  if (!name) throw new Error('The patch needs a name.');
  const values = meta.params.map(() => {
    requireBytes(4);
    const value = view.getFloat32(at, true); at += 4;
    if (!Number.isFinite(value) || value < 0 || value > 1) throw new Error('The patch contains an invalid value.');
    return value;
  });
  const routes = Array.from({ length: 16 }, () => {
    const bits = int(), source = (bits & 31) - 1, target = (bits >>> 5) & 255;
    const amount = ((bits >>> 13) & 65535) / 32767 - 1;
    if (source < 0) return [-1, -1, 0];
    if (source > 14 || !meta.params[target]?.modTarget) throw new Error('The patch contains an invalid route.');
    return [source, target, Math.max(-1, Math.min(1, amount))];
  });
  const clips = Array.from({ length: 2 }, () => {
    const frames = int();
    if (!frames) return null;
    if (frames < 3 || frames > 384000) throw new Error('The imported clip exceeds the sample limit.');
    const name = text();
    requireBytes(frames * 2);
    const samples = new Float32Array(frames);
    for (let i = 0; i < frames; i++) { samples[i] = Math.max(-1, view.getInt16(at, true) / 32767); at += 2; }
    return { name, samples };
  });
  if (at !== buffer.byteLength) throw new Error('The patch contains unexpected extra data.');
  return { name, values, routes, clips, preset: -1, dirty: false };
}
export async function saveFile(blob, name) {
  const file = new File([blob], name, { type: blob.type || 'application/octet-stream' });
  const iOS = /iPad|iPhone|iPod/.test(navigator.userAgent)
    || navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1;
  if (iOS && navigator.canShare?.({ files: [file] })) {
    await navigator.share({ files: [file], title: name });
    return;
  }
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url; link.download = name; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
}
