// SPDX-License-Identifier: GPL-3.0-only
// (1.0.1) the relay's install counter (web/relay/worker.js /count): counts a finished install by version, a few an hour from one address,
// refuses a bad version, keeps no address, and the GET has the totals. The worker runs here against an in-memory KV, as local.mjs does.
import worker from "../web/relay/worker.js";

const kv = new Map();
const LIB = {
  async get(k) { const e = kv.get(k); return e ? e.v : null; },
  async getWithMetadata(k) { const e = kv.get(k); return e ? { value: e.v, metadata: e.meta || null } : { value: null, metadata: null }; },
  async put(k, v, o = {}) { kv.set(k, { v, meta: o.metadata }); },
  async delete(k) { kv.delete(k); },
  async list({ prefix = "" } = {}) { return { keys: [...kv].filter(([k]) => k.startsWith(prefix)).map(([name, e]) => ({ name, metadata: e.meta })), list_complete: true }; },
};
globalThis.caches = { default: { async match() { return undefined; }, async put() {}, async delete() {} } };
const env = { LIB, ADMIN: "test-admin", ORIGINS: "https://deamonslinky.github.io" };
let fails = 0;
const ok = (c, m) => { if (!c) { fails++; console.log("FAIL", m); } };
const call = async (method, body, ip = "1.2.3.4") => {
  const r = await worker.fetch(new Request("https://relay/count", { method, headers: { "Content-Type": "application/json", "CF-Connecting-IP": ip, Origin: "https://deamonslinky.github.io" }, body: body ? JSON.stringify(body) : undefined }), env, { waitUntil() {} });
  return { status: r.status, body: await r.json(), cors: r.headers.get("Access-Control-Allow-Origin") };
};

let r = await call("GET");
ok(r.status === 200 && r.body.total === 0, "an empty count reads 0");
r = await call("POST", { v: "1.0.1" });
ok(r.status === 200 && r.body.counted === true, "an install counts");
r = await call("POST", { v: "1.0.1" }, "5.6.7.8");
r = await call("POST", { v: "1.0.0" }, "9.9.9.9");
r = await call("GET");
ok(r.body.total === 3 && r.body.v["1.0.1"] === 2 && r.body.v["1.0.0"] === 1, "totals by version: " + JSON.stringify(r.body));
ok(/^\d{4}-\d\d-\d\d$/.test(r.body.since), "the day counting began: " + r.body.since);
ok(r.cors === "https://deamonslinky.github.io", "the site may read it");
for (let i = 0; i < 6; i++) r = await call("POST", { v: "1.0.1" }, "7.7.7.7");
ok(r.body.counted === false, "a few an hour from one address, then no more");
r = await call("GET");
ok(r.body.total === 6, "three counted from the busy address: " + r.body.total);
r = await call("POST", { v: "<script>" }, "8.8.8.8");
ok(r.status === 400, "a bad version is refused");
r = await call("POST", {}, "8.8.8.8");
ok(r.status === 400, "no version is refused");
const keys = [...kv.keys()];
ok(!keys.some((k) => k.includes("1.2.3.4") || k.includes("7.7.7.7")), "no address is kept: " + keys.join(" "));
ok(!JSON.stringify([...kv.values()]).includes("1.2.3.4"), "no address in any value");
if (fails) { console.log(`web count: ${fails} FAILED`); process.exit(1); }
console.log("web count: ok");
