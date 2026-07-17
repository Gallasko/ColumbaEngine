-- Analytics schema for GameDevJs2026
-- Run this once in the Neon SQL console to set up the table and restricted role.

CREATE TABLE IF NOT EXISTS analytics_events (
    id                  SERIAL PRIMARY KEY,
    session_id          TEXT NOT NULL,
    event_type          TEXT NOT NULL,        -- 'session_start', 'heartbeat', 'session_end'
    session_duration_ms BIGINT,
    total_play_time_ms  BIGINT,
    timestamp_ms        BIGINT,
    save_snapshot       TEXT,                 -- full save data (session_end only)
    created_at          TIMESTAMPTZ DEFAULT NOW()
);

-- Index for common queries
CREATE INDEX IF NOT EXISTS idx_analytics_event_type ON analytics_events (event_type);
CREATE INDEX IF NOT EXISTS idx_analytics_session_id ON analytics_events (session_id);

-- Restricted role for client-side inserts (no read/update/delete).
-- Change the password before deploying!
DO $$
BEGIN
    IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'analytics_writer') THEN
        CREATE ROLE analytics_writer WITH LOGIN PASSWORD 'CHANGEME';
    END IF;
END
$$;

GRANT INSERT ON analytics_events TO analytics_writer;
GRANT USAGE, SELECT ON SEQUENCE analytics_events_id_seq TO analytics_writer;

-- Example queries (run as the main role, not analytics_writer):
--
-- Average session duration:
--   SELECT AVG(session_duration_ms) / 1000.0 AS avg_seconds
--   FROM analytics_events WHERE event_type = 'session_end';
--
-- Session count by day:
--   SELECT DATE(created_at), COUNT(*)
--   FROM analytics_events WHERE event_type = 'session_end'
--   GROUP BY 1 ORDER BY 1;
--
-- Inspect a player's save:
--   SELECT save_snapshot FROM analytics_events
--   WHERE event_type = 'session_end' ORDER BY created_at DESC LIMIT 1;
