#!/bin/sh
# usage: run_integration.sh <fake_nm.py> <test-binary>   (run inside dbus-run-session)
python3 "$1" &
fake=$!
trap 'kill $fake 2>/dev/null' EXIT
for _ in $(seq 50); do
    busctl --user list 2>/dev/null | grep -q org.freedesktop.NetworkManager && break
    sleep 0.1
done
"$2"
