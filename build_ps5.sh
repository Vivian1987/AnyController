#!/bin/bash
# Build AnyController for PS5. Run from WSL: bash build_ps5.sh
# Needs ps5-payload-dev/sdk (https://github.com/ps5-payload-dev/sdk), v0.45+.
# The older john-tornblom/ps5-payload-sdk builds payloads that crash in
# startup code under current ELF loaders.

set -e

export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-$HOME/ps5-sdk-v045}"
cd "$(dirname "$0")"
make clean >/dev/null
make
echo "Send with: python3 send_payload.py <PS5_IP> anycontroller.elf"
