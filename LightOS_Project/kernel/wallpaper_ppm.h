#ifndef WALLPAPER_PPM_H
#define WALLPAPER_PPM_H
/*
 * kernel/wallpaper_ppm.h — Özel (kullanıcı) duvar kağıdı: PPM (P6) desteği
 * ==========================================================================
 * LightOS'ta PNG/JPEG decode etmek için gereken zlib/inflate + Huffman/DCT
 * kütüphaneleri yok (bare-metal kernel'e portlamak başlı başına büyük bir
 * iştir). Bunun yerine ham, sıkıştırılmamış PPM (P6) formatını okuyoruz —
 * decode etmek onlarca satır, formatın kendisi de yaygın araçlarla
 * (ImageMagick, GIMP, ffmpeg) PNG/JPG'den saniyeler içinde üretilebilir:
 *
 *   convert wallpaper.jpg -resize 1024x768! -type TrueColor wallpaper.ppm
 *
 * Diskte yer: PPM verisi CandleFS'in kullandığı alanın (LBA 1..~4354,
 * bkz. kernel/candlefs.h) ÇOK ötesinde, sabit bir bölgede (LBA 5000+)
 * tutulur — CandleFS büyüse bile asla çakışmaz. Format diskte:
 *   LBA 2000        : magic(4) + width(4) + height(4) + data_bytes(4)
 *   LBA 2001..      : ham RGB piksel verisi (width*height*3 byte)
 *
 * Ekran boyutuyla PİKSEL PİKSEL eşleşen bir PPM bekleniyor (varsayılan
 * 1024x768) — kernel içinde ölçekleme/resample yapmıyoruz, bu da kodu
 * basit ve hızlı tutuyor; farklı bir çözünürlük istenirse dönüştürme
 * aracı (yukarıdaki convert komutu) zaten -resize ile hedef boyuta göre
 * üretebilir.
 */
#include "vfs.h"
#include "../drivers/ata.h"

#define WP_PPM_MAGIC     0x50504D57u /* "WMPP" little-endian */
#define WP_PPM_HEADER_LBA 5000u
#define WP_PPM_DATA_LBA   5001u

struct WallpaperPPMHeader {
    uint32_t magic;
    uint32_t width;
    uint32_t height;
    uint32_t data_bytes; /* width*height*3 */
};

/* Diskte kayıtlı özel bir duvar kağıdı var mı? Varsa header'ı doldurur. */
static bool wp_ppm_probe(WallpaperPPMHeader& out){
    if(!g_disk.present) return false;
    uint8_t sec[512];
    if(!g_disk.read_sector(WP_PPM_HEADER_LBA, sec)) return false;
    uint32_t magic = *(uint32_t*)(sec+0);
    if(magic != WP_PPM_MAGIC) return false;
    out.magic       = magic;
    out.width       = *(uint32_t*)(sec+4);
    out.height      = *(uint32_t*)(sec+8);
    out.data_bytes  = *(uint32_t*)(sec+12);
    return true;
}

/* Diskteki ham RGB veriyi doğrudan backbuffer'a çizer (0..SW,0..SH ile
 * kırpılır — PPM ekrandan büyük/küçükse taşma/eksik kalma olmaz, sadece
 * ortak alan kadarı gösterilir). */
static bool wp_ppm_draw(){
    WallpaperPPMHeader hdr;
    if(!wp_ppm_probe(hdr)) return false;
    if(hdr.width==0||hdr.height==0||hdr.width>4096||hdr.height>4096) return false;

    uint32_t total_bytes = hdr.width*hdr.height*3u;
    if(total_bytes != hdr.data_bytes) return false; /* bozuk/eksik dosya */

    uint32_t sectors = (total_bytes+511u)/512u;
    uint8_t rowbuf[512*8]; /* tek seferde okunacak sektör grubu (4KB) */
    const uint32_t CHUNK_SECTORS = 8;
    const uint32_t CHUNK_BYTES = CHUNK_SECTORS*512u; /* 4096 — 3'e bölünmez! */

    uint32_t px=0, py=0; /* backbuffer'daki mevcut yazma pozisyonu */
    uint8_t pending[3]; int pending_n=0; /* chunk sınırında yarım kalan RGB byte'ları */

    for(uint32_t s=0; s<sectors; s+=CHUNK_SECTORS){
        uint32_t this_chunk = (sectors-s<CHUNK_SECTORS)?(sectors-s):CHUNK_SECTORS;
        if(!g_disk.read(WP_PPM_DATA_LBA+s, this_chunk, rowbuf)) return false;
        uint32_t bytes_in_chunk = this_chunk*512u;
        /* Son chunk, dosyanın gerçek sonundan taşabilir (sektör hizalama) —
         * sadece gerçek veri kadarını işle. */
        uint32_t remaining_total = total_bytes - (uint32_t)s*512u;
        if(bytes_in_chunk > remaining_total) bytes_in_chunk = remaining_total;

        uint32_t i=0;
        /* Önceki chunk'tan kalan yarım RGB varsa önce onu tamamla —
         * bu satır olmadan her 4096 byte'ta bir (CHUNK_BYTES 3'e tam
         * bölünmediği için) R/G/B byte'ları kayıyor, renkler bozuluyordu
         * (örn. sarı bir güneş yeşile/mora kayabiliyordu). */
        while(pending_n>0 && i<bytes_in_chunk){
            pending[pending_n++]=rowbuf[i++];
            if(pending_n==3){
                uint8_t r=pending[0], g=pending[1], b=pending[2];
                if(px<SW && py<SH) backbuffer[py*SW+px] = ((uint32_t)r<<16)|((uint32_t)g<<8)|b;
                px++; if(px>=hdr.width){ px=0; py++; }
                pending_n=0;
            }
        }
        for(; i+3<=bytes_in_chunk; i+=3){
            uint8_t r=rowbuf[i], g=rowbuf[i+1], b=rowbuf[i+2];
            if(px<SW && py<SH){
                backbuffer[py*SW+px] = ((uint32_t)r<<16)|((uint32_t)g<<8)|b;
            }
            px++;
            if(px>=hdr.width){ px=0; py++; }
        }
        /* Kalan 1-2 byte varsa (chunk RGB sınırında tam bitmediyse)
         * bir sonraki chunk için sakla. */
        pending_n=0;
        for(; i<bytes_in_chunk; i++) pending[pending_n++]=rowbuf[i];
    }
    return true;
}

/* Bir PPM'i diske yükler (host tarafında hazırlanmış veriden). Bu fonksiyon
 * kernel içinden DEĞİL, ancak ileride bir "duvar kağıdı yükleyici" araç/
 * uygulaması eklenirse ondan çağrılabilir diye burada tutulur. Şu an için
 * PPM'in diske konması host tarafında (build/deploy sırasında) yapılır —
 * bkz. tools/install_wallpaper.py. */
static bool wp_ppm_install(const uint8_t* rgb_data, uint32_t width, uint32_t height){
    if(!g_disk.present) return false;
    uint32_t total_bytes = width*height*3u;
    uint8_t sec[512];
    for(int i=0;i<512;i++) sec[i]=0;
    *(uint32_t*)(sec+0)  = WP_PPM_MAGIC;
    *(uint32_t*)(sec+4)  = width;
    *(uint32_t*)(sec+8)  = height;
    *(uint32_t*)(sec+12) = total_bytes;
    if(!g_disk.write_sector(WP_PPM_HEADER_LBA, sec)) return false;

    uint32_t sectors = (total_bytes+511u)/512u;
    uint8_t buf[512];
    for(uint32_t s=0;s<sectors;s++){
        uint32_t off=s*512u;
        uint32_t n = (total_bytes-off<512u)?(total_bytes-off):512u;
        for(uint32_t i=0;i<512;i++) buf[i]=0;
        for(uint32_t i=0;i<n;i++) buf[i]=rgb_data[off+i];
        if(!g_disk.write_sector(WP_PPM_DATA_LBA+s, buf)) return false;
    }
    return true;
}

/* Diskten özel duvar kağıdını kaldırır (varsayılana dönmek için). */
static bool wp_ppm_remove(){
    if(!g_disk.present) return false;
    uint8_t sec[512];
    for(int i=0;i<512;i++) sec[i]=0; /* magic=0 -> probe artık bulamaz */
    return g_disk.write_sector(WP_PPM_HEADER_LBA, sec);
}
#endif
