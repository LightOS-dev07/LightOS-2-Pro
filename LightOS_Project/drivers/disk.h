#include <stdint.h>

// 16 Byte'lık Bölüm (Partition) Kaydı
struct PartitionEntry {
    uint8_t boot_indicator;  // 0x80 ise boot edilebilir
    uint8_t starting_chs[3]; // Eski CHS adreslemesi (Artık kullanmıyoruz)
    uint8_t partition_id;    // 0x0B veya 0x0C ise bu FAT32'dir!
    uint8_t ending_chs[3];   // Eski CHS adreslemesi
    uint32_t start_lba;      // İŞTE HAZİNE BURADA! Bölümün başladığı gerçek sektör.
    uint32_t total_sectors;  // Bölümün boyutu
} __attribute__((packed)); // Derleyicinin aralara boşluk koymasını engeller

// 512 Byte'lık Ana MBR Yapısı
struct MBR {
    uint8_t boot_code[446];         // İlk 446 byte işletim sistemi bootloader kodudur
    PartitionEntry partitions[4];   // Diskte en fazla 4 ana bölüm (partition) olabilir
    uint16_t boot_signature;        // Her zaman 0xAA55 olmalıdır. Diskin geçerli olduğunu kanıtlar.
} __attribute__((packed));

uint32_t fat32_start_sector = 0; // FAT32'nin başladığı yeri burada saklayacağız

void FindFAT32Partition() {
    uint8_t buffer[512];
    
    // Diskin 0. Sektörünü (MBR) oku ve buffer'a at
    ReadSector(0, buffer); 
    
    // Buffer'ı okuması kolay olsun diye MBR yapısına çevir
    MBR* mbr = (MBR*)buffer;
    
    // Diskin sonunda 0xAA55 imzası yoksa, bu disk bozuktur veya boş formatlanmamıştır
    if (mbr->boot_signature != 0xAA55) {
        // İleride ekrana "Hata: Gecerli bir disk bulunamadi!" yazdırabilirsin
        return;
    }
    
    // Diskteki 1. Bölüme (Partition 0) bakalım
    PartitionEntry* part1 = &mbr->partitions[0];
    
    // Partition ID kontrolü (0x0B = FAT32 with CHS, 0x0C = FAT32 with LBA)
    if (part1->partition_id == 0x0B || part1->partition_id == 0x0C) {
        fat32_start_sector = part1->start_lba;
        // İleride ekrana "FAT32 Bulundu! LBA: " + fat32_start_sector yazdırabilirsin
    }
}