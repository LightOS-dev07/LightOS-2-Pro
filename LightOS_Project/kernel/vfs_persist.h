#ifndef VFS_PERSIST_H
#define VFS_PERSIST_H
/*
 * kernel/vfs_persist.h — CandleFS Disk Kalıcılığı
 * ============================================================================
 * Eski isim (vfs_persist.h, vfs_save/vfs_load) korunuyor çünkü shell.cpp,
 * notepad.cpp, luigi.h gibi dosyalar bu isimleri çağırıyor — sadece içeriği
 * artık gerçek CandleFS formatını (bkz. candlefs.h) okuyup yazıyor.
 *
 * Disk üzerinde format:
 *   LBA 1            : Superblock (magic, versiyon, blok sayıları)
 *   LBA 2            : Free-space bitmap (512 blok / 32 bit = 16 uint32,
 *                       1 sektörde fazlasıyla yer var)
 *   LBA 3..258       : Inode tablosu (128 inode × 2 sektör = 256 sektör)
 *   LBA 259..259+... : Data blokları (512 blok × 8 sektör/blok = 4096 sektör)
 *
 * Bu, eski "vfs_persist.h"nin yaptığı gibi RAM'deki bir struct'ı ham
 * baytlarla diske dökmek değil — gerçek bir dosya sisteminin parçalarını
 * (superblock/bitmap/inode/data) ayrı ayrı, kendi anlamlarına uygun
 * bölgelere yazan bir format.
 */
#include "vfs.h"
#include "candlefs.h"
#include "../drivers/ata.h"

/* ── Superblock yaz/oku ── */
static bool cfs_write_superblock(){
    uint8_t sec[512];
    for(int i=0;i<512;i++) sec[i]=0;
    CfsSuperblock sb;
    sb.magic=CFS_MAGIC; sb.version=CFS_VERSION;
    sb.total_blocks=CFS_TOTAL_BLOCKS; sb.block_size=CFS_BLOCK_SIZE;
    sb.inode_count=CFS_MAX_INODES; sb.root_inode=0;
    uint32_t free_blocks=0;
    for(uint32_t b=0;b<(uint32_t)CFS_TOTAL_BLOCKS;b++)
        if(!g_vfs.BlockUsed(b)) free_blocks++;
    sb.free_blocks=free_blocks;
    for(uint32_t i=0;i<sizeof(CfsSuperblock);i++) sec[i]=((uint8_t*)&sb)[i];
    return g_disk.write_sector(CFS_SB_LBA, sec);
}

static bool cfs_probe_superblock(CfsSuperblock& out){
    uint8_t sec[512];
    if(!g_disk.read_sector(CFS_SB_LBA, sec)) return false;
    for(uint32_t i=0;i<sizeof(CfsSuperblock);i++) ((uint8_t*)&out)[i]=sec[i];
    return out.magic==CFS_MAGIC;
}

/* ── Bitmap yaz/oku (512 blok = 16 × uint32_t = 64 byte, 1 sektöre kolayca sığar) ── */
static bool cfs_write_bitmap(){
    uint8_t sec[512];
    for(int i=0;i<512;i++) sec[i]=0;
    for(int i=0;i<CFS_TOTAL_BLOCKS/32;i++)
        *(uint32_t*)(sec+i*4)=g_vfs.block_bitmap[i];
    return g_disk.write_sector(CFS_BITMAP_LBA, sec);
}
static bool cfs_read_bitmap(){
    uint8_t sec[512];
    if(!g_disk.read_sector(CFS_BITMAP_LBA, sec)) return false;
    for(int i=0;i<CFS_TOTAL_BLOCKS/32;i++)
        g_vfs.block_bitmap[i]=*(uint32_t*)(sec+i*4);
    return true;
}

/* ── Tek bir inode'u sektörlere serialize et ── */
static void cfs_serialize_inode(int idx, uint8_t* buf /* CFS_INODE_SECTS_EACH*512 byte */){
    for(int i=0;i<CFS_INODE_SECTS_EACH*512;i++) buf[i]=0;
    VfsNode& nd=g_vfs.nodes[idx];
    int p=0;
    for(int i=0;i<CFS_NAME_LEN;i++) buf[p++]=(uint8_t)nd.name[i];
    buf[p++]=(uint8_t)nd.type;
    buf[p++]=(uint8_t)nd.used;
    *(int32_t*)(buf+p)=nd.parent; p+=4;
    *(int32_t*)(buf+p)=nd.child_count; p+=4;
    for(int i=0;i<CFS_MAX_CHILDREN;i++){ *(int32_t*)(buf+p)=nd.children[i]; p+=4; }
    *(uint32_t*)(buf+p)=nd.size; p+=4;
    *(uint32_t*)(buf+p)=g_vfs.inode_block_count[idx]; p+=4;
    for(int i=0;i<CFS_DIRECT_BLOCKS;i++){ *(uint32_t*)(buf+p)=g_vfs.inode_blocks[idx][i]; p+=4; }
}
static void cfs_deserialize_inode(int idx, const uint8_t* buf){
    VfsNode& nd=g_vfs.nodes[idx];
    int p=0;
    for(int i=0;i<CFS_NAME_LEN;i++) nd.name[i]=(char)buf[p++];
    nd.name[CFS_NAME_LEN-1]=0;
    nd.type=(VfsNodeType)buf[p++];
    nd.used=(bool)buf[p++];
    nd.parent=*(int32_t*)(buf+p); p+=4;
    nd.child_count=*(int32_t*)(buf+p); p+=4;
    for(int i=0;i<CFS_MAX_CHILDREN;i++){ nd.children[i]=*(int32_t*)(buf+p); p+=4; }
    nd.size=*(uint32_t*)(buf+p); p+=4;
    g_vfs.inode_block_count[idx]=*(uint32_t*)(buf+p); p+=4;
    for(int i=0;i<CFS_DIRECT_BLOCKS;i++){ g_vfs.inode_blocks[idx][i]=*(uint32_t*)(buf+p); p+=4; }
}

/* ── CandleFS'i diske yaz (superblock + bitmap + inode tablosu + data blokları) ── */
static bool vfs_save(){
    if(!g_disk.present) return false;
    if(!cfs_write_superblock()) return false;
    if(!cfs_write_bitmap()) return false;

    uint8_t ibuf[CFS_INODE_SECTS_EACH*512];
    for(int n=0;n<CFS_MAX_INODES;n++){
        cfs_serialize_inode(n, ibuf);
        uint32_t lba=CFS_INODE_LBA+(uint32_t)n*CFS_INODE_SECTS_EACH;
        if(!g_disk.write(lba, CFS_INODE_SECTS_EACH, ibuf)) return false;
    }

    /* Sadece KULLANILAN (bitmap'te dolu işaretli) blokları yaz — boş
     * bloklara yazmak gereksiz disk I/O olurdu. */
    for(uint32_t b=0;b<(uint32_t)CFS_TOTAL_BLOCKS;b++){
        if(!g_vfs.BlockUsed(b)) continue;
        uint32_t lba=CFS_DATA_LBA+b*CFS_BLOCK_SECTORS;
        if(!g_disk.write(lba, CFS_BLOCK_SECTORS, g_vfs.block_storage[b])) return false;
    }
    return true;
}

/* ── Diskten CandleFS yükle ── */
static bool vfs_load(){
    if(!g_disk.present) return false;

    CfsSuperblock sb;
    if(!cfs_probe_superblock(sb)) return false; /* disk hiç formatlanmamış */
    if(sb.version!=CFS_VERSION) return false;   /* bilinmeyen/uyumsuz sürüm */

    if(!cfs_read_bitmap()) return false;

    uint8_t ibuf[CFS_INODE_SECTS_EACH*512];
    g_vfs.node_count=0;
    for(int n=0;n<CFS_MAX_INODES;n++){
        uint32_t lba=CFS_INODE_LBA+(uint32_t)n*CFS_INODE_SECTS_EACH;
        if(!g_disk.read(lba, CFS_INODE_SECTS_EACH, ibuf)){ g_vfs.nodes[n].used=false; continue; }
        cfs_deserialize_inode(n, ibuf);
        if(g_vfs.nodes[n].used && n>=g_vfs.node_count) g_vfs.node_count=n+1;
    }

    /* Kullanılan her blok için gerçek veriyi diskten oku */
    for(uint32_t b=0;b<(uint32_t)CFS_TOTAL_BLOCKS;b++){
        if(!g_vfs.BlockUsed(b)) continue;
        uint32_t lba=CFS_DATA_LBA+b*CFS_BLOCK_SECTORS;
        if(!g_disk.read(lba, CFS_BLOCK_SECTORS, g_vfs.block_storage[b])) return false;
    }
    return true;
}
#endif
