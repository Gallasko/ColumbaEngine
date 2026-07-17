#pragma once

// JavaScript bridge for sending analytics data from C++/WASM to Neon Postgres.
// Uses fetch() with keepalive:true so requests survive page close/tab switch.
// On desktop builds, all functions are no-ops.

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// Generate a random UUID v4 session ID (per page load, no PII).
// Returns a malloc'd C string — caller must free().
EM_JS(char*, js_analytics_get_session_id, (), {
    if (!Module._analyticsSessionId) {
        Module._analyticsSessionId = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(
            /[xy]/g, function(c) {
                var r = Math.random() * 16 | 0;
                var v = c === 'x' ? r : (r & 0x3 | 0x8);
                return v.toString(16);
            });
    }
    var id = Module._analyticsSessionId;
    var len = lengthBytesUTF8(id) + 1;
    var buf = _malloc(len);
    stringToUTF8(id, buf, len);
    return buf;
});

// Send a parameterized INSERT to the analytics proxy worker using fetch + keepalive.
// proxyUrl: e.g. "https://analytics-proxy.your-account.workers.dev"
// connStr:  unused (kept for API compat) — the worker holds the Neon connection string.
// query:    the SQL query with $1, $2, ... placeholders
// paramsJson: a JSON array string, e.g. '["abc","session_end",12345]'
EM_JS(void, js_analytics_send, (const char* proxyUrl, const char* connStr,
                                 const char* query, const char* paramsJson), {
    var url  = UTF8ToString(proxyUrl);
    var sql  = UTF8ToString(query);
    var params = UTF8ToString(paramsJson);

    var body = JSON.stringify({
        query: sql,
        params: JSON.parse(params)
    });

    fetch(url, {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: body,
        keepalive: true
    }).then(function(r) {
        if (!r.ok) r.text().then(function(t) { console.warn("[Analytics] POST failed:", r.status, t); });
    }).catch(function(e) {
        console.warn("[Analytics] fetch error:", e);
    });
});

// Returns Date.now() (milliseconds since epoch).
EM_JS(double, js_analytics_now_ms, (), {
    return Date.now();
});

#else
// Desktop stubs — analytics is web-only.
inline char* js_analytics_get_session_id() { return nullptr; }
inline void js_analytics_send(const char*, const char*, const char*, const char*) {}
inline double js_analytics_now_ms() { return 0.0; }
#endif
