import test from "node:test";
import assert from "node:assert/strict";
import worker from "../src/index.js";
import { handleBug, INSERT_SQL, NEXT_ID_SQL, COUNT_PLAYER_SQL, COUNT_IP_SQL } from "../src/bug.js";

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
// sql stub: a limit (player/ip) makes the conditional insert insert nothing
function stub(limit = null, wait = "42") {
  const calls = [];
  const sql = async (q, p) => {
    calls.push({ q, p });
    if (q === NEXT_ID_SQL) return { rows: [{ id: "7" }] };
    if (q === INSERT_SQL) return { rows: [], rowCount: limit ? 0 : 1 };
    if (q === COUNT_PLAYER_SQL) return { rows: [{ count: limit === "player" ? "3" : "0" }] };
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
  const { sql } = stub("player");
  const r = await handleBug(req(good()), env, sql);
  assert.equal(r.status, 429);
  assert.equal((await r.json()).wait, 42);
});

test("bug_limits_a_connection_to_ten", async () => {
  const { sql, calls } = stub("ip", "0");
  const r = await handleBug(req(good()), env, sql);
  assert.equal(r.status, 429);
  assert.equal((await r.json()).wait, 0);
  const ins = calls.find((c) => c.q === INSERT_SQL);
  assert.match(ins.p[3], /^[0-9a-f]{64}$/);
  assert.notEqual(ins.p[3], "1.2.3.4");
});

test("bug_inserts_with_fixed_sql_and_nextval_id", async () => {
  const { sql, calls } = stub();
  const r = await handleBug(req(good()), env, sql);
  assert.equal(r.status, 200);
  assert.deepEqual(await r.json(), { id: 7 });
  assert.ok(calls.every((c) => !/RETURNING/i.test(c.q)));
  assert.equal(calls[0].q, NEXT_ID_SQL);
  const ins = calls[1];
  assert.equal(ins.q, INSERT_SQL);
  assert.equal(ins.p.length, 11);
  const g = good();
  assert.equal(ins.p[0], "7");
  assert.deepEqual(ins.p.slice(1, 3), [g.player_id, g.session_id]);
  assert.match(ins.p[3], /^[0-9a-f]{64}$/);
  assert.deepEqual(ins.p.slice(4), [g.version, g.message, g.screen, g.save, g.logs, g.place, g.client]);
});

test("bug_caps_short_fields", async () => {
  const { sql } = stub();
  for (const [k, n] of [["session_id", 201], ["version", 201], ["place", 201], ["client", 501]])
    assert.equal((await handleBug(req({ ...good(), [k]: "x".repeat(n) }), env, sql)).status, 400);
});

test("bug_store_failure_is_502_and_logged", async () => {
  const sql = async () => { throw new Error("neon 500 boom"); };
  const logged = [];
  const orig = console.error; console.error = (...a) => logged.push(a.join(" "));
  try {
    const r = await handleBug(req(good()), env, sql);
    assert.equal(r.status, 502);
    assert.deepEqual(await r.json(), { error: "store" });
  } finally { console.error = orig; }
  assert.ok(logged.some((l) => l.includes("boom")));
  assert.ok(!logged.some((l) => l.includes("it broke")));
});

test("bug_route_uses_bug_conn_never_main", async () => {
  const seen = [];
  const realFetch = globalThis.fetch;
  globalThis.fetch = async (url, init) => {
    seen.push(init);
    return new Response(JSON.stringify({ rows: [{ id: "1" }], rowCount: 1 }), { status: 200 });
  };
  try {
    const r = await worker.fetch(req(good()), env);
    assert.equal(r.status, 200);
    assert.equal(seen.length, 2);
    for (const s of seen) {
      assert.equal(s.headers["Neon-Connection-String"], "bug");
      const b = JSON.parse(s.body);
      assert.equal(typeof b.query, "string");
      assert.ok(Array.isArray(b.params));
    }
    assert.equal(r.headers.get("Access-Control-Allow-Origin"), "*");
  } finally { globalThis.fetch = realFetch; }
});

test("bug_missing_rowcount_fails_closed", async () => {
  const sql = async (q) => (q === NEXT_ID_SQL ? { rows: [{ id: "7" }] } : { rows: [] });
  const orig = console.error; console.error = () => {};
  try {
    const r = await handleBug(req(good()), env, sql);
    assert.equal(r.status, 502);
  } finally { console.error = orig; }
});

test("bug_sql_uses_the_limit_constants", () => {
  assert.match(INSERT_SQL, /< 3 AND/);
  assert.match(INSERT_SQL, /< 10$/);
});
