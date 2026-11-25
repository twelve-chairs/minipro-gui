// Stand-in for the minipro CLI, used by MainWindowTest.
// The test copies this program into a temp dir as "minipro" ("minipro.exe" on Windows) and puts
// that dir first on PATH. Plain C++ with no Qt, so it runs on Windows without Qt DLLs beside it.
//
// Environment:
//   FAKE_MINIPRO_LOG          file each invocation's arguments are appended to (one line per call)
//   FAKE_MINIPRO_NO_PROGRAMMER  if set, --presence_check reports no programmer
//   FAKE_MINIPRO_PRESENCE     if set, --presence_check prints this text instead (\n allowed)
//   FAKE_MINIPRO_READ_HEX     hex string written (as bytes) to the -r target file
//   FAKE_MINIPRO_READ_EMPTY   if set, -r writes an empty file
//   FAKE_MINIPRO_READ_EXIT    exit code for -r / -w / -F operations (default 0)
//   FAKE_MINIPRO_CRASH        if set, device operations crash (SIGKILL; an access-violation exit
//                             code on Windows)
//   FAKE_MINIPRO_DELAY        seconds a device operation takes (default 0)
//   FAKE_MINIPRO_DONE_LOG     file each device operation appends its arguments to when it ends
//   FAKE_MINIPRO_INFO_PROMPT  if set, -l and -d ask "Which database...?" on stdin like minipro
//                             does without a programmer, and abort unless answered
//   FAKE_MINIPRO_INFO_SLEEP   seconds -l and -d take (default 0)
//   FAKE_MINIPRO_INFO_FAIL    if set, -d reports an unknown device and exits 1
//   FAKE_MINIPRO_CRLF         if set, -l and -d use Windows line endings
//   FAKE_MINIPRO_LIST_EMPTY   if set, -l prints no devices
//   FAKE_MINIPRO_TL866A       if set, -F asks like minipro does for a TL866A/CS (numbered menu)
//   FAKE_MINIPRO_FW_PROMPT_UNKNOWN  if set, -F asks a question the app doesn't recognise
//   FAKE_MINIPRO_IGNORE_TERM  if set, ignore SIGTERM (only SIGKILL stops it; no effect on Windows)
//   FAKE_MINIPRO_VERSION_TEXT if set, --version prints this to stderr instead (\n, \t, \r, \e,
//                             \\ and \0NNN escapes, like printf %b)
//   FAKE_MINIPRO_INFO_FILE    if set, -d prints this file's contents to stderr instead (a file,
//                             because Windows limits an environment variable to 32,767 characters)

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define NOMINMAX  // Windows.h would otherwise turn std::min into its min() macro
#include <Windows.h>
#endif

namespace {

// The variable's value; empty if it isn't set.
std::string env(const char *name) {
#ifdef _WIN32
  char *value = nullptr;
  size_t length = 0;
  if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) return {};
  std::string result(value);
  free(value);
  return result;
#else
  const char *value = std::getenv(name);
  return value == nullptr ? std::string() : std::string(value);
#endif
}

bool isSet(const char *name) { return !env(name).empty(); }

void sleepSeconds(const std::string &seconds) {
  if (seconds.empty()) return;
  std::this_thread::sleep_for(std::chrono::duration<double>(std::stod(seconds)));
}

void appendLine(const std::string &path, const std::string &line) {
  if (path.empty()) return;
  std::ofstream(path, std::ios::app | std::ios::binary) << line << '\n';
}

// Expands escapes the way the shell fake's `printf %b` did.
std::string unescape(std::string_view text) {
  constexpr char escape = 27;
  std::string out;
  size_t i = 0;
  while (i < text.size()) {
    if (text[i] != '\\' || i + 1 == text.size()) {
      out += text[i];
      i++;
      continue;
    }
    const char c = text[i + 1];
    i += 2;  // past the backslash and c
    switch (c) {
      case 'n': out += '\n'; break;
      case 't': out += '\t'; break;
      case 'r': out += '\r'; break;
      case 'e': out += escape; break;
      case '\\': out += '\\'; break;
      case '0': {
        // \0NNN: up to three octal digits.
        const size_t end = std::min(text.size(), i + 3);
        int value = 0;
        while (i < end && text[i] >= '0' && text[i] <= '7') {
          value = value * 8 + (text[i] - '0');
          i++;
        }
        out += static_cast<char>(value);
        break;
      }
      default:
        out += '\\';
        out += c;
    }
  }
  return out;
}

// One line from stdin without its line ending; empty at end-of-input.
std::string readLine() {
  std::string line;
  if (!std::getline(std::cin, line)) return {};
  if (!line.empty() && line.back() == '\r') line.pop_back();
  return line;
}

std::string decodeHex(std::string_view hex) {
  std::string bytes;
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    bytes += static_cast<char>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16));
  }
  return bytes;
}

[[noreturn]] void crash() {
#ifdef _WIN32
  // Qt reports exit codes from 0x80000000 as a crash; 0xC0000005 is an access violation.
  TerminateProcess(GetCurrentProcess(), 0xC0000005);
#else
  std::raise(SIGKILL);
#endif
  std::abort();
}

int listOrInfo(const std::vector<std::string> &args) {
  sleepSeconds(env("FAKE_MINIPRO_INFO_SLEEP"));
  if (isSet("FAKE_MINIPRO_INFO_PROMPT")) {
    std::cerr << "No device found. Which database do you want to display?\n1) TL866A/CS\n"
                 "2) TL866II+/T48/T56\n3) T76\n4) Abort\n";
    if (readLine() != "2") {
      std::cerr << "Aborted.\n";
      return 1;
    }
  }
  const std::string eol = isSet("FAKE_MINIPRO_CRLF") ? "\r\n" : "\n";
  if (args[0] == "-l") {
    if (isSet("FAKE_MINIPRO_LIST_EMPTY")) return 0;
    for (const char *name : {"W27C512", "AT28C256", "27C256", "AT28C256"}) std::cout << name << eol;
    return 0;
  }
  if (isSet("FAKE_MINIPRO_INFO_FILE")) {
    std::ifstream info(env("FAKE_MINIPRO_INFO_FILE"), std::ios::binary);
    std::cerr << info.rdbuf();
    return 0;
  }
  const std::string device = args.size() > 1 ? args[1] : std::string();
  if (isSet("FAKE_MINIPRO_INFO_FAIL")) {
    std::cerr << "Unknown device: " << device << "\n";
    return 1;
  }
  for (const std::string &line :
       {std::string("Found TL866II+ 04.2.132 (0x284)"), std::string("Serial code: ABCDEF0123456789"),
        "Name: " + device, std::string("Memory: 32768 Bytes"), std::string("Package: DIP28"),
        std::string("Protocol: 0x05"), std::string("Read buffer size: 1024 Bytes"),
        std::string("Write buffer size: 128 Bytes")}) {
    std::cerr << line << eol;
  }
  return 0;
}

// minipro -F <file>; returns -1 to carry on, or an exit code.
int firmwareUpdate(const std::string &file) {
  if (isSet("FAKE_MINIPRO_FW_PROMPT_UNKNOWN")) {
    // A prompt worded unlike any minipro release, as a future version might ask.
    std::cerr << "Really flash the programmer? [yes/no] ";
    if (readLine() != "yes") {
      std::cerr << "Firmware update aborted.\n";
      return 1;
    }
    std::cerr << "Firmware update completed.\n";
    return -1;
  }
  if (isSet("FAKE_MINIPRO_TL866A")) {
    // tl866a.c: versions, then a numbered menu; 1 reflashes the device's own variant.
    std::cerr << "Found TL866CS 03.2.85 (0x255)\n"
              << file << " contains firmware version 03.2.86 (newer)\n"
              << "\n\nWhich firmware version do you want to reflash? \n1) Device default (CS)\n"
                 "2) A\n3) Exit\n";
    if (const std::string choice = readLine(); choice == "1") {
      std::cerr << "Reflashing TL866CS firmware... OK\nFirmware update completed.\n";
    } else if (choice == "2") {
      std::cerr << "Reflashing TL866A firmware... OK\nFirmware update completed.\n";
    } else {
      std::cerr << "Firmware update aborted.\n";
      return 1;
    }
    return -1;
  }
  // Like minipro, show the versions and ask for confirmation on stdin; anything but y aborts.
  std::cerr << "Found TL866II+ 04.2.126 (0x27e)\n"
            << file << " contains firmware version 04.2.132 (newer)\n"
            << "\n\nDo you want to continue with firmware update? y/n:";
  if (readLine() != "y") {
    std::cerr << "Firmware update aborted.\n";
    return 1;
  }
  std::cerr << "Switching to bootloader... OK\nWriting firmware... OK\nFirmware update completed.\n";
  return -1;
}

// --version and --presence_check.
int query(const std::string &first) {
  if (first == "--version") {
    if (isSet("FAKE_MINIPRO_VERSION_TEXT")) {
      std::cerr << unescape(env("FAKE_MINIPRO_VERSION_TEXT"));
    } else {
      std::cerr << "minipro version 0.7.4     A free and open TL866 series programmer\n"
                   "Commit date:\t2024-01-01 00:00:00 +0000\n";
    }
    return 0;
  }
  // Same wording as minipro's print_connected_programmer_and_exit()
  if (isSet("FAKE_MINIPRO_PRESENCE")) {
    std::cerr << unescape(env("FAKE_MINIPRO_PRESENCE")) << "\n";
  } else if (isSet("FAKE_MINIPRO_NO_PROGRAMMER")) {
    std::cerr << "[No programmer found]\n";
  } else {
    std::cerr << "tl866ii: TL866II+\n";
  }
  return 0;
}

void readTo(const std::string &target) {
  if (isSet("FAKE_MINIPRO_READ_EMPTY")) {
    std::ofstream(target, std::ios::binary | std::ios::trunc);
  } else if (isSet("FAKE_MINIPRO_READ_HEX")) {
    std::ofstream(target, std::ios::binary | std::ios::trunc) << decodeHex(env("FAKE_MINIPRO_READ_HEX"));
  }
  std::cerr << "Reading Code...  0.10Sec  OK\n";
}

// Device operations: minipro -p <device> [-r|-w|-F <file>] [flags...]
int deviceOperation(const std::vector<std::string> &args, const std::string &joined) {
  size_t i = 0;
  while (i < args.size()) {
    const bool hasValue = i + 1 < args.size();
    if (args[i] == "-r" && hasValue) {
      readTo(args[i + 1]);
      i++;  // skip the file
    } else if (args[i] == "-F") {
      const int code = firmwareUpdate(hasValue ? args[i + 1] : std::string());
      if (code >= 0) return code;
      i++;  // skip the file
    }
    i++;
  }
  sleepSeconds(env("FAKE_MINIPRO_DELAY"));
  appendLine(env("FAKE_MINIPRO_DONE_LOG"), joined);
  if (isSet("FAKE_MINIPRO_CRASH")) crash();
  if (const std::string code = env("FAKE_MINIPRO_READ_EXIT"); !code.empty()) return std::stoi(code);
  return 0;
}

}  // namespace

int main(int argc, char *argv[]) {
#ifdef _WIN32
  // Write exactly the bytes asked for: text mode would turn every \n into \r\n.
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif
  const std::vector<std::string> args(argv + 1, argv + argc);
  std::string joined;
  for (const std::string &arg : args) joined += (joined.empty() ? "" : " ") + arg;

#ifndef _WIN32
  if (isSet("FAKE_MINIPRO_IGNORE_TERM")) std::signal(SIGTERM, SIG_IGN);
#endif
  appendLine(env("FAKE_MINIPRO_LOG"), joined);

  const std::string first = args.empty() ? std::string() : args[0];
  if (first == "--version" || first == "--presence_check") return query(first);
  if (first == "-l" || first == "-d") return listOrInfo(args);
  return deviceOperation(args, joined);
}
