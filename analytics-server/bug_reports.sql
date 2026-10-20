-- Bug reports from the Chronicle in-game form.  Run once in the Neon console,
-- as the table's owner (the main role), in the analytics' project and database.
-- Change CHANGEME before running.
CREATE TABLE bug_reports (
  id SERIAL PRIMARY KEY,
  player_id TEXT NOT NULL, session_id TEXT, ip_hash TEXT NOT NULL,
  version TEXT, message TEXT NOT NULL, screen TEXT, save TEXT, logs TEXT,
  place TEXT, client TEXT,
  created_at TIMESTAMPTZ DEFAULT NOW()
);
CREATE INDEX ON bug_reports (player_id, created_at);
CREATE INDEX ON bug_reports (ip_hash, created_at);
CREATE ROLE bug_writer WITH LOGIN PASSWORD 'CHANGEME';
GRANT INSERT ON bug_reports TO bug_writer;
GRANT USAGE, SELECT ON SEQUENCE bug_reports_id_seq TO bug_writer;
GRANT SELECT (player_id, ip_hash, created_at) ON bug_reports TO bug_writer;
