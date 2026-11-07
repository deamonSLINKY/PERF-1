// SPDX-License-Identifier: GPL-3.0-only
// The library relay on this computer, for trying the Pieces page without Cloudflare: worker.js behind Node's HTTP server, its KV in memory
// (or in a JSON file, so it lasts), its cache a no-op.   node web/relay/local.mjs [port] [kv-file]   (then relay.json: {"url": "http://localhost:8787"})
import http from "node:http";
import fs from "node:fs";
import worker from "./worker.js";

const port = +(process.argv[2] || 8787), file = process.argv[3] || "";
const kv = new Map(file && fs.existsSync(file) ? JSON.parse(fs.readFileSync(file, "utf8")) : []);
const save = () => { if (file) fs.writeFileSync(file, JSON.stringify([...kv])); };
const LIB = {
  async get(k) { const e = kv.get(k); return e && (!e.exp || e.exp > Date.now()) ? e.v : null; },
  async getWithMetadata(k) { const e = kv.get(k); return e && (!e.exp || e.exp > Date.now()) ? { value: e.v, metadata: e.meta || null } : { value: null, metadata: null }; },
  async put(k, v, o = {}) { kv.set(k, { v, meta: o.metadata, exp: o.expirationTtl ? Date.now() + o.expirationTtl * 1000 : 0 }); save(); },
  async delete(k) { kv.delete(k); save(); },
  async list({ prefix = "" } = {}) { return { keys: [...kv].filter(([k]) => k.startsWith(prefix)).map(([name, e]) => ({ name, metadata: e.meta })), list_complete: true }; },
};
globalThis.caches = { default: { async match() { return undefined; }, async put() {}, async delete() {} } };
const env = { LIB, ADMIN: process.env.ADMIN || "local-admin", ORIGINS: "http://localhost:8765" };

http.createServer(async (req, res) => {
  const body = await new Promise((r) => { const b = []; req.on("data", (c) => b.push(c)); req.on("end", () => r(Buffer.concat(b))); });
  const headers = { ...req.headers, "cf-connecting-ip": req.socket.remoteAddress || "" };
  const r = await worker.fetch(new Request("http://localhost:" + port + req.url, { method: req.method, headers, body: ["GET", "HEAD", "OPTIONS"].includes(req.method) ? undefined : body }), env, { waitUntil() {} });
  res.writeHead(r.status, Object.fromEntries(r.headers));
  res.end(Buffer.from(await r.arrayBuffer()));
}).listen(port, () => console.log("library relay on http://localhost:" + port + (file ? " (kept in " + file + ")" : "")));
