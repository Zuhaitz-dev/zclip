# zclip

A minimal peer-to-peer clipboard synchronization utility for Windows 11, written in modern C++23.

It keeps the clipboards of two machines on the same private network in sync: copy on either machine, paste on the other.

## Features

- Native clipboard monitoring via `WM_CLIPBOARDUPDATE` (no polling).
- Bidirectional sync of Unicode text (UTF-16 on the clipboard, UTF-8 on the wire).
- Loop prevention and deduplication against clipboard-change echoes.
- Automatic reconnect after a temporary connection loss.
- AES-256-GCM authenticated encryption with a shared secret.
- Simple CLI pairing and console-based connection status.

## Requirements

- Windows 11 (latest stable)
- Visual Studio 2026 (MSVC) and CMake 3.25+

## Build

```cmd
cmake -B build -S .
cmake --build build --config Release
```

Or use the helper scripts (`task.bat` / `task.ps1`):

```cmd
task build
```

## Run & Pair Two Machines

Machine A (server):

```cmd
zclip.exe --server --port 6767 --secret "your-shared-secret"
```

Machine B (client):

```cmd
zclip.exe --ip 192.168.1.50 --port 6767 --secret "your-shared-secret"
```

Both machines must use the same `--secret`. Allow inbound TCP on the chosen port through Windows Firewall for the private network profile.

### Command-Line Options

| Option | Description |
|---|---|
| `--server` | Run as the listening server (default: client mode) |
| `--ip <addr>` | Peer IP to connect to (client mode only) |
| `--port <n>` | TCP port to bind or connect to (default 6767) |
| `--secret <psk>` | Enable AES-256-GCM encryption with this shared secret |
| `-h`, `--help` | Show usage |

If no `--secret` is given the tool runs unencrypted and prints a warning.

## Architecture

Six small, testable modules:

| Module | Responsibility |
|---|---|
| `clipboard_guard` | RAII wrappers for the Win32 clipboard API and UTF-16 read/write |
| `clipboard_listener` | Message-only window listening for `WM_CLIPBOARDUPDATE` |
| `loop_preventer` | Hash-based dedup ring buffer + remote-change suppression |
| `framing` | Length-prefixed frame encode/decode with a size cap |
| `crypto` | AES-256-GCM via the Windows Cryptography API (CNG) |
| `net_peer` | Async TCP peer: accept or connect, frame RX loop, send |

Data flow: OS clipboard change -> `clipboard_listener` -> `loop_preventer` (dedup) -> UTF-8 encode -> `crypto` (optional) -> `framing` -> `net_peer` -> TCP. The reverse path applies received frames to the local clipboard while seeding `loop_preventer`, so the echo never bounces back.

## Protocol

Length-prefixed frames over TCP:

```
+-----------------------+-------------------------------+
| Length (4 bytes)      | Payload (Length bytes, UTF-8)  |
| Big-Endian uint32_t   | Raw text (optionally encrypted)|
+-----------------------+-------------------------------+
```

- Big-endian length; max payload 10 MB (guards against malformed frames).
- Text is UTF-8 on the wire, UTF-16 on the Windows clipboard.
- With a shared secret the payload is `[IV 12 bytes] + [ciphertext] + [GCM tag 16 bytes]`; framing treats it as opaque bytes.

## Security Approach

- AES-256-GCM authenticated encryption via CNG; the key is the SHA-256 of the shared secret.
- A random 12-byte IV per message. The 16-byte GCM tag authenticates each frame, so a peer without the correct secret can neither read nor inject frames (tampered frames are dropped).
- Trade-offs:
  - Shared-secret (PSK) only: no key exchange or certificates. Fine for a trusted private network, not for the Internet.
  - The GCM tag gives implicit authentication, but there is no explicit handshake/key-confirmation at connect time.
  - CNG avoids third-party dependencies but is Windows-only and verbose; libsodium/OpenSSL would be more portable.
  - The key is a plain hash of the secret (no KDF salt); acceptable for a demo, a real tool would use PBKDF2/Argon2.

## Automated Tests

```cmd
ctest --test-dir build -C Release --output-on-failure
```

- `LoopPreventerTest` - dedup and suppression behavior.
- `FramingTest` - round-trip, Unicode, TCP chunking, multiple frames.
- `CryptoTest` - encrypt/decrypt round-trip, wrong-secret and tamper rejection.
- `NetPeerTest` - bidirectional delivery over a real loopback TCP socket.

CI (GitHub Actions) builds and runs the tests on `windows-latest`, plus an MSVC AddressSanitizer job.

## Known Limitations & Future Work

- Only one peer connection at a time; no multiple/broadcast peers.
- Synchronous `send`; a stalled peer can block the clipboard listener.
- Clipboard text is truncated at the first embedded NUL.
- `send_payload`/`disconnect` are not yet synchronized with a lock.
- No tray icon / GUI; status is console text only.
- No graceful Ctrl+C shutdown handling.
- With more time: a proper handshake (TLS or PAKE), a tray app, config file, reconnect backoff, multiple peers, and a platform abstraction layer (Linux support).

## Time Spent

I have worked on this from Tuesday, 26th August, until Wednesday, 27th August. I have surely spent 8-9 hours on this, as I have had a few breaks between the phases.

## AI Use

AI tooling was used for low-risk scaffolding only: the `task.bat` / `task.ps1` helper scripts, the `.clang-format` / `.clang-tidy` configs were generated/copied. Same applies to the Github Actions, specifically to make `ASAN` work with Windows. Tests have also been written with these tools, and then verified. This `README.md` was written with AI, but mainly extracting information from the hand-written code and notes that are inline in the source comments. Of course, AI has been used for checking information online, specific APIs, and how to implement specific parts, same way as places like Stack Overflow have been used for the same purpose. Also, after implementing the modules, AI tooling has been used to verify the whole architecture, and to catch possible issues that were not detected by other tools, such as the code analyzer and the sanitizers.

For documentation, `Gemini` was mainly used. For more technical parts, `DeepSeek V4 Flash` was used.