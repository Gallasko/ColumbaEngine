// POST /bug: takes a bug report as data, never as SQL.  Writes through the
// restricted NEON_BUG_CONN login (insert into, and count, bug_reports only).

export const NEON_SQL_URL = "https://ep-hidden-meadow-abvf8aat-pooler.eu-west-2.aws.neon.tech/sql";

const MAX_BODY = 2 * 1024 * 1024;
const PER_PLAYER = 3;
const PER_CONNECTION = 10;

export const COUNT_PLAYER_SQL =
  "SELECT count(*) FROM bug_reports WHERE player_id = $1 AND created_at > now() - interval '15 minutes'";
export const COUNT_IP_SQL =
  "SELECT count(*) FROM bug_reports WHERE ip_hash = $1 AND created_at > now() - interval '15 minutes'";
export const WAIT_PLAYER_SQL =
  "SELECT EXTRACT(EPOCH FROM (min(created_at) + interval '15 minutes' - now())) AS wait FROM bug_reports WHERE player_id = $1 AND created_at > now() - interval '15 minutes'";
export const WAIT_IP_SQL =
  "SELECT EXTRACT(EPOCH FROM (min(created_at) + interval '15 minutes' - now())) AS wait FROM bug_reports WHERE ip_hash = $1 AND created_at > now() - interval '15 minutes'";
export const INSERT_SQL =
  "INSERT INTO bug_reports (player_id, session_id, ip_hash, version, message, screen, save, logs, place, client) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) RETURNING id";

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
    if (!resp.ok) throw new Error("neon " + resp.status);
    return resp.json();
  };
}

async function sha256Hex(text) {
  const buf = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
  return [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

const str = (v) => (typeof v === "string" ? v : null);

export async function handleBug(request, env, sql = neonSql(env)) {
  const declared = Number(request.headers.get("Content-Length"));
  if (declared > MAX_BODY) return json(413, { error: "too large" });

  const raw = await request.text();
  if (new TextEncoder().encode(raw).length > MAX_BODY) return json(413, { error: "too large" });

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

  const ip = request.headers.get("CF-Connecting-IP") || "";
  const ipHash = await sha256Hex(ip + (env.BUG_SALT || ""));

  try {
    const checks = [
      [COUNT_PLAYER_SQL, WAIT_PLAYER_SQL, b.player_id, PER_PLAYER],
      [COUNT_IP_SQL, WAIT_IP_SQL, ipHash, PER_CONNECTION],
    ];
    for (const [countSql, waitSql, key, limit] of checks) {
      const c = await sql(countSql, [key]);
      if (Number(c.rows?.[0]?.count) >= limit) {
        const w = await sql(waitSql, [key]);
        const wait = Math.max(1, Math.ceil(Number(w.rows?.[0]?.wait) || 900));
        return json(429, { error: "slow down", wait });
      }
    }
    const r = await sql(INSERT_SQL, [
      b.player_id, str(b.session_id), ipHash, str(b.version), b.message,
      str(b.screen) || "", str(b.save), str(b.logs), str(b.place), str(b.client),
    ]);
    return json(200, { id: r.rows[0].id });
  } catch {
    return json(502, { error: "store failed" });
  }
}
