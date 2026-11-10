// Steps a Yeocam Grid through tile counts in time with a song's BPM, then puts it back.
//
//   node examples/beat-party.mjs --bpm 128
//   node examples/beat-party.mjs --bpm 140 --every 2 --sequence 1,2,4,9,16 --loops 4 --flip
//   node examples/beat-party.mjs --bpm 128 --dry-run          (print the schedule, no OBS)
//
// Options
//   --bpm <n>          song tempo (default 120)
//   --every <beats>    beats per step (default 4 = one bar in 4/4)
//   --sequence <list>  tile counts to step through (default 1,2,3,4,6,9)
//   --loops <n>        times through the sequence; 0 = until Ctrl+C (default 2)
//   --end <n>          tile count to finish on (default 1)
//   --delay <ms>       wait before the first step, to line up with the drop (default 0)
//   --flip             toggle alternate-tile mirroring on every step
//   --grid <name>      grid source name (optional when there is only one)
//   --dry-run          print the timing and exit
//
// Timing is scheduled against the start time, so it doesn't drift over a long song.
// It's a fixed tempo, not beat detection: start it on the beat (or use --delay).

import { connectObs, parseArgs } from './obs-ws.mjs';

const a = parseArgs(process.argv.slice(2), {
  bpm: '120',
  every: '4',
  sequence: '1,2,3,4,6,9',
  loops: '2',
  end: '1',
  delay: '0',
  grid: '',
});

const bpm = Number(a.bpm);
const every = Number(a.every);
const loops = Number(a.loops);
const end = Number(a.end);
const delay = Number(a.delay);
const sequence = String(a.sequence).split(',').map((s) => Number(s.trim()));

const bad = [bpm, every, loops, end, delay, ...sequence].some((n) => !Number.isFinite(n));
if (bad || bpm <= 0 || every <= 0 || sequence.length === 0) {
  console.error('Check the options: --bpm, --every, --loops, --end, --delay must be numbers; --sequence like 1,2,4.');
  process.exit(1);
}

const stepMs = (60000 / bpm) * every;
const totalSteps = loops > 0 ? sequence.length * loops : Infinity;

if (a['dry-run']) {
  console.log(`${bpm} BPM, a step every ${every} beat(s) = ${stepMs.toFixed(1)} ms`);
  const shown = Math.min(totalSteps, sequence.length * 2);
  for (let i = 0; i < shown; i++) {
    const t = delay + i * stepMs;
    const mirror = a.flip ? (i % 2 ? ' mirror' : '') : '';
    console.log(`  ${(t / 1000).toFixed(2).padStart(7)}s  count ${sequence[i % sequence.length]}${mirror}`);
  }
  if (totalSteps > shown) console.log(loops > 0 ? `  ... ${totalSteps} steps in total` : '  ... repeats until Ctrl+C');
  console.log(`  then count ${end}`);
  process.exit(0);
}

const obs = await connectObs();

let grid = a.grid;
if (!grid) {
  const list = await obs.vendor('ListGrids');
  if (!list.success || list.grids.length !== 1) {
    console.error(list.grids?.length > 1 ? 'Several grids exist; pass --grid "<name>".' : 'No Yeocam Grid source found.');
    obs.close();
    process.exit(1);
  }
  grid = list.grids[0].sourceName;
}

const original = (await obs.call('GetInputSettings', { inputName: grid })).responseData.inputSettings;
const originalMirror = original.mirror ?? 0;

const apply = (settings) => obs.call('SetInputSettings', { inputName: grid, inputSettings: settings, overlay: true });

let finished = false;
async function finish() {
  if (finished) return;
  finished = true;
  await apply({ count: end, mirror: originalMirror });
  console.log(`\nBack to ${end} tile(s).`);
  obs.close();
  process.exit(0);
}
process.on('SIGINT', finish);

console.log(`"${grid}": ${bpm} BPM, step every ${every} beat(s) (${stepMs.toFixed(0)} ms). Ctrl+C to stop.`);

const t0 = performance.now() + delay;
for (let i = 0; i < totalSteps && !finished; i++) {
  const wait = t0 + i * stepMs - performance.now();
  if (wait > 0) await new Promise((r) => setTimeout(r, wait));
  if (finished) break;

  const count = sequence[i % sequence.length];
  const settings = { count };
  if (a.flip) settings.mirror = i % 2 ? 2 : 0;
  await apply(settings);
  process.stdout.write(`${count} `);
}

await new Promise((r) => setTimeout(r, Math.max(0, t0 + totalSteps * stepMs - performance.now())));
await finish();
