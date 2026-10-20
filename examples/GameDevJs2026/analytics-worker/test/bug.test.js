import test from "node:test";
import assert from "node:assert/strict";
import worker from "../src/index.js";
import { handleBug, INSERT_SQL, COUNT_PLAYER_SQL, COUNT_IP_SQL } from "../src/bug.js";

const good = () => ({
  player_id: "abcd-1234-efgh", session_id: "s1", version: "0.7", message: "it broke",
  screen: "data:image/jpeg;base64,AAAA", save: "SAVE", logs: "LOGS", place: "town", client: "UA",
});
const req = (body, headers = {}, path = "/bug") =>
  new Request("https://w.test" + path, {
    method: "POST", headers: { "CF-Connecting-IP": "1.2.3.4", ...headers },
    body: typeof body === "string" ? body : JSON.stringify(body),
  });
const env = { NEON_CONN: "main", NEON_BUG_CONN: "bug", BUG_SALT: "salt" };
// sql stub: counts per query text, records calls
function stub(counts = {}, wait = "42") {
  const calls = [];
  const sql = async (q, p) => {
    calls.push({ q, p });
    if (q === COUNT_PLAYER_SQL) return { rows: [{ count: String(counts.player ?? 0) }] };
    if (q === COUNT_IP_SQL) return { rows: [{ count: String(counts.ip ?? 0) }] };
    if (q === INSERT_SQL) return { rows: [{ id: 7 }] };
    return { rows: [{ wait }] };
  };
  return { sql, calls };
}

test("old_route_is_untouched", async () => {
  const seen = [];
  const realFetch = globalThis.fetch;
  globalThis.fetch = async (url, init) => { seen.push({ url, init }); return new Response('{"ok":1}', { status: 200 }); };
  try {
    const r = await worker.fetch(req('{"query":"select 1"}', {}, "/"), env);
    assert.equal(r.status, 200);
    assert.equal(await r.text(), '{"ok":1}');
    assert.equal(seen[0].init.headers["Neon-Connection-String"], "main");
    assert.equal(seen[0].init.body, '{"query":"select 1"}');
    assert.equal(r.headers.get("Access-Control-Allow-Origin"), "*");
  } finally { globalThis.fetch = realFetch; }
});

test("bug_rejects_sql_and_bad_bodies", async () => {
  const { sql, calls } = stub();
  assert.equal((await handleBug(req({ query: "select * from bug_reports" }), env, sql)).status, 400);
  assert.equal((await handleBug(req("not json"), env, sql)).status, 400);
  assert.equal((await handleBug(req([1]), env, sql)).status, 400);
  const big = JSON.stringify({ ...good(), save: "x".repeat(3 * 1024 * 1024) });
  assert.equal((await handleBug(req(big), env, sql)).status, 413);
  assert.equal((await handleBug(req({ ...good(), player_id: "bad id!" }), env, sql)).status, 400);
  assert.equal((await handleBug(req({ ...good(), player_id: "short" }), env, sql)).status, 400);
  assert.equal((await handleBug(req({ ...good(), message: "" }), env, sql)).status, 400);
  assert.equal((await handleBug(req({ ...good(), message: "x".repeat(2001) }), env, sql)).status, 400);
  assert.equal((await handleBug(req({ ...good(), screen: "data:text/html;base64,AA" }), env, sql)).status, 400);
  assert.equal(calls.length, 0);
});

test("bug_413_by_content_length", async () => {
  const { sql } = stub();
  const r = await handleBug(req(good(), { "Content-Length": String(3 * 1024 * 1024) }), env, sql);
  assert.equal(r.status, 413);
});

test("bug_limits_a_player_to_three", async () => {
  const { sql, calls } = stub({ player: 3 });
  const r = await handleBug(req(good()), env, sql);
  assert.equal(r.status, 429);
  assert.equal((await r.json()).wait, 42);
  assert.ok(!calls.some((c) => c.q === INSERT_SQL));
});

test("bug_limits_a_connection_to_ten", async () => {
  const { sql, calls } = stub({ ip: 10 });
  const r = await handleBug(req(good()), env, sql);
  assert.equal(r.status, 429);
  assert.ok((await r.json()).wait >= 0);
  assert.ok(!calls.some((c) => c.q === INSERT_SQL));
  const ipCall = calls.find((c) => c.q === COUNT_IP_SQL);
  assert.match(ipCall.p[0], /^[0-9a-f]{64}$/);
  assert.notEqual(ipCall.p[0], "1.2.3.4");
});

test("bug_inserts_with_fixed_sql", async () => {
  const { sql, calls } = stub({ player: 2, ip: 9 });
  const r = await handleBug(req(good()), env, sql);
  assert.equal(r.status, 200);
  assert.deepEqual(await r.json(), { id: 7 });
  const ins = calls.find((c) => c.q === INSERT_SQL);
  assert.equal(ins.p.length, 10);
  assert.equal(ins.p[0], "abcd-1234-efgh");
  assert.match(ins.q, /^INSERT INTO bug_reports \(player_id, session_id, ip_hash, version, message, screen, save, logs, place, client\) VALUES \(\$1, \$2, \$3, \$4, \$5, \$6, \$7, \$8, \$9, \$10\) RETURNING id$/);
});

test("bug_route_uses_bug_conn_never_main", async () => {
  const seen = [];
  const realFetch = globalThis.fetch;
  globalThis.fetch = async (url, init) => {
    seen.push(init);
    return new Response(JSON.stringify({ rows: [{ count: "0", id: 1 }] }), { status: 200 });
  };
  try {
    const r = await worker.fetch(req(good()), env);
    assert.equal(r.status, 200);
    assert.ok(seen.length >= 3);
    for (const s of seen) {
      assert.equal(s.headers["Neon-Connection-String"], "bug");
      const b = JSON.parse(s.body);
      assert.equal(typeof b.query, "string");
      assert.ok(Array.isArray(b.params));
    }
    assert.equal(r.headers.get("Access-Control-Allow-Origin"), "*");
  } finally { globalThis.fetch = realFetch; }
});
