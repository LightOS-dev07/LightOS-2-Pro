#!/bin/bash
set -e

# ── 64-bit flags ──────────────────────────────────────────────
# mcmodel=small: code/data within first 2GB → 1MB load adresi OK
# mno-red-zone: interrupt güvenliği
CFLAGS="-ffreestanding -O2 -fno-builtin -nostdlib \
        -fno-stack-protector -fno-pie -fno-common \
        -msse2 -mfpmath=sse -mcmodel=small -mno-red-zone"
FLAGS="$CFLAGS -fno-exceptions -fno-rtti -fno-threadsafe-statics"

echo "==========================================="
echo "  LightOS 2 Pro — Build System [64-bit]"
echo "==========================================="

echo "[1/7] Boot..."
nasm -f elf64 boot.asm -o boot.o

echo "[2/7] Kernel..."
gcc $CFLAGS -c kernel/init.c -o kernel.o

echo "[3/7] Drivers..."
g++ $FLAGS -c drivers/io.cpp                -o io.o
g++ $FLAGS -c drivers/keyboard/keyboard.cpp -o keyboard.o
g++ $FLAGS -c drivers/mouse/mouse.cpp       -o mouse.o

echo "[4/7] Apps..."
g++ $FLAGS -c apps/notepad.cpp    -o notepad.o
g++ $FLAGS -c apps/calculator.cpp -o calculator.o
g++ $FLAGS -c apps/filemgr.cpp    -o filemgr.o
g++ $FLAGS -c apps/settings.cpp   -o settings.o
g++ $FLAGS -c apps/terminal.cpp   -o terminal.o
g++ $FLAGS -c apps/paint.cpp      -o paint.o
g++ $FLAGS -c apps/sysinfo.cpp    -o sysinfo.o
g++ $FLAGS -c apps/clock_app.cpp  -o clock_app.o

echo "[5/7] Shell..."
g++ $FLAGS -c shell.cpp -o shell.o

echo "[6/7] Link..."
ld -T linker.ld -o kernel.bin \
    boot.o kernel.o io.o keyboard.o mouse.o shell.o \
    notepad.o calculator.o filemgr.o settings.o \
    terminal.o paint.o sysinfo.o clock_app.o

echo "[7/7] ISO..."
mkdir -p iso_root/boot/grub
cp kernel.bin iso_root/boot/kernel.bin
cp grub.cfg   iso_root/boot/grub/grub.cfg
grub-mkrescue -o LightOS.iso iso_root 2>/dev/null

echo "==========================================="
echo " LightOS.iso hazir! [64-bit Long Mode]"
echo "==========================================="
