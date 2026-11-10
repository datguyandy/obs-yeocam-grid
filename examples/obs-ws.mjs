// Minimal obs-websocket v5 client for the Yeocam Grid examples. Node 22+ (built-in WebSocket).
//
// Password: OBS_WS_PASSWORD, otherwise a hidden prompt. Port: OBS_WS_PORT (default 4455).

import { createHash } from 'node:crypto';
import { createInterface } from 'node:readline';

const sha256b64 = (s) => createHash('sha256').update(s).digest('base64');

export function askHidden(question) {
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

/** Connects and identifies. Returns { call, vendor, close }. */
export async function connectObs() {
  const port = process.env.OBS_WS_PORT ?? '4455';
  const password = process.env.OBS_WS_PASSWORD ?? (await askHidden('OBS WebSocket password: '));

  return new Promise((resolve, reject) => {
    const ws = new WebSocket(`ws://127.0.0.1:${port}`);
    const pending = new Map();
    let nextId = 1;
    let ready = false;

    const call = (requestType, requestData) =>
      new Promise((res, rej) => {
        if (ws.readyState !== WebSocket.OPEN) return rej(new Error('Not connected to OBS'));
        const requestId = String(nextId++);
        pending.set(requestId, res);
        ws.send(JSON.stringify({ op: 6, d: { requestType, requestId, requestData } }));
      });

    // Calls a "yeocam" vendor request and returns its responseData ({ success, ... }).
    const vendor = async (requestType, requestData = {}) => {
      const res = await call('CallVendorRequest', { vendorName: 'yeocam', requestType, requestData });
      if (!res.requestStatus.result) return { success: false, error: res.requestStatus.comment };
      return res.responseData.responseData;
    };

    ws.onerror = () => reject(new Error(`Could not connect to OBS on ws://127.0.0.1:${port}. Is the WebSocket server on?`));
    ws.onclose = (e) => {
      if (!ready) reject(new Error(`OBS closed the connection (${e.code}${e.reason ? `: ${e.reason}` : ''}). Wrong password?`));
    };
    ws.onmessage = (event) => {
      const msg = JSON.parse(event.data);
      if (msg.op === 0) {
        const identify = { rpcVersion: 1, eventSubscriptions: 0 };
        if (msg.d.authentication) {
          const { salt, challenge } = msg.d.authentication;
          identify.authentication = sha256b64(sha256b64(password + salt) + challenge);
        }
        ws.send(JSON.stringify({ op: 1, d: identify }));
      } else if (msg.op === 2) {
        ready = true;
        resolve({ call, vendor, close: () => ws.close() });
      } else if (msg.op === 7) {
        pending.get(msg.d.requestId)?.(msg.d);
        pending.delete(msg.d.requestId);
      }
    };
  });
}

/** Tiny --flag value parser. */
export function parseArgs(argv, defaults) {
  const out = { ...defaults, _: [] };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (!a.startsWith('--')) {
      out._.push(a);
      continue;
    }
    const key = a.slice(2);
    const next = argv[i + 1];
    if (next === undefined || next.startsWith('--')) out[key] = true;
    else {
      out[key] = next;
      i++;
    }
  }
  return out;
}
