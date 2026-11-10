// Applies a named layout preset from layouts.json to a Yeocam Grid.
//
//   node examples/set-layout.mjs duo
//   node examples/set-layout.mjs wall --grid "Yeocam Grid"
//   node examples/set-layout.mjs --list
//
// Good for Stream Deck "Open" / "Run" buttons: one button per preset.
// Set OBS_WS_PASSWORD so it runs without a prompt.

import { readFileSync } from 'node:fs';
import { connectObs, parseArgs } from './obs-ws.mjs';

const presets = JSON.parse(readFileSync(new URL('./layouts.json', import.meta.url), 'utf8'));
const args = parseArgs(process.argv.slice(2), { grid: '' });

if (args.list || args._.length === 0) {
  console.log('Presets:');
  for (const [name, settings] of Object.entries(presets)) console.log(`  ${name.padEnd(12)} ${JSON.stringify(settings)}`);
  process.exit(args.list ? 0 : 1);
}

const presetName = args._[0];
const preset = presets[presetName];
if (!preset) {
  console.error(`Unknown preset "${presetName}". Run with --list to see them.`);
  process.exit(1);
}

const obs = await connectObs();
try {
  let grid = args.grid;
  if (!grid) {
    const list = await obs.vendor('ListGrids');
    if (!list.success || list.grids.length !== 1) {
      console.error(list.grids?.length > 1 ? 'Several grids exist; pass --grid "<name>".' : 'No Yeocam Grid source found.');
      process.exit(1);
    }
    grid = list.grids[0].sourceName;
  }

  const res = await obs.call('SetInputSettings', { inputName: grid, inputSettings: preset, overlay: true });
  if (!res.requestStatus.result) {
    console.error(`OBS refused: ${res.requestStatus.comment}`);
    process.exit(1);
  }
  console.log(`"${grid}" -> ${presetName}`);
} finally {
  obs.close();
}
