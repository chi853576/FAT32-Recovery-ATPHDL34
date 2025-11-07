/**
 * recover_volume_br_full_no_diacritics.c
 * Muc tieu: Phuc hoi Boot Record (BR) cua he thong FAT32.
 *
 * Cac tinh huong:
 * 1. BR chinh bi hong, con ban sao hop le -> Sao chep phuc hoi.
 * 2. BR chinh va ban sao deu hong -> Tai tao BPB/BR moi.
 * 3. BR chi sai 1 vai tham so -> Tu dong sua loi nhe.
 *
 * Cach bien dich:  gcc recover_volume_br_full_no_diacritics.c -o recover_volume_br
 * Cach chay:       ./recover_volume_br ../base_images/test_volume_err.img
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SECTOR_SIZE 512
#define MAX_SEARCH_SECTORS 4096
#define DEFAULT_RESERVED 32
#define DEFAULT_NUM_FATS 2
#define DEFAULT_ROOT_CLUSTER 2

// --- Ham doc gia tri little-endian ---
static inline uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
}
static inline void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}

// --- Kiem tra chu ky 0x55AA o cuoi sector ---
int is_valid_signature(const uint8_t *sec) {
    return sec[510] == 0x55 && sec[511] == 0xAA;
}

// --- Kiem tra tinh hop le cua BPB (thong so dinh dang FAT32) ---
int sane_bpb(const uint8_t *sec) {
    uint16_t bytes_per_sector = le16(sec + 11);
    uint8_t sectors_per_cluster = sec[13];
    uint16_t reserved = le16(sec + 14);
    uint8_t fats = sec[16];
    uint32_t fatsz32 = le32(sec + 36);
    uint32_t root_cluster = le32(sec + 44);

    if (!(bytes_per_sector == 512 || bytes_per_sector == 1024 || bytes_per_sector == 2048 || bytes_per_sector == 4096))
        return 0;
    if (sectors_per_cluster == 0 || (sectors_per_cluster & (sectors_per_cluster - 1)) != 0)
        return 0; // phai la boi cua 2
    if (reserved == 0 || reserved > 65535)
        return 0;
    if (!(fats == 1 || fats == 2))
        return 0;
    if (fatsz32 == 0)
        return 0;
    if (root_cluster < 2)
        return 0;
    return 1;
}

// --- Doc va ghi sector ---
int read_sector(FILE *f, uint8_t *buf, long sector) {
    fseek(f, sector * SECTOR_SIZE, SEEK_SET);
    return fread(buf, 1, SECTOR_SIZE, f) == SECTOR_SIZE;
}
int write_sector(FILE *f, const uint8_t *buf, long sector) {
    fseek(f, sector * SECTOR_SIZE, SEEK_SET);
    int ok = fwrite(buf, 1, SECTOR_SIZE, f) == SECTOR_SIZE;
    fflush(f);
    return ok;
}

// --- Ham tai tao BR moi khi khong con ban sao ---
void reconstruct_bpb(uint8_t *sec, uint64_t total_sectors) {
    memset(sec, 0, SECTOR_SIZE);
    sec[0] = 0xEB; sec[1] = 0x58; sec[2] = 0x90;
    memcpy(sec + 3, "MSDOS5.0", 8);

    write_le16(sec + 11, 512);       // bytes/sector
    sec[13] = 8;                     // sectors/cluster
    write_le16(sec + 14, DEFAULT_RESERVED);
    sec[16] = DEFAULT_NUM_FATS;
    sec[21] = 0xF8;                  // media descriptor
    write_le32(sec + 32, (uint32_t)total_sectors);
    write_le32(sec + 36, 8192);      // FAT size gia dinh
    write_le32(sec + 44, DEFAULT_ROOT_CLUSTER);
    write_le16(sec + 48, 1);         // FSInfo sector
    write_le16(sec + 50, 6);         // backup sector
    sec[66] = 0x29;                  // Extended boot signature

    uint32_t volid = (uint32_t)time(NULL);
    write_le32(sec + 67, volid);

    memcpy(sec + 71, "NO NAME    ", 11);
    memcpy(sec + 82, "FAT32   ", 8);

    sec[510] = 0x55; sec[511] = 0xAA;
}

// --- Ham chinh ---
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Cach dung: %s <ten_file_anh>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb+");
    if (!fp) {
        perror("Khong the mo file anh");
        return 2;
    }

    uint8_t sector[SECTOR_SIZE];
    read_sector(fp, sector, 0);

    // Truong hop 3: BR chinh hop le mot phan, sua loi nhe
    if (is_valid_signature(sector) && !sane_bpb(sector)) {
        printf("[!] BR chinh co loi nhe, tien hanh sua...\n");
        write_le16(sector + 11, 512);
        if (sector[13] == 0) sector[13] = 8;
        if (le16(sector + 14) == 0) write_le16(sector + 14, 32);
        if (sector[16] != 2) sector[16] = 2;
        sector[510] = 0x55; sector[511] = 0xAA;
        write_sector(fp, sector, 0);
        printf("[✓] Da sua thanh cong cac tham so nho trong BR chinh.\n");
        fclose(fp);
        return 0;
    }

    // Truong hop 1: BR chinh hong, tim ban sao
    printf("[*] BR chinh khong hop le, dang tim ban sao trong %d sector dau...\n", MAX_SEARCH_SECTORS);
    int found = 0;
    long found_sector = -1;
    for (long i = 1; i < MAX_SEARCH_SECTORS; i++) {
        if (!read_sector(fp, sector, i)) break;
        if (is_valid_signature(sector) && sane_bpb(sector)) {
            found = 1; found_sector = i;
            printf("[+] Tim thay BR sao luu hop le tai sector %ld\n", i);
            break;
        }
    }

    if (found) {
        write_sector(fp, sector, 0);
        printf("[✓] Da phuc hoi BR chinh tu ban sao tai sector %ld.\n", found_sector);
        fclose(fp);
        return 0;
    }

    // Truong hop 2: BR va ban sao deu hong -> tai tao moi
    printf("[!] Khong tim thay BR hop le. Tien hanh tai tao BR moi...\n");
    fseek(fp, 0, SEEK_END);
    uint64_t total_bytes = ftell(fp);
    uint64_t total_sectors = total_bytes / SECTOR_SIZE;

    reconstruct_bpb(sector, total_sectors);
    write_sector(fp, sector, 0);
    printf("[✓] Da tai tao BR moi thanh cong (FAT32 mac dinh, %llu sector)\n", total_sectors);

    fclose(fp);
    return 0;
}
