#!/bin/bash
set -e
PROJECT_DIR="$(cd .. && pwd)"
WIN_OUT="/mnt/c/lightos_project"
VBOXMANAGE="/mnt/c/Program Files/Oracle/VirtualBox/VBoxManage.exe"

echo "=== LightOS Bootloader Build ==="

echo "[1/5] Stage 1..."
/usr/bin/nasm -f bin stage1.asm -o stage1.bin

echo "[2/5] Stage 2..."
/usr/bin/nasm -f bin stage2.asm -o stage2.bin

echo "[3/5] Kernel build..."
cd "$PROJECT_DIR" && ./build.sh
cd bootloader

echo "[4/5] Disk imajı..."
dd if=/dev/zero of=lightos.img bs=1M count=64 status=none
dd if=stage1.bin of=lightos.img bs=512 seek=0  conv=notrunc status=none
dd if=stage2.bin of=lightos.img bs=512 seek=1  conv=notrunc status=none
# Kernel boyutunu hesapla ve yaz (max 2MB)
KERN_SIZE=$(stat -c%s "$PROJECT_DIR/kernel.bin" 2>/dev/null || echo 0)
echo "    Kernel boyutu: ${KERN_SIZE} bytes"
dd if="$PROJECT_DIR/kernel.bin" of=lightos.img bs=512 seek=34 conv=notrunc status=none
echo "    lightos.img: $(du -sh lightos.img | cut -f1)"

echo "[5/5] Windows VDI..."
mkdir -p "$WIN_OUT"
cp lightos.img "$WIN_OUT/"
if [ -f "$VBOXMANAGE" ]; then
    # Mevcut VDI varsa unregister et (UUID çakışma önlemi)
    "$VBOXMANAGE" closemedium disk "C:\\lightos_project\\lightos.vdi" \
        --delete 2>/dev/null || true
    rm -f "$WIN_OUT/lightos.vdi"
    "$VBOXMANAGE" convertdd "C:\\lightos_project\\lightos.img" \
        "C:\\lightos_project\\lightos.vdi" --format VDI 2>/dev/null && \
        echo "    lightos.vdi hazir!" || \
        echo "    VDI donusurum basarisiz, IMG kullan"
else
    echo "    VBoxManage bulunamadi. PowerShell:"
    echo "    VBoxManage convertdd C:\\lightos_project\\lightos.img C:\\lightos_project\\lightos.vdi --format VDI"
fi

echo ""
echo "=== VirtualBox VM Ayarlari ==="
echo "  Tip: Other/Unknown 32-bit"
echo "  RAM: 256MB"
echo "  Disk: lightos.vdi (IDE Primary Master)"
echo "  IO APIC: KAPALI"
echo "  Video: VMSVGA, 64MB"
echo "  Ses: ICH AC97 + DirectSound"
