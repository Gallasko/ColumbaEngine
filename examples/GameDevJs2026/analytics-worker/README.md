# analytics-proxy

Cloudflare Worker in front of Neon.

- `POST /` (old route): forwards the body as SQL to Neon with `NEON_CONN`.
- `POST /bug`: takes a Chronicle bug report as JSON data (never SQL), checks it,
  limits it (3 a player and 10 a connection per 15 minutes) and inserts it with
  fixed SQL through the restricted `NEON_BUG_CONN` login.

Tests: `cd examples/GameDevJs2026/analytics-worker && node --test` (Node 18+, no dependencies).

## Deploy steps (by hand)

1. In the Neon console, open `/analytics-server/bug_reports.sql`, replace `CHANGEME` with a
   strong password, and run it as the main role.
2. Take the `bug_writer` connection string (same host and database as `NEON_CONN`, user
   `bug_writer`, that password).
3. `cd examples/GameDevJs2026/analytics-worker`
4. `wrangler secret put NEON_BUG_CONN` (paste the `bug_writer` string)
5. `wrangler secret put BUG_SALT` (any long random string; it salts the connection hash)
6. `wrangler deploy`
7. Check: `curl -X POST <worker-url>/bug -d '{"query":"select 1"}'` answers 400.
8. Check that the old route cannot read the reports. `POST /` runs any SQL as `NEON_CONN`, so
   the reports (with every save) are safe only if `NEON_CONN` is the restricted
   `analytics_writer` login of `/analytics-server/schema.sql`, never the main role:

   ```
   curl -s -X POST <worker-url>/ -H 'Content-Type: application/json' \
     -d '{"query":"SELECT count(*) FROM bug_reports","params":[]}'
   ```

   It must answer an error, `permission denied for table bug_reports`. If it answers a count,
   `NEON_CONN` is a login that can read the reports: put the `analytics_writer` connection
   string in it (`wrangler secret put NEON_CONN`), `wrangler deploy`, and run this check again
   before the game is published.

Until then the game's Send answers with the failure line.
