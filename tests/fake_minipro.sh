#!/bin/sh
# Stand-in for the minipro CLI, used by MainWindowTest.
# The test copies this script into a temp dir as "minipro" and puts that dir first on PATH.
#
# Environment:
#   FAKE_MINIPRO_LOG          file each invocation's arguments are appended to (one line per call)
#   FAKE_MINIPRO_NO_PROGRAMMER  if set, --presence_check reports no programmer
#   FAKE_MINIPRO_READ_HEX     hex string written (as bytes) to the -r target file
#   FAKE_MINIPRO_READ_EXIT    exit code for -r / -w / -F operations (default 0)

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
    if [ -n "$FAKE_MINIPRO_NO_PROGRAMMER" ]; then
      echo "No programmer found." >&2
    else
      echo "Found programmer: TL866II+" >&2
    fi
    exit 0
    ;;
  -l)
    printf 'W27C512\nAT28C256\n27C256\nAT28C256\n'
    exit 0
    ;;
  -d)
    {
      echo "Found TL866II+ 04.2.132 (0x284)"
      echo "Serial code: ABCDEF0123456789"
      echo "Name: $2"
      echo "Memory: 32768 Bytes"
      echo "Package: DIP28"
      echo "Protocol: 0x05"
      echo "Read buffer size: 1024 Bytes"
      echo "Write buffer size: 128 Bytes"
    } >&2
    exit 0
    ;;
esac

# Device operations: minipro -p <device> [-r|-w|-F <file>] [flags...]
while [ $# -gt 0 ]; do
  case "$1" in
    -r)
      shift
      if [ -n "$FAKE_MINIPRO_READ_HEX" ]; then
        printf '%s' "$FAKE_MINIPRO_READ_HEX" | xxd -r -p > "$1"
      fi
      echo "Reading Code...  0.10Sec  OK" >&2
      ;;
  esac
  shift
done
exit "${FAKE_MINIPRO_READ_EXIT:-0}"
