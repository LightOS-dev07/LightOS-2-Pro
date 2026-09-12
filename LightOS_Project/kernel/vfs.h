#ifndef VFS_H
#define VFS_H
/*
 * kernel/vfs.h — LightOS Dosya Sistemi Arayüzü (CandleFS üzerinde)
 * ============================================================================
 * Bu dosyanın dışarıya sunduğu API (VfsNode, VFS, FindChild, MkFile,
 * WriteFile, GetData, nodes[]/node_count alanları) ESKİ "RAM'de düz dizi"
 * VFS ile birebir aynı — shell.cpp, filemgr, notepad, terminal ve
 * diğer ~8 dosya hiç değişmeden çalışmaya devam ediyor.
 *
 * Perde arkasında değişen şey: artık gerçek bir dosya sistemi mimarisi
 * (bkz. kernel/candlefs.h) kullanılıyor — inode tablosu, doğrudan blok
 * işaretçileri, free-space bitmap. RAM'deki nodes[] hâlâ var (hız için,
 * disk her okumada tekrar okunmuyor) ama artık bu bir "cache" — gerçek
 * kaynak diskteki CandleFS yapılarıdır ve candlefs_persist.h bu ikisini
 * birbirine senkronize eder.
 *
 * Bu ayrım şunu sağlıyor: dosyayı KULLANAN kod (Notepad, FileMgr, ...)
 * dosya sisteminin nasıl saklandığından habersiz kalıyor — tıpkı gerçek
 * işletim sistemlerinde uygulamaların ext4 mi FAT32 mi olduğunu bilmesine
 * gerek olmaması gibi.
 */

#include <stdint.h>
#include "candlefs.h"

/* Geriye dönük isimler — eski koddaki VFS_FILE/VFS_DIR/VfsNodeType hâlâ
 * çalışsın diye CandleFS enum'una eşleniyor. */
typedef CfsNodeType VfsNodeType;
#define VFS_FILE CFS_FILE
#define VFS_DIR  CFS_DIR

static const int VFS_NAME_LEN     = CFS_NAME_LEN;
static const int VFS_MAX_NODES    = CFS_MAX_INODES;
static const int VFS_MAX_DATA     = CFS_MAX_FILE_SIZE;   /* artık blok tabanlı, ama eski isim korunuyor */
static const int VFS_MAX_CHILDREN = CFS_MAX_CHILDREN;

/* Eski kod nodes[i].name / .type / .size / .parent / .children / .used
 * alanlarına doğrudan erişiyordu — VfsNode'u CfsInode'un üstüne ince bir
 * takma ad (alias) olarak tanımlıyoruz, alan isimleri birebir aynı. */
struct VfsNode {
    char        name[CFS_NAME_LEN];
    VfsNodeType type;
    uint32_t    size;
    uint32_t    data_offset;   /* artık kullanılmıyor (blok tabanlı depolama),
                                   geriye dönük uyum için duruyor, hep 0 */
    int         parent;
    int         children[CFS_MAX_CHILDREN];
    int         child_count;
    bool        used;

    void Init(const char* n, VfsNodeType t, int par) {
        int i=0; while(n[i]&&i<CFS_NAME_LEN-1){name[i]=n[i];i++;} name[i]=0;
        type=t; size=0; data_offset=0; parent=par;
        child_count=0; used=true;
        for(int j=0;j<CFS_MAX_CHILDREN;j++) children[j]=-1;
    }
};

/* ── VFS: CandleFS motorunu saran, eski API'yi koruyan katman ── */
struct VFS {
    VfsNode  nodes[CFS_MAX_INODES];   /* RAM cache — eski koddaki gibi doğrudan erişilebilir */
    int      node_count;
    uint32_t data_used;               /* artık gerçek anlamı yok (geriye dönük uyum) */

    /* CandleFS'in gerçek durumu: hangi bloklar dolu/boş, ve her inode'un
     * blok listesi. Bunlar nodes[]'un "arkasında" tutuluyor, GetData()
     * gibi fonksiyonlar bunları kullanarak veriyi RAM'de birleştirip
     * eski davranışı (tek bir düz const char* dönmesi) taklit ediyor. */
    uint32_t block_bitmap[CFS_TOTAL_BLOCKS/32]; /* 1 bit/blok, 32 blok/uint32_t */
    uint32_t inode_blocks[CFS_MAX_INODES][CFS_DIRECT_BLOCKS];
    uint32_t inode_block_count[CFS_MAX_INODES];
    /* Dosya içeriği okuma için tek-dosyalık geçici birleştirme tamponu.
     * GetData() eski kodda "data_pool + offset" şeklinde kalıcı bir
     * pointer döndürüyordu; CandleFS'te veri artık ayrık bloklarda
     * olduğu için okurken bu tampona birleştirip pointer'ını döndürüyoruz.
     * Tek seferde bir dosya okunduğu sürece (mevcut kullanım deseni budur)
     * bu güvenlidir. */
    uint8_t  read_buf[CFS_MAX_FILE_SIZE+1];

    /* ── Bitmap yardımcıları ── */
    bool BlockUsed(uint32_t b){ return (block_bitmap[b/32] >> (b%32)) & 1u; }
    void SetBlockUsed(uint32_t b, bool used_){
        if(used_) block_bitmap[b/32] |= (1u << (b%32));
        else      block_bitmap[b/32] &= ~(1u << (b%32));
    }
    int AllocBlock(){
        for(uint32_t b=0;b<(uint32_t)CFS_TOTAL_BLOCKS;b++)
            if(!BlockUsed(b)){ SetBlockUsed(b,true); return (int)b; }
        return -1; /* disk dolu */
    }
    void FreeBlock(uint32_t b){ if(b<(uint32_t)CFS_TOTAL_BLOCKS) SetBlockUsed(b,false); }

    /* Bir inode'un tüm bloklarını serbest bırakır (dosya silinirken ya
     * da içerik değiştirilip yeniden yazılırken eskisi boşa çıkarılır).
     * Eski sistemde bu YOKTU — data_used hiç azalmıyor, disk dolana
     * kadar "sızdırıyordu". */
    void FreeInodeBlocks(int idx){
        for(uint32_t i=0;i<inode_block_count[idx];i++){
            FreeBlock(inode_blocks[idx][i]);
            inode_blocks[idx][i]=CFS_BLOCK_UNUSED;
        }
        inode_block_count[idx]=0;
    }

    void Init() {
        node_count = 0; data_used = 0;
        for(int i=0;i<CFS_MAX_INODES;i++){
            nodes[i].used=false;
            inode_block_count[i]=0;
            for(int j=0;j<CFS_DIRECT_BLOCKS;j++) inode_blocks[i][j]=CFS_BLOCK_UNUSED;
        }
        for(int i=0;i<CFS_TOTAL_BLOCKS/32;i++) block_bitmap[i]=0;

        /* Kök dizin: inode 0, isim "#". */
        nodes[0].Init("#", CFS_DIR, -1);
        node_count = 1;

        int sys  = MkDir(0, "System");
        int usr  = MkDir(0, "Users");
        int apps = MkDir(0, "Apps");
        int temp = MkDir(0, "Temp");
        int docs = MkDir(usr, "Default");
        MkFile(sys,  "boot.cfg",    "# LightOS Boot Config\nresolution=800x600\ntheme=luna\n");
        MkFile(sys,  "kernel.bin",  "[binary]");
        MkFile(docs, "readme.txt",  "Welcome to LightOS 2 Pro!\nThis is your home directory.\n");
        MkFile(docs, "notes.txt",   "");
        MkFile(apps, "notepad.app", "[app]");
        MkFile(apps, "calc.app",    "[app]");
        MkFile(temp, "tmp0.tmp",    "");
        (void)temp;
    }

    int AllocNode() {
        for(int i=0;i<CFS_MAX_INODES;i++) if(!nodes[i].used) return i;
        return -1;
    }

    int MkDir(int parent, const char* name) {
        int idx = AllocNode(); if(idx<0) return -1;
        nodes[idx].Init(name, CFS_DIR, parent);
        if(parent>=0 && nodes[parent].child_count < CFS_MAX_CHILDREN)
            nodes[parent].children[nodes[parent].child_count++] = idx;
        if(idx >= node_count) node_count = idx+1;
        return idx;
    }

    /* İçeriği verilen inode'a gerçek bloklara bölerek yazar. Önce eski
     * blokları serbest bırakır (varsa), sonra ihtiyaç kadar yeni blok
     * ayırıp içeriği bunlara dağıtır. Disk doluysa (AllocBlock -1
     * dönerse) o ana kadar ayrılan bloklar geri bırakılır ve false
     * döner — yarım yamalak/tutarsız bir dosya bırakılmaz. */
    bool WriteInodeData(int idx, const char* content, int len){
        FreeInodeBlocks(idx);
        int need = (len + CFS_BLOCK_SIZE - 1) / CFS_BLOCK_SIZE;
        if(need==0) need=1; /* boş dosya bile 1 blok tutar, basitlik için */
        if(need > CFS_DIRECT_BLOCKS) return false; /* CFS_MAX_FILE_SIZE aşıldı */

        uint32_t allocated[CFS_DIRECT_BLOCKS];
        for(int i=0;i<need;i++){
            int b=AllocBlock();
            if(b<0){
                /* Disk dolu: bu ana kadar ayrılanları geri ver */
                for(int j=0;j<i;j++) FreeBlock(allocated[j]);
                return false;
            }
            allocated[i]=(uint32_t)b;
        }
        for(int i=0;i<need;i++){
            inode_blocks[idx][i]=allocated[i];
            int off=i*CFS_BLOCK_SIZE;
            int chunk = len-off; if(chunk>CFS_BLOCK_SIZE) chunk=CFS_BLOCK_SIZE;
            if(chunk>0) WriteBlockContent(allocated[i], content+off, chunk);
        }
        inode_block_count[idx]=(uint32_t)need;
        nodes[idx].size=(uint32_t)len;
        return true;
    }

    /* Blokların gerçek veri baytlarını tutan RAM alanı. Disk kalıcılığı
     * (candlefs_persist.h) bunu diskteki CFS_DATA_LBA bölgesiyle
     * senkronize eder — burada RAM içi hızlı erişim için ayrı tutuluyor. */
    uint8_t block_storage[CFS_TOTAL_BLOCKS][CFS_BLOCK_SIZE];

    void WriteBlockContent(uint32_t block_num, const char* src, int len){
        for(int i=0;i<len;i++) block_storage[block_num][i]=(uint8_t)src[i];
    }

    int MkFile(int parent, const char* name, const char* content) {
        int idx = AllocNode(); if(idx<0) return -1;
        nodes[idx].Init(name, CFS_FILE, parent);
        int len=0; while(content[len]) len++;
        if(!WriteInodeData(idx, content, len)){
            nodes[idx].used=false; /* rollback: disk dolu vb. */
            return -1;
        }
        if(parent>=0 && nodes[parent].child_count < CFS_MAX_CHILDREN)
            nodes[parent].children[nodes[parent].child_count++] = idx;
        if(idx >= node_count) node_count = idx+1;
        return idx;
    }

    /* Bir dosyanın içeriğini yaz (notepad'den) — eskisi gibi tüm içeriği
     * değiştirir, ama artık eski bloklar GERÇEKTEN serbest bırakılıyor. */
    bool WriteFile(int idx, const char* content) {
        if(idx<0||!nodes[idx].used||nodes[idx].type!=CFS_FILE) return false;
        int len=0; while(content[len]) len++;
        if(len > CFS_MAX_FILE_SIZE) len = CFS_MAX_FILE_SIZE;
        return WriteInodeData(idx, content, len);
    }

    /* Ham byte dizisi yaz — içeride \0 byte'ları olabilecek ikili veri
     * (BMP ekran görüntüsü, vb.) için. WriteFile() strlen() kullandığından
     * ilk \0'da keserdi; bu fonksiyon tam olarak verilen uzunluğu yazar. */
    bool WriteFileBytes(int idx, const uint8_t* data, int len) {
        if(idx<0||!nodes[idx].used||nodes[idx].type!=CFS_FILE) return false;
        if(len > CFS_MAX_FILE_SIZE) len = CFS_MAX_FILE_SIZE;
        return WriteInodeData(idx, (const char*)data, len);
    }

    /* Dosyanın içindeki TEK bir byte'ı, tüm içeriği yeniden yazmadan
     * günceller (Hex Editor'ün tek-byte düzenleme özelliği için). Doğru
     * bloğu (offset/CFS_BLOCK_SIZE) ve o bloktaki doğru pozisyonu
     * (offset%CFS_BLOCK_SIZE) bulup doğrudan oraya yazar. */
    bool WriteByteAt(int idx, int offset, uint8_t value){
        if(idx<0||!nodes[idx].used||nodes[idx].type!=CFS_FILE) return false;
        if(offset<0||offset>=(int)nodes[idx].size) return false;
        uint32_t blk_i = (uint32_t)offset / CFS_BLOCK_SIZE;
        uint32_t blk_off = (uint32_t)offset % CFS_BLOCK_SIZE;
        if(blk_i >= inode_block_count[idx]) return false;
        uint32_t blk = inode_blocks[idx][blk_i];
        block_storage[blk][blk_off] = value;
        return true;
    }

    int CreateFile(int parent, const char* name) {
        return MkFile(parent, name, "");
    }

    /* İsme göre child bul */
    int FindChild(int parent, const char* name) {
        if(parent<0) return -1;
        VfsNode& p = nodes[parent];
        for(int i=0;i<p.child_count;i++){
            int ci = p.children[i];
            if(ci<0) continue;
            const char* a=nodes[ci].name; const char* b=name;
            int j=0; while(a[j]&&b[j]&&a[j]==b[j]) j++;
            if(a[j]==b[j]) return ci;
        }
        return -1;
    }

    /* Dosyanın içeriğini döndürür. Eski API bir "const char*" bekliyordu
     * (tek, sürekli bellek bloğu) — CandleFS'te veri ayrık bloklarda
     * olduğu için burada read_buf'a birleştirip null-terminate ediyoruz.
     * NOT: dönen pointer bir sonraki GetData() çağrısına kadar geçerlidir
     * (aynı eski sistemde olduğu gibi tek seferlik/anlık okuma deseni). */
    const char* GetData(int idx) {
        if(idx<0||!nodes[idx].used){ read_buf[0]=0; return (const char*)read_buf; }
        uint32_t sz = nodes[idx].size;
        if(sz > (uint32_t)CFS_MAX_FILE_SIZE) sz = CFS_MAX_FILE_SIZE;
        uint32_t copied=0;
        for(uint32_t i=0;i<inode_block_count[idx] && copied<sz;i++){
            uint32_t blk=inode_blocks[idx][i];
            int chunk = (int)(sz-copied); if(chunk>CFS_BLOCK_SIZE) chunk=CFS_BLOCK_SIZE;
            for(int j=0;j<chunk;j++) read_buf[copied+j]=block_storage[blk][j];
            copied+=(uint32_t)chunk;
        }
        read_buf[copied]=0;
        return (const char*)read_buf;
    }
};

/* Global VFS örneği — shell.cpp'de tanımlanır, extern ile erişilir */
extern VFS g_vfs;

#endif
