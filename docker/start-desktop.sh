#!/bin/sh
# Start a virtual X display (:1) with a window manager, served to the browser by noVNC
# on port 6080, then run the given command. Used by the `cpu` service in compose.yaml.
set -e

Xvnc :1 -geometry 1600x900 -depth 24 -SecurityTypes None -localhost=1 -rfbport 5901 >/tmp/xvnc.log 2>&1 &
sleep 1
DISPLAY=:1 fluxbox >/tmp/fluxbox.log 2>&1 &
websockify --web /usr/share/novnc 6080 localhost:5901 >/tmp/novnc.log 2>&1 &

exec "$@"
