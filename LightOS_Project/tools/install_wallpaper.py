#!/usr/bin/env python3
"""
install_wallpaper.py — PNG/JPG fotoğrafını LightOS disk imajına gömer.

Kullanım:
    python3 install_wallpaper.py <resim.png|jpg> <lightdisk.img>

Ne yapar:
  1. Verilen resmi 1024x768'e (LightOS'un ekran çözünürlüğü) kırparak/
     doldurarak ölçekler (en-boy oranı bozulmasın diye "cover" mantığı
     kullanılır: resim önce oranı koruyarak büyütülür, taşan kısımlar
     ortadan kırpılır — tıpkı CSS'teki background-size: cover gibi).
  2. Ham RGB (P6 PPM) formatına çevirir.
  3. kernel/wallpaper_ppm.h'daki WP_PPM_HEADER_LBA / WP_PPM_DATA_LBA
     sektörlerine, aynı formatta (magic + width + height + data_bytes
     header'ı, ardından ham RGB veri) yazar.

Diskteki format kernel/wallpaper_ppm.h ile birebir eşleşmelidir; biri
değişirse diğeri de güncellenmelidir.

Gereksinim: pip install Pillow
"""
import sys
import struct

WP_PPM_MAGIC     = 0x50504D57  # "WMPP" little-endian, wallpaper_ppm.h ile aynı
WP_PPM_HEADER_LBA = 5000
WP_PPM_DATA_LBA   = 5001
SECTOR = 512
TARGET_W, TARGET_H = 1024, 768  # LightOS ekran çözünürlüğü


def load_and_cover(path, target_w, target_h):
    from PIL import Image
    img = Image.open(path).convert("RGB")
    src_w, src_h = img.size
    src_ratio = src_w / src_h
    dst_ratio = target_w / target_h

    if src_ratio > dst_ratio:
        # kaynak daha geniş: yüksekliğe göre ölçekle, yanlardan kırp
        new_h = target_h
        new_w = int(new_h * src_ratio)
    else:
        # kaynak daha dar/uzun: genişliğe göre ölçekle, üst-alttan kırp
        new_w = target_w
        new_h = int(new_w / src_ratio)

    img = img.resize((new_w, new_h), Image.LANCZOS)
    left = (new_w - target_w) // 2
    top = (new_h - target_h) // 2
    img = img.crop((left, top, left + target_w, top + target_h))
    return img


def main():
    if len(sys.argv) != 3:
        print(f"Kullanım: {sys.argv[0]} <resim.png|jpg> <lightdisk.img>")
        sys.exit(1)

    img_path, disk_path = sys.argv[1], sys.argv[2]

    print(f"Resim yükleniyor: {img_path}")
    img = load_and_cover(img_path, TARGET_W, TARGET_H)
    rgb_data = img.tobytes()  # RGB, satır satır, sıkıştırmasız
    assert len(rgb_data) == TARGET_W * TARGET_H * 3

    print(f"Ölçeklendi: {TARGET_W}x{TARGET_H}, {len(rgb_data)} byte")

    with open(disk_path, "r+b") as f:
        # Header sektörü
        header = bytearray(SECTOR)
        struct.pack_into("<I", header, 0, WP_PPM_MAGIC)
        struct.pack_into("<I", header, 4, TARGET_W)
        struct.pack_into("<I", header, 8, TARGET_H)
        struct.pack_into("<I", header, 12, len(rgb_data))
        f.seek(WP_PPM_HEADER_LBA * SECTOR)
        f.write(header)

        # Veri sektörleri
        f.seek(WP_PPM_DATA_LBA * SECTOR)
        f.write(rgb_data)
        # Son sektörü 512'ye tamamla (padding)
        remainder = len(rgb_data) % SECTOR
        if remainder:
            f.write(b"\x00" * (SECTOR - remainder))

    total_sectors = 1 + (len(rgb_data) + SECTOR - 1) // SECTOR
    print(f"Diske yazıldı: LBA {WP_PPM_HEADER_LBA}..{WP_PPM_HEADER_LBA+total_sectors-1}")
    print("Tamamlandı. LightOS içinde Settings → Wallpaper → Custom (photo) → Apply.")


if __name__ == "__main__":
    main()
