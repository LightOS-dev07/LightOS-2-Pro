#ifndef ACPI_H
#define ACPI_H
/*
 * kernel/acpi.h — ACPI Power Management
 * ==========================================================================
 * Gerçek donanımda çalışması hedeflenen ACPI shutdown/restart.
 *
 * Önceki sürüm sadece QEMU/Bochs gibi emülatörlerin ürettiği minimal/basit
 * DSDT'lerde işe yarayacak şekilde yazılmıştı:
 *   - RSDP checksum'ı hiç doğrulanmıyordu (yanlış-pozitif riski)
 *   - Sadece RSDT kullanılıyordu; ACPI 2.0+ donanımda (2010 sonrası hemen
 *     her makine) asıl güvenilir kaynak XSDT'dir, bazı UEFI sistemlerinde
 *     RSDT hiç yoktur
 *   - "_S5_" arayışı AML PkgLength encoding'ini hiç hesaba katmıyordu
 *     (PkgLength 1-4 byte olabilir, kod hep 1 byte sanıyordu) — gerçek
 *     üretici DSDT'lerinde (Dell/HP/Lenovo/ASUS vb.) bu, yanlış offset'ten
 *     çöp veri okuyup sistemi ya hiç kapatmamaya ya da kilitlenmeye
 *     götürebilirdi
 *   - Reset register sadece I/O adres tipini destekliyordu; bazı gerçek
 *     donanımlar memory-mapped reset register kullanır
 *
 * Bu sürüm hâlâ tam bir AML interpreter değil (onu yazmak binlerce satır
 * gerektirir) ama gerçek donanımın çok daha geniş bir kesiminde doğru
 * çalışacak şekilde: checksum doğrulanıyor, XSDT önceliği var, PkgLength
 * doğru decode ediliyor, hem I/O hem memory-mapped reset register
 * destekleniyor, ve her adımda "emin değilsem dur, güvenli fallback'e
 * düş" ilkesi izleniyor (yanlış SLP_TYP ile port yazıp sistemin
 * kilitlenmesindense, ACPI shutdown başarısız olup APM/PS2 fallback'ine
 * düşmesi tercih edilir).
 */
#include <stdint.h>

static void acpi_outb(uint16_t p, uint8_t  v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static void acpi_outw(uint16_t p, uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static void acpi_outl(uint16_t p, uint32_t v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static uint8_t  acpi_inb(uint16_t p){uint8_t  v;__asm__ volatile("inb  %1,%0":"=a"(v):"Nd"(p));return v;}

/* Basit bellek okuma yardımcıları */
static inline uint8_t  mem8 (uint64_t a){return *(volatile uint8_t* )(uintptr_t)a;}
static inline uint16_t mem16(uint64_t a){return *(volatile uint16_t*)(uintptr_t)a;}
static inline uint32_t mem32(uint64_t a){return *(volatile uint32_t*)(uintptr_t)a;}
static inline uint64_t mem64(uint64_t a){
    return (uint64_t)mem32(a) | ((uint64_t)mem32(a+4)<<32);
}

/* ── ACPI tablo imza kontrolü ── */
static int acpi_sig(uint64_t addr, const char* sig){
    for(int i=0;i<4;i++) if(mem8(addr+i)!=(uint8_t)sig[i]) return 0;
    return 1;
}

/* Herhangi bir ACPI tablosunun (RSDP dahil) checksum'ı: tüm byte'ların
 * toplamı 0xFF ile maskelendiğinde 0 olmalı. Bu kontrolü atlamak,
 * bellekte tesadüfen imzaya benzeyen bozuk/ilgisiz veriyi gerçek bir
 * ACPI tablosu sanıp işlemeye (ve donanıma yanlış komut göndermeye)
 * yol açabilir — gerçek donanımda bunun bedeli crash/hang olabilir. */
static int acpi_checksum_ok(uint64_t addr, uint32_t len){
    uint8_t sum=0;
    for(uint32_t i=0;i<len;i++) sum=(uint8_t)(sum+mem8(addr+i));
    return sum==0;
}

static uint64_t acpi_scan_range(uint64_t start,uint64_t end){
    for(uint64_t a=start;a<end;a+=16){
        if(mem8(a)=='R'&&mem8(a+1)=='S'&&mem8(a+2)=='D'&&
           mem8(a+3)==' '&&mem8(a+4)=='P'&&mem8(a+5)=='T'&&
           mem8(a+6)=='R'&&mem8(a+7)==' '){
            /* v1.0 checksum: ilk 20 byte */
            if(!acpi_checksum_ok(a,20)) continue;
            /* v2.0+: revision>=2 ise extended checksum da (34 byte) kontrol et */
            uint8_t rev=mem8(a+15);
            if(rev>=2 && !acpi_checksum_ok(a,mem32(a+20))) continue;
            return a;
        }
    }
    return 0;
}

/* ── RSDP Bul (checksum doğrulamalı) ── */
static uint64_t acpi_find_rsdp(){
    /* EBDA ilk KB (spesifikasyonda RSDP aranacak ilk yer) */
    uint64_t ebda=(uint64_t)mem16(0x40E)<<4;
    if(ebda>=0x80000&&ebda<0xA0000){
        uint64_t r=acpi_scan_range(ebda,ebda+1024);
        if(r) return r;
    }
    /* BIOS ROM alanı: 0xE0000 - 0xFFFFF */
    return acpi_scan_range(0xE0000,0x100000);
}

/* ACPI güç yönetimi global değişkenler */
static uint32_t g_pm1a_cnt = 0;
static uint32_t g_pm1b_cnt = 0;
static uint16_t g_slp_typa = 0;
static uint16_t g_slp_typb = 0;
static int      g_acpi_ok  = 0;
static uint64_t g_reset_reg_addr = 0;
static uint8_t  g_reset_val = 0;
static int      g_reset_via_reg = 0;   /* 0=yok, 1=I/O, 2=memory-mapped */

static int acpi_read_val(uint64_t& pos){
    uint8_t b = mem8(pos);
    if(b==0x0A){ pos++; uint8_t v=mem8(pos); pos++; return v; }
    if(b<=1){ pos++; return b; } /* ZeroOp/OneOp */
    return -1; /* tanınmayan encoding — güvenli çık */
}

/* DSDT/SSDT AML'inde "_S5_" paketini ara. PkgLength'i doğru decode eder
 * (1-4 byte, ilk byte'ın üst 2 biti kaç byte olduğunu belirler — ACPI
 * spec §20.2.4). Bulursa SLP_TYPa/b değerlerini yazar ve 1 döner. */
static int acpi_parse_s5(uint64_t table, uint32_t table_len){
    uint64_t end = table + table_len;
    for(uint64_t a = table + 36; a + 4 < end; a++){
        if(mem8(a)=='_'&&mem8(a+1)=='S'&&mem8(a+2)=='5'&&mem8(a+3)=='_'){
            uint64_t p = a + 4;
            if(mem8(p)==0x08) p++;              /* NameOp (0x08) */
            if(mem8(p)==0x5C) p++;               /* RootChar */
            while(mem8(p)=='^') p++;             /* ParentPrefix'ler */
            if(mem8(p)==0x12){                   /* PackageOp */
                p++;
                /* PkgLength decode (ACPI §20.2.4) */
                uint8_t lead = mem8(p);
                uint8_t nbytes = (uint8_t)(lead >> 6); /* 0-3 ek byte */
                p += 1 + nbytes;
                /* PkgLength sonrası: NumElements (1 byte) */
                p++;
                /* Sırayla SLP_TYPa, SLP_TYPb değerleri gelir. */
                int va = acpi_read_val(p);
                if(va<0) continue;
                int vb = acpi_read_val(p);
                if(vb<0) vb=va; /* bazı DSDT'lerde tek değer olur */
                g_slp_typa = (uint16_t)(va << 10);
                g_slp_typb = (uint16_t)(vb << 10);
                return 1;
            }
            /* PackageOp bulunamadıysa bu eşleşme muhtemelen yanlış
             * pozitiftir (başka bir yerde geçen "_S5_" string'i) —
             * aramaya devam et, ilk bulduğumuzda hemen dönme. */
        }
    }
    return 0;
}

/* ── ACPI Init: RSDP → (XSDT tercihli, yoksa RSDT) → FADT → PM1 + S5 ── */
static void acpi_init(){
    uint64_t rsdp = acpi_find_rsdp();
    if(!rsdp) return; /* ACPI yok/bulunamadı — shutdown() APM/hack fallback'e düşer */

    uint8_t rev = mem8(rsdp+15);
    uint64_t xsdt = (rev>=2) ? mem64(rsdp+24) : 0;
    uint64_t rsdt = mem32(rsdp+16);

    uint64_t table=0; uint32_t entry_sz=4; uint32_t table_len=0;
    if(xsdt && acpi_sig(xsdt,"XSDT") && acpi_checksum_ok(xsdt,mem32(xsdt+4))){
        table=xsdt; entry_sz=8; table_len=mem32(xsdt+4);
    } else if(rsdt && acpi_sig(rsdt,"RSDT") && acpi_checksum_ok(rsdt,mem32(rsdt+4))){
        table=rsdt; entry_sz=4; table_len=mem32(rsdt+4);
    } else {
        return; /* ne XSDT ne RSDT doğrulanabildi — güvenli çık */
    }
    if(table_len < 36) return;
    uint32_t n_entries = (table_len - 36) / entry_sz;

    /* FADT'yi ara */
    uint64_t fadt = 0;
    for(uint32_t i = 0; i < n_entries && i < 64; i++){
        uint64_t tbl = (entry_sz==8) ? mem64(table + 36 + i*8)
                                     : (uint64_t)mem32(table + 36 + i*4);
        if(!tbl) continue;
        if(acpi_sig(tbl,"FACP") && acpi_checksum_ok(tbl,mem32(tbl+4))){ fadt=tbl; break; }
    }
    if(!fadt) return;

    /* FADT'den PM1a/PM1b control block adresi (offset 64, 68 — 32-bit,
     * her ACPI sürümünde mevcut, X_PM1a_CNT_BLK varsa onu tercih ederiz
     * ama 32-bit alan pratikte hemen her donanımda doğru ve yeterlidir) */
    g_pm1a_cnt = mem32(fadt + 64);
    g_pm1b_cnt = mem32(fadt + 68);

    /* Reset register (FADT revision >= 2, offset 116: RESET_REG GAS) */
    uint8_t fadt_rev = mem8(fadt + 8);
    if(fadt_rev >= 2 && table_len > 128){
        uint8_t gas_type = mem8(fadt + 116);  /* 0=memory, 1=I/O, 2=PCI config */
        uint64_t gas_addr = mem64(fadt + 120);
        if(gas_addr){
            if(gas_type==1){ g_reset_reg_addr=gas_addr; g_reset_via_reg=1; }
            else if(gas_type==0){ g_reset_reg_addr=gas_addr; g_reset_via_reg=2; }
            /* gas_type==2 (PCI config): desteklenmiyor, PS/2 fallback kullanılacak */
            if(g_reset_via_reg) g_reset_val = mem8(fadt + 128);
        }
    }

    /* DSDT adresi: X_DSDT (offset 140, 64-bit, rev>=2) varsa onu, yoksa
     * klasik DSDT (offset 40, 32-bit) kullan. */
    uint64_t dsdt = 0;
    if(fadt_rev >= 2 && table_len > 148) dsdt = mem64(fadt + 140);
    if(!dsdt) dsdt = mem32(fadt + 40);
    if(!dsdt || !acpi_sig(dsdt,"DSDT")) return;
    uint32_t dsdt_len = mem32(dsdt + 4);
    if(!acpi_checksum_ok(dsdt,dsdt_len)) return;

    if(acpi_parse_s5(dsdt, dsdt_len)) g_acpi_ok = 1;
    /* S5 bulunamazsa g_acpi_ok=0 kalır: shutdown() APM/hack fallback'e
     * düşer — bulamadığımız bir SLP_TYP ile port yazıp sistemin
     * kilitlenmesi riskini almaktansa güvenli tarafta kalmak tercih
     * edilir. PM1 control adresleri (g_pm1a_cnt vb.) yine de restart
     * gibi ACPI'siz yollarla kullanılabilir kalır. */
}

/* ── ACPI Shutdown ── */
static void acpi_shutdown(){
    if(!g_acpi_ok || !g_pm1a_cnt){
        /* ACPI bulunamadı/S5 çözülemedi: APM/emülatör fallback.
         * Gerçek donanımda bu portlara yazmak zararsızdır (kullanılmayan
         * I/O adreslerine yazma etkisizdir), sadece emülatörlerde işe
         * yarar; bu yüzden ACPI'nin önüne değil sonrasına konuyor. */
        acpi_outw(0x604, 0x2000); /* QEMU */
        acpi_outw(0xB004,0x2000); /* Bochs */
        acpi_outw(0x4004,0x3400); /* VirtualBox eski */
        return;
    }
    /* PM1a/b SLP_EN + SLP_TYPa/b — gerçek ACPI shutdown yolu */
    acpi_outw((uint16_t)g_pm1a_cnt, (uint16_t)(g_slp_typa | 0x2000));
    if(g_pm1b_cnt)
        acpi_outw((uint16_t)g_pm1b_cnt, (uint16_t)(g_slp_typb | 0x2000));
}

/* ── ACPI / PS2 Restart ── */
static void acpi_restart(){
    /* 1. ACPI reset register (FADT v2+) — hem I/O hem memory-mapped */
    if(g_reset_via_reg==1 && g_reset_reg_addr){
        acpi_outb((uint16_t)g_reset_reg_addr, g_reset_val);
    } else if(g_reset_via_reg==2 && g_reset_reg_addr){
        *(volatile uint8_t*)(uintptr_t)g_reset_reg_addr = g_reset_val;
    }
    /* 2. PS/2 kontroller reset (8042) — hemen her x86 sistemde çalışan
     * evrensel fallback (gerçek donanımda ACPI reset register'ı yoksa
     * ya da tepki vermezse buraya düşülür) */
    for(int i=0;i<0xFFFF;i++){
        if(!(acpi_inb(0x64)&0x02)) break;
    }
    acpi_outb(0x64, 0xFE);  /* CPU reset komutu */

    /* 3. Triple fault ile zorla reset (son çare) */
    uint8_t null_idtr[6] = {0,0,0,0,0,0};
    __asm__ volatile(
        "cli\n"
        "lidt (%0)\n"
        "int $3\n"
        :: "r"(null_idtr)
    );
}

/* Global init flag */
static int g_acpi_inited = 0;
static void acpi_ensure_init(){
    if(!g_acpi_inited){ acpi_init(); g_acpi_inited=1; }
}

#endif
