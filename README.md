<div align="center">

<h1>Tin</h1>

> Hello? Does any one hear me??

[![Language](https://img.shields.io/badge/language-C17-blue.svg)](https://en.wikipedia.org/wiki/C17_(C_standard_revision))
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](#requirements)
[![Crypto](https://img.shields.io/badge/crypto-libsodium-green.svg)](https://doc.libsodium.org/)
[![License](https://img.shields.io/badge/license-GPL--3.0-orange.svg)](LICENSE.md)

</div>

---

## Table of Contents

- [About](#about)
- [How It Works](#how-it-works)
- [Requirements](#requirements)
- [Building](#building)
- [Demo](#demo)
- [Usage](#usage)
  - [Roles](#roles)
  - [Capabilities](#capabilities)
  - [Examples](#examples)
- [Keys and Configuration](#keys-and-configuration)
- [Development](#development)
  - [Tests](#tests)
  - [Coverage](#coverage)
  - [Formatting](#formatting)
  - [Editor Integration](#editor-integration)

---

## About

Tin is a small peer-to-peer communication tool for two machines on the same network. One
side hosts, the other connects, and everything between them travels encrypted
over a single TCP connection.

There is no server to sign up for, no account, and no discovery service. Each
node owns a long-term Ed25519 identity generated on first run, and every session
derives fresh keys from it. It is a tin-can telephone with a real cryptographic
string.

## How It Works

Tin is symmetric once the connection is up. The only asymmetry is who binds the
listening socket and who dials it.

```
   host                                        connect
  ------                                       ---------
  bind + listen                                connect
       \                                          /
        \-------- TCP accept / connect ----------/
                          |
              signed handshake, both ways
                          |
             X25519 -> per-direction session keys
                          |
              ChaCha20-Poly1305 encryption 
```

Frames are captured at exactly 1920 bytes, which is 20 ms of 48 kHz 16-bit mono
PCM. Each frame is wrapped in an audio packet carrying a version byte and a
32-bit sequence number, encrypted, and written as `[nonce][ciphertext || tag]`.
Because both ends know the plaintext size in advance, no length prefix is
needed on the wire.

The capture side computes the RMS amplitude of every frame and drops anything at
or below -50 dBFS, keeping a ten-frame hangover so word endings are not clipped.
The playback side logs sequence gaps rather than trying to conceal them.

## Requirements

Linux, plus the following development packages:

| Dependency  | Used for                                   |
| ----------- | ------------------------------------------ |
| libsodium   | Ed25519, X25519, ChaCha20-Poly1305         |
| PulseAudio  | Capture and playback (`libpulse-simple`)   |
| ncurses     | Interactive source selection menus         |
| libudev     | Input device discovery                     |
| pkg-config  | Locating libsodium at build time           |

Debian and Ubuntu:

```sh
sudo apt install build-essential pkg-config libsodium-dev \
                 libpulse-dev libncurses-dev libudev-dev
```

Fedora:

```sh
sudo dnf install gcc make pkgconf-pkg-config libsodium-devel \
                 pulseaudio-libs-devel ncurses-devel systemd-devel
```

Arch:

```sh
sudo pacman -S base-devel pkgconf libsodium libpulse ncurses systemd-libs
```

Optional: `clang-format` for the formatting targets, and `lcov` for coverage.

## Building

```sh
make
```

That produces the `tin` binary in the project root and regenerates
`compile_commands.json`. Sources and headers are discovered recursively, so new
files under `src/` and `include/` are picked up without editing the Makefile.

```sh
make clean
```

## Demo

https://github.com/user-attachments/assets/26d96251-a732-42a7-bc55-c6e070809618

## Usage

```
./tin <role> <addr[:port]> <caps>
```

Addresses are IPv4 literals. The default port is `6969` when none is given, and
`host` binds to whatever address you pass, so use `0.0.0.0` to accept a peer on
any interface.

### Roles

| Role      | Meaning                                       |
| --------- | --------------------------------------------- |
| `host`    | Bind and listen for one incoming peer         |
| `connect` | Dial the given address                        |

### Capabilities

Capabilities are a comma-separated list describing what this node contributes to
the call.

| Capability | Meaning                                                        |
| ---------- | -------------------------------------------------------------- |
| `mic`      | Capture from a local source and send it to the peer            |
| `speaker`  | Receive from the peer and play it back locally                 |
| `input`    | Reserved for input forwarding                                  |

Passing `mic` opens an ncurses menu listing the available capture sources so you
can pick one before the call starts.

### Examples

Host a call and speak into a local microphone:

```sh
./tin host 0.0.0.0 mic
```

Connect from the other machine and listen:

```sh
./tin connect 192.168.1.20 speaker
```

Use a non-default port:

```sh
./tin connect 192.168.1.20:7000 speaker
```

Stop either side with `Ctrl-C`. The signal handler tears the socket down so the
blocked audio thread wakes up and exits cleanly.

## Keys and Configuration

Tin keeps its identity in `$HOME/.config/tin`, creating the directory with mode
`0700` on first run:

| File          | Mode   | Contents                          |
| ------------- | ------ | --------------------------------- |
| `private.pem` | `0600` | Ed25519 secret key, base64 in PEM |
| `public.pem`  | `0644` | Ed25519 public key, base64 in PEM |

On startup the pair is loaded and validated: the public key must be derivable
from the secret key, and a half-present pair is treated as an error rather than
silently regenerated. Delete both files to rotate your identity.

## Development

### Tests

```sh
make test
```

Every production object except `main.o` is linked into the test runner, which is
built on the vendored [greatest](https://github.com/silentbicycle/greatest)
header.

### Coverage

Requires `lcov`:

```sh
make coverage
```

The HTML report lands in `coverage/html/index.html`.

### Formatting

```sh
make format        # rewrite sources in place
make format-check  # fail if anything is unformatted
```

The vendored `tests/greatest.h` is excluded from both.

### Editor Integration

`compile_commands.json` is regenerated by `make` whenever sources are added or
`CFLAGS` change, so clangd always sees the include path. To rebuild it alone:

```sh
make compdb
```
