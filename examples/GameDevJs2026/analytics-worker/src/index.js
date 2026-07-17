// Lightweight Cloudflare Worker that proxies analytics POSTs to Neon's
// SQL-over-HTTP endpoint.  This avoids the CORS preflight failure caused by
// the custom Neon-Connection-String header — the worker adds it server-side.

const NEON_SQL_URL = "https://ep-hidden-meadow-abvf8aat-pooler.eu-west-2.aws.neon.tech/sql";

const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type",
};

export default {
  async fetch(request, env) {
    // Handle CORS preflight
    if (request.method === "OPTIONS") {
      return new Response(null, { status: 204, headers: CORS_HEADERS });
    }

    if (request.method !== "POST") {
      return new Response("Method not allowed", { status: 405, headers: CORS_HEADERS });
    }

    const body = await request.text();

    const resp = await fetch(NEON_SQL_URL, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "Neon-Connection-String": env.NEON_CONN,
      },
      body,
    });

    const respBody = await resp.text();
    return new Response(respBody, {
      status: resp.status,
      headers: { ...CORS_HEADERS, "Content-Type": "application/json" },
    });
  },
};
