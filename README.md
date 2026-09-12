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

# Run with QEMU (Linux)
qemu-system-x86_64 -m 1G -cdrom ~/lightos_project/lightos.iso -boot d -accel kvm -display sdl

# Run with QEMU (Windows)
"C:\Program Files\qemu\qemu-system-x86_64.exe" -m 1G -cdrom C:\lightos_project\lightos.iso -boot d -accel whpx -display sdl

# Run with QEMU (macOS)
qemu-system-x86_64 -m 1G -cdrom ~/lightos_project/lightos.iso -boot d -accel hvf -display sdl
```

> **Note:** The kernel runs in 64-bit long mode. `qemu-system-i386` does not support
> long mode in TCG mode and will trigger continuous triple faults leading to constant
> resets — make sure to use `qemu-system-x86_64`.

> **IMPORTANT — `-hda lightdisk.img` is required to save files:**
> Anything you write in Notepad, generate with Luigi, or create via Files is stored
> on CandleFS (the disk filesystem). If you run without the `-hda lightdisk.img` flag,
> there will be no disk access — even if pressing Ctrl+S seems to "save", nothing will
> persist and everything will reset on every reboot (even if the file size displays
> correctly on screen, the contents will never be recovered). Make sure to use the
> `lightdisk.img` file in the project folder and supply the same `-hda lightdisk.img`
> argument every time you launch QEMU to ensure changes persist.

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
 9 Apps + 10            ↓
 DevTools            VFS (RAM)
```

## Author

**Xaef BTL** — 2026
