// POST /bug: takes a bug report as data, never as SQL.  Writes through the
// restricted NEON_BUG_CONN login (insert into, and count, bug_reports only).

export const NEON_SQL_URL = "https://ep-hidden-meadow-abvf8aat-pooler.eu-west-2.aws.neon.tech/sql";

const MAX_BODY = 2 * 1024 * 1024;
const PER_PLAYER = 3;
const PER_IP = 10;

export const COUNT_PLAYER_SQL =
  "SELECT count(*) FROM bug_reports WHERE player_id = $1 AND created_at > now() - interval '15 minutes'";
export const COUNT_IP_SQL =
  "SELECT count(*) FROM bug_reports WHERE ip_hash = $1 AND created_at > now() - interval '15 minutes'";
export const WAIT_PLAYER_SQL =
  "SELECT EXTRACT(EPOCH FROM (min(created_at) + interval '15 minutes' - now())) AS wait FROM bug_reports WHERE player_id = $1 AND created_at > now() - interval '15 minutes'";
export const WAIT_IP_SQL =
  "SELECT EXTRACT(EPOCH FROM (min(created_at) + interval '15 minutes' - now())) AS wait FROM bug_reports WHERE ip_hash = $1 AND created_at > now() - interval '15 minutes'";
export const NEXT_ID_SQL = "SELECT nextval('bug_reports_id_seq') AS id";
// One statement counts and inserts, so a burst cannot slip past the limits.
// $1 id, $2 player_id, $3 session_id, $4 ip_hash, $5 version, $6 message,
// $7 screen, $8 save, $9 logs, $10 place, $11 client.  rowCount 0 = a limit.
export const INSERT_SQL =
  "INSERT INTO bug_reports (id, player_id, session_id, ip_hash, version, message, screen, save, logs, place, client) " +
  "SELECT $1::integer, $2::text, $3::text, $4::text, $5::text, $6::text, $7::text, $8::text, $9::text, $10::text, $11::text " +
  "WHERE (SELECT count(*) FROM bug_reports WHERE player_id = $2::text AND created_at > now() - interval '15 minutes') < " + PER_PLAYER + " " +
  "AND (SELECT count(*) FROM bug_reports WHERE ip_hash = $4::text AND created_at > now() - interval '15 minutes') < " + PER_IP;

const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type",
};

function json(status, obj) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: { ...CORS_HEADERS, "Content-Type": "application/json" },
  });
}

// The default sql(): Neon SQL-over-HTTP with the restricted login.
export function neonSql(env) {
  return async (query, params) => {
    const resp = await fetch(NEON_SQL_URL, {
      method: "POST",
      headers: { "Content-Type": "application/json", "Neon-Connection-String": env.NEON_BUG_CONN },
      body: JSON.stringify({ query, params }),
    });
    if (!resp.ok) throw new Error("neon " + resp.status + " " + (await resp.text()).slice(0, 300));
    return resp.json();
  };
}

async function sha256Hex(text) {
  const buf = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
  return [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

// Reads the body as a stream, stopping at MAX_BODY (null when over).
async function readCapped(request) {
  if (!request.body) return "";
  const reader = request.body.getReader();
  const chunks = [];
  let total = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    total += value.length;
    if (total > MAX_BODY) { await reader.cancel(); return null; }
    chunks.push(value);
  }
  const all = new Uint8Array(total);
  let at = 0;
  for (const c of chunks) { all.set(c, at); at += c.length; }
  return new TextDecoder().decode(all);
}

const str = (v) => (typeof v === "string" ? v : null);

export async function handleBug(request, env, sql = neonSql(env)) {
  const declared = Number(request.headers.get("Content-Length"));
  if (declared > MAX_BODY) return json(413, { error: "too large" });

  const raw = await readCapped(request);
  if (raw === null) return json(413, { error: "too large" });

  let b;
  try { b = JSON.parse(raw); } catch { return json(400, { error: "not json" }); }
  if (b === null || typeof b !== "object" || Array.isArray(b)) return json(400, { error: "not an object" });

  if (typeof b.player_id !== "string" || !/^[A-Za-z0-9-]{8,64}$/.test(b.player_id))
    return json(400, { error: "bad player_id" });
  if (typeof b.message !== "string" || b.message.length < 1 || b.message.length > 2000)
    return json(400, { error: "bad message" });
  if (b.screen !== undefined && b.screen !== null && b.screen !== "") {
    if (typeof b.screen !== "string" || !b.screen.startsWith("data:image/jpeg;base64,"))
      return json(400, { error: "bad screen" });
  }

  for (const [k, max] of [["session_id", 200], ["version", 200], ["place", 200], ["client", 500]]) {
    if (b[k] !== undefined && b[k] !== null && (typeof b[k] !== "string" || b[k].length > max))
      return json(400, { error: "bad " + k });
  }

  const ip = request.headers.get("CF-Connecting-IP") || "";
  const ipHash = await sha256Hex(ip + (env.BUG_SALT || ""));

  try {
    const id = (await sql(NEXT_ID_SQL, [])).rows[0].id;
    const r = await sql(INSERT_SQL, [
      id, b.player_id, str(b.session_id), ipHash, str(b.version), b.message,
      str(b.screen) || "", str(b.save), str(b.logs), str(b.place), str(b.client),
    ]);
    // Fail closed: only a positive rowCount is a kept report.  No number at all
    // is a store error (502), not a success.
    if (typeof r.rowCount !== "number") throw new Error("neon answered without rowCount");
    if (!(r.rowCount > 0)) {
      // A limit held it back.  Counting again is only to tell which, for the wait.
      const c = await sql(COUNT_PLAYER_SQL, [b.player_id]);
      const byPlayer = Number(c.rows?.[0]?.count) >= PER_PLAYER;
      const w = await sql(byPlayer ? WAIT_PLAYER_SQL : WAIT_IP_SQL, [byPlayer ? b.player_id : ipHash]);
      const wait = Math.max(0, Math.ceil(Number(w.rows?.[0]?.wait) || 0));
      return json(429, { error: "slow down", wait });
    }
    return json(200, { id: Number(id) });
  } catch (e) {
    console.error("bug store failed:", e && e.message);
    return json(502, { error: "store" });
  }
}
