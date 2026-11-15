#!/usr/bin/env bash
set -euo pipefail
# Optional ARDUINO_CONFIG points to an arduino-cli.yaml outside the repository.
root="$(cd "$(dirname "$0")/.." && pwd)"
cli_args=()
if [[ -n "${ARDUINO_CONFIG:-}" ]]; then cli_args+=(--config-file "$ARDUINO_CONFIG"); fi
fqbn='esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=huge_app,CDCOnBoot=cdc'
for sketch in Arvore_ASCII Arvore_ASCII_Codigo AguaViva_ASCII AguaViva_ASCII_Codigo; do
  arduino-cli "${cli_args[@]}" compile --fqbn "$fqbn" --warnings default --jobs 2 "$root/$sketch"
done
