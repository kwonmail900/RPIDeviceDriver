#!/usr/bin/env bash
# Print Linux global GPIO number for Raspberry Pi header GPIO17.
# No custom DT overlay; requires GPIO sysfs chip metadata from the kernel.
set -euo pipefail

OFFSET="${1:-17}"
if ! [[ "$OFFSET" =~ ^[0-9]+$ ]] || (( OFFSET > 53 )); then
    printf 'Invalid GPIO line offset: %s\n' "$OFFSET" >&2
    exit 2
fi

shopt -s nullglob
chips=(/sys/class/gpio/gpiochip*)
matches=()
for chip in "${chips[@]}"; do
    if [[ -r "$chip/label" && -r "$chip/base" && -r "$chip/ngpio" ]]; then
        label=$(<"$chip/label")
        base=$(<"$chip/base")
        ngpio=$(<"$chip/ngpio")
        case "$label" in
          pinctrl-rp1|pinctrl-bcm2711|pinctrl-bcm2835|pinctrl-bcm2708)
            if (( OFFSET >= ngpio )); then
                printf 'Requested offset %d is outside %s (%d lines)\n' "$OFFSET" "$label" "$ngpio" >&2
                exit 1
            fi
            gpio=$((base + OFFSET))
            matches+=("$gpio:$label:$base")
            ;;
        esac
    fi
done

if (( ${#matches[@]} == 0 )); then
    printf 'No known Raspberry Pi pinctrl GPIO chip found.\n' >&2
    printf 'Inspect: for c in /sys/class/gpio/gpiochip*; do echo "$c"; cat "$c/label" "$c/base"; done\n' >&2
    printf 'Or inspect: sudo cat /sys/kernel/debug/gpio\n' >&2
    exit 1
fi
if (( ${#matches[@]} > 1 )); then
    printf 'Multiple pinctrl chips found; choose the correct GPIO chip manually.\n' >&2
    exit 1
fi

IFS=: read -r selected_gpio selected_label selected_base <<< "${matches[0]}"
printf 'GPIO line %d: chip=%s, base=%s, global=%s\n' "$OFFSET" "$selected_label" "$selected_base" "$selected_gpio" >&2
printf '%s\n' "$selected_gpio"
