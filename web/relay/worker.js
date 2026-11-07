// SPDX-License-Identifier: GPL-3.0-only
// PERF-1's library relay: a Cloudflare Worker (free plan) with one KV namespace (LIB). The Pieces page submits piece and set codes here and
// reads the list back, so a submission is in the Library at once, with no account and no GitHub issue. Every code is decoded with the site's
// own fm1piece.js before it is kept (a real piece or set; title, name and note short). The maker removes anything with the ADMIN secret; a
// submitter can remove their own with the key the page keeps for it.
//   GET  /list                          {items: [{id, code, added}]}, newest first (cached a minute)
//   POST /submit {codes: [...]}         {done: [{id, del} | {error}]}  (16 a time; 30 an hour from one address)
//   POST /remove {id, del}              {ok}  (del: the submitter's key, or the ADMIN secret)
// Setup: web/relay/README.md.
import { pcFromCode } from "../fm1piece.js";

const MAX_CODE = 16000, MAX_ITEMS = 3000, PER_HOUR = 30;
const enc = new TextEncoder();

function cors(req, env) {
  const o = req.headers.get("Origin") || "", ok = (env.ORIGINS || "").split(",").map((s) => s.trim()).filter(Boolean);
  const allow = ok.includes(o) || /^http:\/\/(localhost|127\.0\.0\.1)(:\d+)?$/.test(o) ? o : ok[0] || "*";
  return { "Access-Control-Allow-Origin": allow, "Access-Control-Allow-Methods": "GET, POST, OPTIONS", "Access-Control-Allow-Headers": "Content-Type", Vary: "Origin" };
}
const json = (req, env, body, status = 200, extra = {}) =>
  new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json", ...cors(req, env), ...extra } });

async function hmac(secret, text) {
  const k = await crypto.subtle.importKey("raw", enc.encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const s = new Uint8Array(await crypto.subtle.sign("HMAC", k, enc.encode(text)));
  return [...s.slice(0, 12)].map((b) => b.toString(16).padStart(2, "0")).join("");
}
async function sha(text) {
  const s = new Uint8Array(await crypto.subtle.digest("SHA-256", enc.encode(text)));
  return [...s.slice(0, 10)].map((b) => b.toString(16).padStart(2, "0")).join("");
}
const plain = (s, n) => typeof s !== "string" || (s.length <= n && !/[\u0000-\u0008\u000b-\u001f\u007f<>]/.test(s));

async function check(code) {                       // the reason it is refused, or "" (a piece or set whose words are short and plain)
  if (typeof code !== "string" || !/^P1\.[A-Za-z0-9_-]+$/.test(code)) return "not a piece code";
  if (code.length > MAX_CODE) return "too long";
  let x;
  try { x = await pcFromCode(code); } catch (e) { return "it does not read: " + (e && e.message ? e.message : "damaged"); }
  const m = x.meta || {};
  if (!plain(m.t, 60) || !plain(m.a, 40) || !plain(m.n, 400)) return "the title, name or note is too long or has odd characters";
  return "";
}

async function list(env) {
  const items = [];
  let cursor;
  do {
    const r = await env.LIB.list({ prefix: "e:", cursor });
    for (const k of r.keys) items.push({ id: k.name.slice(2), added: (k.metadata && k.metadata.added) || "" });
    cursor = r.list_complete ? undefined : r.cursor;
  } while (cursor && items.length < MAX_ITEMS);
  const codes = await Promise.all(items.map((i) => env.LIB.get("e:" + i.id)));
  return items.map((i, k) => ({ ...i, code: codes[k] })).filter((i) => i.code);
}
// the list the page reads is one key ("index"), rebuilt from the entries when one is added or removed: a read of the list costs one KV read.
// (Two submissions at once may each miss the other in their rebuild; the entries stay, and the next rebuild has them.) KV's list() is eventually
// consistent (a key a few seconds old can be missing), so a rebuild starts from the index as it was too, and a resubmission puts back an entry the
// index lost (2026-11-07: the first entry was stored, the index left empty by a read just before it).
async function rebuild(env, add = [], drop = "") {
  const have = new Map((await list(env)).map((i) => [i.id, i]));
  try {
    const old = JSON.parse((await env.LIB.get("index")) || "[]");
    for (const i of Array.isArray(old) ? old : []) if (i && i.id && i.code && !have.has(i.id)) have.set(i.id, i);
  } catch (_) {}
  for (const i of add) have.set(i.id, i);
  if (drop) have.delete(drop);
  const items = [...have.values()].sort((a, b) => (a.added < b.added ? 1 : a.added > b.added ? -1 : 0));
  await env.LIB.put("index", JSON.stringify(items));
  return items;
}

export default {
  async fetch(req, env, ctx) {
    const url = new URL(req.url);
    if (req.method === "OPTIONS") return new Response(null, { headers: cors(req, env) });
    try {
      if (req.method === "GET" && url.pathname === "/list") {
        const cache = caches.default, key = new Request(url.origin + "/list");
        let hit = await cache.match(key);
        if (!hit) {
          const idx = await env.LIB.get("index");
          hit = new Response(JSON.stringify({ items: idx ? JSON.parse(idx) : await rebuild(env) }), { headers: { "Content-Type": "application/json", "Cache-Control": "public, max-age=60" } });
          ctx.waitUntil(cache.put(key, hit.clone()));
        }
        return new Response(hit.body, { headers: { "Content-Type": "application/json", ...cors(req, env) } });
      }
      if (req.method === "POST" && url.pathname === "/submit") {
        if (!env.ADMIN)                                               // (2026-11-07: an empty secret made every submit fail after storing the piece, the list never rebuilt)
          return json(req, env, { error: "the relay is not set up: its ADMIN secret is empty (wrangler secret put ADMIN)" }, 503);
        const body = await req.json().catch(() => ({}));
        const codes = Array.isArray(body.codes) ? body.codes.slice(0, 16) : [];
        if (!codes.length) return json(req, env, { error: "nothing to submit" }, 400);
        const who = await sha((req.headers.get("CF-Connecting-IP") || "") + (env.ADMIN || "")), hour = Math.floor(Date.now() / 3600000);
        const rk = "rl:" + who + ":" + hour, used = +((await env.LIB.get(rk)) || 0);
        if (used + codes.length > PER_HOUR) return json(req, env, { error: "too many from here this hour: try later" }, 429);
        await env.LIB.put(rk, String(used + codes.length), { expirationTtl: 3700 });
        const done = [], fresh = [];
        for (const code of codes) {
          const why = await check(code);
          if (why) { done.push({ error: why }); continue; }
          const id = await sha(code);                                // the same code twice is one entry
          const was = await env.LIB.getWithMetadata("e:" + id);
          if (!was.value) {
            const added = new Date().toISOString();
            await env.LIB.put("e:" + id, code, { metadata: { added } });
            fresh.push({ id, added, code });
          } else {
            fresh.push({ id, added: (was.metadata && was.metadata.added) || new Date().toISOString(), code });   // (already in: the index gets it again)
          }
          done.push({ id, del: await hmac(env.ADMIN || "", id) });
        }
        if (fresh.length) await rebuild(env, fresh);
        ctx.waitUntil(caches.default.delete(new Request(url.origin + "/list")));
        return json(req, env, { done });
      }
      if (req.method === "POST" && url.pathname === "/remove") {
        const body = await req.json().catch(() => ({}));
        const id = String(body.id || ""), del = String(body.del || "");
        if (!/^[0-9a-f]{20}$/.test(id)) return json(req, env, { error: "no such entry" }, 400);
        if (!env.ADMIN || (del !== env.ADMIN && del !== (await hmac(env.ADMIN, id)))) return json(req, env, { error: "not yours to remove" }, 403);
        await env.LIB.delete("e:" + id);
        await rebuild(env, [], id);
        ctx.waitUntil(caches.default.delete(new Request(url.origin + "/list")));
        return json(req, env, { ok: true });
      }
      return json(req, env, { error: "not found" }, 404);
    } catch (e) {
      return json(req, env, { error: "the relay failed: " + (e && e.message ? e.message : e) }, 500);
    }
  },
};
