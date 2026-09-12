#ifndef CANDLEFS_H
#define CANDLEFS_H
/*
 * kernel/candlefs.h — CandleFS: LightOS'un gerçek disk dosya sistemi
 * ============================================================================
 * LightOS'un "hobby OS"den gerçek bir işletim sistemine geçişindeki ilk
 * adım. Öncesinde "VFS" adı verilen yapı aslında bir dosya sistemi değildi:
 * disk üzerine RAM'deki bir C struct dizisinin ham baytlarını döküyordu,
 * hiç free-space takibi yoktu (silinen dosyanın yeri asla geri kazanılmaz,
 * disk_used sonsuza dek artardı), ve "dosya verisi" tek bir sürekli offset
 * ile adresleniyordu (gerçek dosya sistemlerindeki blok tabanlı depolamanın
 * hiçbir faydası yoktu — parçalanma, kısmi güncelleme, büyüyüp küçülen
 * dosyalar desteklenmiyordu).
 *
 * CandleFS gerçek bir dosya sistemi mimarisi kullanır:
 *
 *   ┌──────────────┬──────────────┬──────────────────┬─────────────────┐
 *   │  Superblock  │  Free-space  │   Inode Table     │   Data Blocks   │
 *   │   (1 sektör) │   bitmap     │  (128 inode)      │  (4KB'lik       │
 *   │              │  (1 sektör)  │                    │   bloklar)      │
 *   └──────────────┴──────────────┴──────────────────┴─────────────────┘
 *      LBA CFS_SB      LBA CFS_BMP    LBA CFS_INODE_LBA    LBA CFS_DATA_LBA+
 *
 * - Superblock: format sürümü, toplam blok sayısı, diğer bölgelerin konumu.
 *   Boot'ta ilk okunan şey; magic uyuşmazsa CandleFS'in hiç kurulmadığı
 *   anlaşılır ve format işlemi tetiklenir.
 * - Free-space bitmap: her bit bir 4KB veri bloğunun dolu/boş durumunu
 *   tutar. Dosya silindiğinde blokları GERÇEKTEN serbest bırakır — eski
 *   sistemde bu imkansızdı.
 * - Inode tablosu: dosya/dizin meta verisi + DOĞRUDAN BLOK İŞARETÇİLERİ
 *   (direct pointers). Bir dosya en fazla CFS_DIRECT_BLOCKS×4KB büyüklüğünde
 *   olabilir (basitlik için tek seviyeli — indirect block yok, ama gerçek
 *   blok tabanlı depolama var, bu da parçalı/ayrı bloklarda dosya tutmayı
 *   ve blokları bağımsız serbest bırakmayı mümkün kılıyor).
 * - Data blokları: gerçek dosya içeriği, 4KB'lik sabit boyutlu bloklar
 *   halinde, bitmap ile takip edilen boş bloklara YERLEŞTİRİLİR (sabit
 *   "slot" ya da hep-ileri-giden sayaç değil, gerçek allocate/free).
 *
 * Üst seviye API (FindChild, MkFile, WriteFile, GetData, nodes[] erişimi)
 * eski VFS ile AYNI kalıyor — bu, dosyayı kullanan 12 dosyaya (shell,
 * filemgr, notepad, terminal, vs.) hiç dokunmadan, sadece alttaki disk
 * formatını gerçek bir dosya sistemiyle değiştirmemizi sağlıyor.
 */
#include <stdint.h>

static const int  CFS_NAME_LEN      = 32;
static const int  CFS_MAX_INODES    = 128;
static const int  CFS_MAX_CHILDREN  = 16;
static const int  CFS_BLOCK_SIZE    = 4096;           /* 1 veri bloğu */
static const int  CFS_BLOCK_SECTORS = CFS_BLOCK_SIZE/512;
static const int  CFS_DIRECT_BLOCKS = 12;              /* inode başına doğrudan blok */
static const int  CFS_MAX_FILE_SIZE = CFS_DIRECT_BLOCKS*CFS_BLOCK_SIZE; /* 48KB/dosya */
static const int  CFS_TOTAL_BLOCKS  = 512;             /* toplam veri bloğu (2MB alan) */

/* Disk üzerindeki bölge adresleri (LBA, ATA sektör numarası) */
static const uint32_t CFS_SB_LBA     = 1;    /* Superblock */
static const uint32_t CFS_BITMAP_LBA = 2;    /* Free-space bitmap (1 sektör = 4096 bit = 4096 blok'a yeter) */
static const uint32_t CFS_INODE_LBA  = 3;    /* Inode tablosu başlangıcı */
static const int      CFS_INODE_SECTS_EACH = 2; /* inode başına 2 sektör (1024 byte) yeterli */
static const uint32_t CFS_DATA_LBA   = CFS_INODE_LBA + CFS_MAX_INODES*CFS_INODE_SECTS_EACH; /* 3+256=259 */

#define CFS_MAGIC 0x53464843u /* "CHFS" little-endian: CandleFS imzası */
#define CFS_VERSION 1u

enum CfsNodeType { CFS_FILE = 0, CFS_DIR = 1 };

/* ── Superblock: diskteki ilk sektör, formatın "kimlik kartı" ── */
struct CfsSuperblock {
    uint32_t magic;          /* CFS_MAGIC — yoksa disk hiç formatlanmamış */
    uint32_t version;
    uint32_t total_blocks;   /* CFS_TOTAL_BLOCKS */
    uint32_t block_size;     /* CFS_BLOCK_SIZE */
    uint32_t inode_count;    /* CFS_MAX_INODES */
    uint32_t root_inode;     /* her zaman 0 */
    uint32_t free_blocks;    /* şu an boş blok sayısı (bilgi amaçlı) */
};

/* ── Inode: bir dosya ya da dizinin tüm meta verisi ── */
struct CfsInode {
    char     name[CFS_NAME_LEN];
    uint8_t  type;                        /* CfsNodeType */
    uint8_t  used;
    uint32_t size;                        /* dosya: byte cinsinden gerçek boyut */
    int32_t  parent;                      /* -1 = kök */
    int32_t  children[CFS_MAX_CHILDREN];  /* sadece dizinler için */
    int32_t  child_count;
    uint32_t blocks[CFS_DIRECT_BLOCKS];   /* veri blok NUMARALARI (0xFFFFFFFF = kullanılmıyor) */
    uint32_t block_count;                 /* kaç blok kullanılıyor */
};

#define CFS_BLOCK_UNUSED 0xFFFFFFFFu

#endif
