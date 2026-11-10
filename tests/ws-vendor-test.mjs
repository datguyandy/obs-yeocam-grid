// Live test for the "yeocam" obs-websocket vendor requests.
//
//   node tests/ws-vendor-test.mjs [grid source name]
//
// Needs OBS running with Tools -> WebSocket Server Settings enabled and at least
// one Yeocam Grid source. The password is read from OBS_WS_PASSWORD or typed at
// the prompt (input is hidden). Port: OBS_WS_PORT, default 4455.
// The grid's original count, fit and anchor are restored at the end.

import { createHash } from 'node:crypto';
import { createInterface } from 'node:readline';

const port = process.env.OBS_WS_PORT ?? '4455';
const gridName = process.argv[2] ?? '';

function askHidden(question) {
  return new Promise((resolve) => {
    const rl = createInterface({ input: process.stdin, output: process.stdout, terminal: true });
    rl._writeToOutput = (s) => {
      if (s.startsWith(question)) rl.output.write(question);
    };
    rl.question(question, (answer) => {
      rl.close();
      process.stdout.write('\n');
      resolve(answer);
    });
  });
}

const sha256b64 = (s) => createHash('sha256').update(s).digest('base64');

function connect(password) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(`ws://127.0.0.1:${port}`);
    const pending = new Map();
    let nextId = 1;

    const call = (requestType, requestData) =>
      new Promise((res) => {
        const requestId = String(nextId++);
        pending.set(requestId, res);
        ws.send(JSON.stringify({ op: 6, d: { requestType, requestId, requestData } }));
      });

    ws.onerror = () => reject(new Error(`Could not connect to ws://127.0.0.1:${port}`));
    ws.onclose = (e) => reject(new Error(`Connection closed (${e.code}${e.reason ? `: ${e.reason}` : ''})`));
    ws.onmessage = (event) => {
      const msg = JSON.parse(event.data);
      if (msg.op === 0) {
        const identify = { rpcVersion: 1 };
        if (msg.d.authentication) {
          const { salt, challenge } = msg.d.authentication;
          identify.authentication = sha256b64(sha256b64(password + salt) + challenge);
        }
        ws.send(JSON.stringify({ op: 1, d: identify }));
      } else if (msg.op === 2) {
        resolve({ ws, call });
      } else if (msg.op === 7) {
        pending.get(msg.d.requestId)?.(msg.d);
        pending.delete(msg.d.requestId);
      }
    };
  });
}

let failures = 0;
function check(label, cond, detail) {
  console.log(`${cond ? 'PASS' : 'FAIL'}  ${label}${detail ? `  (${detail})` : ''}`);
  if (!cond) failures++;
}

const password = process.env.OBS_WS_PASSWORD ?? (await askHidden('OBS WebSocket password: '));
const { ws, call } = await connect(password);
console.log(`Connected to OBS on port ${port}\n`);

const vendor = async (requestType, data = {}) => {
  const res = await call('CallVendorRequest', {
    vendorName: 'yeocam',
    requestType,
    requestData: gridName ? { sourceName: gridName, ...data } : data,
  });
  if (!res.requestStatus.result) return { success: false, error: res.requestStatus.comment };
  return res.responseData.responseData;
};

// ListGrids
const list = await vendor('ListGrids');
check('ListGrids returns grids', list.success && list.grids?.length > 0, JSON.stringify(list.grids));
const name = gridName || list.grids?.[0]?.sourceName;
if (!name) {
  console.log('\nNo Yeocam Grid source found. Add one in OBS first.');
  ws.close();
  process.exit(1);
}

// Remember original settings via the core API, so they can be restored.
const before = await call('GetInputSettings', { inputName: name });
const orig = before.responseData.inputSettings;
const origCount = orig.count ?? 4;
const origFit = orig.fit ?? 0;
const origAnchor = orig.anchor ?? 4;
console.log(`Testing "${name}" (count ${origCount}, fit ${origFit}, anchor ${origAnchor})\n`);

const named = (data = {}) => (gridName ? data : { sourceName: name, ...data });
const v = (type, data) => vendor(type, named(data));

let r = await v('SetCount', { count: 6 });
check('SetCount 6', r.success && r.count === 6, JSON.stringify(r));

r = await v('AdjustCount', { delta: 2 });
check('AdjustCount +2 -> 8', r.success && r.count === 8, JSON.stringify(r));

r = await v('AdjustCount', { delta: -1 });
check('AdjustCount -1 -> 7', r.success && r.count === 7, JSON.stringify(r));

r = await v('AdjustCount');
check('AdjustCount default delta -> 8', r.success && r.count === 8, JSON.stringify(r));

r = await v('SetCount', { count: 999 });
check('SetCount clamps to 64', r.success && r.count === 64, JSON.stringify(r));

r = await v('SetCount', { count: 0 });
check('SetCount clamps to 1', r.success && r.count === 1, JSON.stringify(r));

r = await v('SetCount', {});
check('SetCount without count fails', r.success === false, JSON.stringify(r));

const fit1 = await v('ToggleFit');
const fit2 = await v('ToggleFit');
check('ToggleFit flips and flips back', fit1.success && fit2.success && fit1.fit !== fit2.fit,
  `${fit1.fit} -> ${fit2.fit}`);

r = await v('SetAnchor', { anchor: 'top_left' });
check('SetAnchor top_left', r.success && r.anchor === 'top_left', JSON.stringify(r));

r = await v('NextAnchor');
check('NextAnchor top_left -> top', r.success && r.anchor === 'top', JSON.stringify(r));

r = await v('SetAnchor', { anchor: 'sideways' });
check('SetAnchor rejects unknown name', r.success === false, r.error);

r = await vendor('AdjustCount', { sourceName: 'definitely-not-a-source' });
check('Unknown sourceName fails', r.success === false, r.error);

// The core API should see the same values the vendor requests wrote.
await v('SetCount', { count: 5 });
const after = await call('GetInputSettings', { inputName: name });
check('SetInputSettings view agrees (count 5)', after.responseData.inputSettings.count === 5);

// Restore.
await call('SetInputSettings', {
  inputName: name,
  inputSettings: { count: origCount, fit: origFit, anchor: origAnchor },
  overlay: true,
});
console.log(`\nRestored count ${origCount}, fit ${origFit}, anchor ${origAnchor}.`);
console.log(failures ? `\n${failures} check(s) failed` : '\nAll vendor request checks passed');

ws.close();
process.exit(failures ? 1 : 0);
