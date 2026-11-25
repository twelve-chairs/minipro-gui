#!/bin/sh
# Stand-in for the minipro CLI, used by MainWindowTest.
# The test copies this script into a temp dir as "minipro" and puts that dir first on PATH.
#
# Environment:
#   FAKE_MINIPRO_LOG          file each invocation's arguments are appended to (one line per call)
#   FAKE_MINIPRO_NO_PROGRAMMER  if set, --presence_check reports no programmer
#   FAKE_MINIPRO_PRESENCE     if set, --presence_check prints this text instead (\n allowed)
#   FAKE_MINIPRO_READ_HEX     hex string written (as bytes) to the -r target file
#   FAKE_MINIPRO_READ_EMPTY   if set, -r writes an empty file
#   FAKE_MINIPRO_READ_EXIT    exit code for -r / -w / -F operations (default 0)
#   FAKE_MINIPRO_CRASH        if set, device operations kill themselves with SIGKILL (a crash)
#   FAKE_MINIPRO_DELAY        seconds a device operation takes (default 0)
#   FAKE_MINIPRO_DONE_LOG     file each device operation appends its arguments to when it ends
#   FAKE_MINIPRO_INFO_PROMPT  if set, -l and -d ask "Which database...?" on stdin like minipro
#                             does without a programmer, and abort unless answered
#   FAKE_MINIPRO_INFO_SLEEP   seconds -l and -d take (default 0)
#   FAKE_MINIPRO_INFO_FAIL    if set, -d reports an unknown device and exits 1
#   FAKE_MINIPRO_CRLF         if set, -l and -d use Windows line endings

ARGS="$*"
if [ -n "$FAKE_MINIPRO_LOG" ]; then
  echo "$*" >> "$FAKE_MINIPRO_LOG"
fi

case "$1" in
  --version)
    echo "minipro version 0.7.4     A free and open TL866 series programmer" >&2
    echo "Commit date:	2024-01-01 00:00:00 +0000" >&2
    exit 0
    ;;
  --presence_check)
    # Same wording as minipro's print_connected_programmer_and_exit()
    if [ -n "$FAKE_MINIPRO_PRESENCE" ]; then
      printf '%b\n' "$FAKE_MINIPRO_PRESENCE" >&2
    elif [ -n "$FAKE_MINIPRO_NO_PROGRAMMER" ]; then
      echo "[No programmer found]" >&2
    else
      echo "tl866ii: TL866II+" >&2
    fi
    exit 0
    ;;
  -l|-d)
    if [ -n "$FAKE_MINIPRO_INFO_SLEEP" ]; then
      sleep "$FAKE_MINIPRO_INFO_SLEEP"
    fi
    if [ -n "$FAKE_MINIPRO_INFO_PROMPT" ]; then
      printf 'No device found. Which database do you want to display?\n1) TL866A/CS\n2) TL866II+/T48/T56\n3) T76\n4) Abort\n' >&2
      read -r choice || choice=""
      if [ "$choice" != "2" ]; then
        echo "Aborted." >&2
        exit 1
      fi
    fi
    ;;
esac

case "$1" in
  -l)
    if [ -n "$FAKE_MINIPRO_CRLF" ]; then
      printf 'W27C512\r\nAT28C256\r\n27C256\r\nAT28C256\r\n'
    else
      printf 'W27C512\nAT28C256\n27C256\nAT28C256\n'
    fi
    exit 0
    ;;
  -d)
    if [ -n "$FAKE_MINIPRO_INFO_FAIL" ]; then
      echo "Unknown device: $2" >&2
      exit 1
    fi
    {
      echo "Found TL866II+ 04.2.132 (0x284)"
      echo "Serial code: ABCDEF0123456789"
      echo "Name: $2"
      echo "Memory: 32768 Bytes"
      echo "Package: DIP28"
      echo "Protocol: 0x05"
      echo "Read buffer size: 1024 Bytes"
      echo "Write buffer size: 128 Bytes"
    } | if [ -n "$FAKE_MINIPRO_CRLF" ]; then sed 's/$/\r/'; else cat; fi >&2
    exit 0
    ;;
esac

# Device operations: minipro -p <device> [-r|-w|-F <file>] [flags...]
while [ $# -gt 0 ]; do
  case "$1" in
    -r)
      shift
      if [ -n "$FAKE_MINIPRO_READ_EMPTY" ]; then
        : > "$1"
      elif [ -n "$FAKE_MINIPRO_READ_HEX" ]; then
        printf '%s' "$FAKE_MINIPRO_READ_HEX" | xxd -r -p > "$1"
      fi
      echo "Reading Code...  0.10Sec  OK" >&2
      ;;
    -F)
      # Like minipro, show the versions and ask for confirmation on stdin; anything but y aborts.
      echo "Found TL866II+ 04.2.126 (0x27e)" >&2
      echo "$2 contains firmware version 04.2.132 (newer)" >&2
      printf '\n\nDo you want to continue with firmware update? y/n:' >&2
      read -r answer || answer=""
      if [ "$answer" != "y" ]; then
        echo "Firmware update aborted." >&2
        exit 1
      fi
      echo "Switching to bootloader... OK" >&2
      echo "Writing firmware... OK" >&2
      echo "Firmware update completed." >&2
      ;;
  esac
  shift
done
if [ -n "$FAKE_MINIPRO_DELAY" ]; then
  sleep "$FAKE_MINIPRO_DELAY"
fi
if [ -n "$FAKE_MINIPRO_DONE_LOG" ]; then
  echo "$ARGS" >> "$FAKE_MINIPRO_DONE_LOG"
fi
if [ -n "$FAKE_MINIPRO_CRASH" ]; then
  kill -9 $$
fi
exit "${FAKE_MINIPRO_READ_EXIT:-0}"
