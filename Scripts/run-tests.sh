#!/usr/bin/env bash
#
# Runs the awsim automation specs headlessly and exits non-zero on any failure,
# so `make` fails when a spec fails.
#
# Usage: run-tests.sh [test-filter]        (default filter: awsim.Simulation)
# Env:   UNREALROOTPATH  path to the engine. If unset, it is read from the
#                        machine-local (gitignored) generated Makefile, so this
#                        script carries no hardcoded path.

set -uo pipefail

FILTER="${1:-awsim.Simulation}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$PROJECT_DIR/awsim.uproject"
OUT_DIR="$PROJECT_DIR/Saved/Automation"
REPORT="$OUT_DIR/index.json"
LOG="$OUT_DIR/editor.log"

# Engine path: prefer $UNREALROOTPATH, else read it from the generated Makefile.
ENGINE="${UNREALROOTPATH:-}"
if [[ -z "$ENGINE" ]]; then
	ENGINE="$(sed -n 's/^UNREALROOTPATH *= *//p' "$PROJECT_DIR/Makefile" 2>/dev/null | head -1)"
fi
if [[ -z "$ENGINE" ]]; then
	echo "error: engine path unknown. Set UNREALROOTPATH, or run the engine's" >&2
	echo "       GenerateProjectFiles so a Makefile defining UNREALROOTPATH exists." >&2
	exit 2
fi
EDITOR="$ENGINE/Engine/Binaries/Linux/UnrealEditor-Cmd"

if [[ ! -x "$EDITOR" ]]; then
	echo "error: UnrealEditor-Cmd not found at '$EDITOR' (check UNREALROOTPATH)" >&2
	exit 2
fi

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

echo ">> Running automation specs: $FILTER"
"$EDITOR" "$PROJECT" \
	-ExecCmds="Automation RunTests $FILTER" \
	-unattended -nopause -nosplash -nullrhi \
	-TestExit="Automation Test Queue Empty" \
	-ReportOutputPath="$OUT_DIR" \
	-log -abslog="$LOG" >/dev/null 2>&1 || true

if [[ ! -f "$REPORT" ]]; then
	echo "error: no test report at '$REPORT' — the editor failed to launch or run tests." >&2
	echo "       see $LOG" >&2
	exit 2
fi

# Parse the JSON report; fail if any test did not succeed.
python3 - "$REPORT" <<'PY'
import json, sys
d = json.load(open(sys.argv[1], encoding="utf-8-sig"))
tests = d.get("tests", [])
def is_fail(t):
    return str(t.get("state", "")).lower() in ("fail", "failed", "error")
bad = [t for t in tests if is_fail(t)]
warned = d.get("succeededWithWarnings", 0)
print(f">> Automation: {len(tests)-len(bad)}/{len(tests)} passed"
      + (f" ({warned} with warnings)" if warned else ""))
for t in bad:
    print(f"   FAIL {t.get('fullTestPath', t.get('testDisplayName', '?'))}")
    for e in (t.get("entries") or []):
        ev = e.get("event") or {}
        if str(ev.get("type", "")).lower() == "error":
            print(f"        {ev.get('message')}")
sys.exit(1 if (bad or d.get("failed", 0)) else 0)
PY
