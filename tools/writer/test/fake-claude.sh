#!/usr/bin/env bash
# A stand-in for `claude -p` in the writing tool's tests: it answers from replies/<step>.json,
# the step being the "title" of the --json-schema it is given.
#   FAKE_CLAUDE_LOG      append the arguments and the prompt (stdin) to this file
#   FAKE_CLAUDE_FAIL     answer as a failed run, exit 1
#   FAKE_CLAUDE_VARIANT  answer from replies/<step>-<variant>.json when that file exists
here="$(cd "$(dirname "$0")" && pwd)"

schema=""
previous=""
for arg in "$@"; do
    if [ "$previous" = "--json-schema" ]; then
        schema="$arg"
    fi
    previous="$arg"
done

step="$(printf '%s' "$schema" | sed -n 's/.*"title"[[:space:]]*:[[:space:]]*"\([a-z]*\)".*/\1/p')"
prompt="$(cat)"

if [ -n "$FAKE_CLAUDE_LOG" ]; then
    {
        printf '=== call %s\n' "$step"
        for arg in "$@"; do printf 'arg: %s\n' "$arg"; done
        printf -- '--- prompt\n%s\n' "$prompt"
    } >> "$FAKE_CLAUDE_LOG"
fi

if [ -n "$FAKE_CLAUDE_FAIL" ]; then
    printf '{"type":"result","subtype":"error_during_execution","is_error":true,"result":"fake failure"}\n'
    exit 1
fi

reply="$here/replies/$step.json"
if [ -n "$FAKE_CLAUDE_VARIANT" ] && [ -f "$here/replies/$step-$FAKE_CLAUDE_VARIANT.json" ]; then
    reply="$here/replies/$step-$FAKE_CLAUDE_VARIANT.json"
fi

if [ ! -f "$reply" ]; then
    printf '{"type":"result","subtype":"error_during_execution","is_error":true,"result":"no fake reply for step %s"}\n' "$step"
    exit 1
fi

printf '{"type":"result","subtype":"success","is_error":false,"result":"","structured_output":%s}\n' "$(cat "$reply")"
