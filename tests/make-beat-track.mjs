// Writes a 40 s, 120 BPM test loop: 32 s of kick + hats + pad, then 8 s of silence.
// Load it into a looping Media Source and pick that as the grid's music source to
// check beat sync (2 steps per second) and the silence reset.
//
//   node tests/make-beat-track.mjs beat-test-120bpm.wav
import { writeFileSync } from 'node:fs';

const rate = 48000, seconds = 40, music = 32, bpm = 120;
const n = rate * seconds;
const pcm = Buffer.alloc(44 + n * 4);
let seed = 7;
const noise = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296) * 2 - 1;

for (let i = 0; i < n; i++) {
  const t = i / rate;
  let x = 0;
  if (t < music) {
    const beat = 60 / bpm, since = t % beat;
    const f = 55 + 60 * Math.exp(-since * 40);
    x += 0.7 * Math.sin(2 * Math.PI * f * since) * Math.exp(-since * 12);
    const eighth = beat / 2, s8 = t % eighth;
    x += 0.12 * noise() * Math.exp(-s8 * 80);
    x += 0.12 * (Math.sin(2 * Math.PI * 220 * t) + Math.sin(2 * Math.PI * 277.2 * t) + Math.sin(2 * Math.PI * 329.6 * t)) / 3;
  }
  const v = Math.max(-1, Math.min(1, x)) * 32767;
  pcm.writeInt16LE(v | 0, 44 + i * 4);
  pcm.writeInt16LE(v | 0, 46 + i * 4);
}

pcm.write('RIFF', 0); pcm.writeUInt32LE(36 + n * 4, 4); pcm.write('WAVE', 8);
pcm.write('fmt ', 12); pcm.writeUInt32LE(16, 16); pcm.writeUInt16LE(1, 20); pcm.writeUInt16LE(2, 22);
pcm.writeUInt32LE(rate, 24); pcm.writeUInt32LE(rate * 4, 28); pcm.writeUInt16LE(4, 32); pcm.writeUInt16LE(16, 34);
pcm.write('data', 36); pcm.writeUInt32LE(n * 4, 40);
writeFileSync(process.argv[2], pcm);
console.log('wrote', process.argv[2]);
