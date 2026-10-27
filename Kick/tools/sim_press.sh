#!/bin/sh
# Press simulator controls by their printed name through macOS accessibility.
#
#   Kick/tools/sim_press.sh "Pad 1" MAIN PUSH "PLAY"
#
# Names are the captions on the panel: "Pad 1".."Pad 16", MAIN, BROWSE, "TRACK MIX", ..., SHIFT,
# A..D (banks), E..H and the other small captions (GRID, SAVE, PAD MIX, ...), PUSH (jog click),
# and the transport keys are "key 42".."key 46" (RECORD, OVERDUB, STOP, PLAY, PLAYSTART; they
# have icons, not text). The simulator (KT_KickEM) must already be running. Needs the terminal to
# be allowed in System Settings > Privacy & Security > Accessibility.
set -e
PID=$(pgrep -x KT_KickEM | head -1)
[ -n "$PID" ] || { echo "KT_KickEM is not running" >&2; exit 1; }
for NAME in "$@"; do
    osascript - "$PID" "$NAME" <<'EOF'
on run argv
    set pid to (item 1 of argv) as integer
    set btn to item 2 of argv
    tell application "System Events"
        set p to first process whose unix id is pid
        set frontmost of p to true
        click button btn of window 1 of p
    end tell
end run
EOF
    sleep 0.3
done
