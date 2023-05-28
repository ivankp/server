#!/bin/sh

base64 /dev/urandom | head -c 1029 | netcat localhost 8080

