#!/bin/bash

cleanup() {
  set -x
  rm -f "$f1" "$f2"
  kill $SERVER_PID
}

f1="$(mktemp)"
f2="$(mktemp)"

../bin/examples/echo_server test >>"$f2" &

SERVER_PID=$!

trap cleanup EXIT

# N=1023
# N=1024
N=1050

for run in {1..20}
do
  base64 /dev/urandom | head -c "$N" | tee "$f1" | netcat localhost 8080

  cmp "$f1" "$f2"
  # diff <(od -c "$f1") <(od -c "$f2")

  :> "$f2"
done
