<img width="1007" height="761" alt="image" src="https://github.com/user-attachments/assets/561185a5-e3a4-47c8-a690-07c42f6d422f" />

# LightOS 2 Pro

> A bare-metal x86 operating system built from scratch — no Linux, no POSIX, pure metal.

---

## Features

- **Custom GUI** — window compositing, shadows, animations, maximize/resize
- **9 Applications** — Notepad, Files, Calculator, Settings, Terminal, Paint, Clock, SysInfo, LightSurf
- **LightSurf Browser** — real HTTP client with RTL8139 NIC + TCP/IP + DNS
- **2 Themes** — Luna Steel, ColdIceBar (frosted glass)
- **3 Wallpapers** — Luna Night, Dawn, Forest
- **VFS** — RAM-based filesystem (128 nodes)
- **ACPI** — real hardware shutdown & restart
- **Audio** — AC97 + PC Speaker fallback
- **Serial Debug** — COM1 UART logging
- **Developer Mode** — 10 dev tools (F4×5 at boot)

## Quick Start

```bash
# Install dependencies (Ubuntu/WSL2)
sudo apt install build-essential nasm grub-pc-bin grub-common xorriso

# Build
./build.sh

# Run with QEMU (with internet!)
qemu-system-x86_64 -cdrom LightOS.iso -m 256M -vga std \
  -device rtl8139,netdev=n0 -netdev user,id=n0
```

> note: If you use "qemu-system-i386" instead "qemu-system-x86_64", the system will reset and give a error. use the x86_64 one.

## Documentation

See **[HOW-TO-USE.md](HOW-TO-USE.md)** for complete documentation.

## Architecture

```
boot.asm → kernel/init.c → start_shell()
              ↓
         GUI Loop (shell.cpp)
              ↓
    ┌─────────┴──────────┐
    │                    │
 Windows              Drivers
 compositor        PS/2 | RTL8139
    │              UART | AC97
 9 Apps + 10         ↓
 DevTools           VFS (RAM)
```

## Author

**Xaef BTL** — 2026
