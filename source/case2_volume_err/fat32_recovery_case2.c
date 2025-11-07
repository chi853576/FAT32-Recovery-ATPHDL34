/**
 * recover_volume_br.c
 * Mô phỏng phục hồi Volume Boot Record (BR) của FAT32
 * Usage: ./recover_volume_br <image_file>
 * Example: ./recover_volume_br ../base_images/test_volume_err.img
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE 512
#define MAX_SEARCH_SECTORS 2048   // Số sector đầu để tìm bản sao BR

// --- Đọc little endian ---
static inline uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// --- Kiểm tra signature (2 byte cuối = 0x55AA) ---
int is_valid_signature(const uint8_t *sec) {
    return sec[510] == 0x55 && sec[511] == 0xAA;
}

// --- Kiểm tra một số tham số hợp lệ trong BR ---
int sane_bpb(const uint8_t *sec) {
    uint16_t bytes_per_sector = le16(sec + 11);
    uint8_t sectors_per_cluster = sec[13];
    uint16_t reserved = le16(sec + 14);
    uint8_t fats = sec[16];
    uint32_t fatsz = le32(sec + 36);

    return (bytes_per_sector == 512 || bytes_per_sector == 1024 || bytes_per_sector == 4096) &&
        (sectors_per_cluster > 0 && sectors_per_cluster <= 128) &&
        reserved >= 1 && fats == 2 && fatsz > 0;
}

// --- Hàm chính ---
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image_file>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb+");
    if (!fp) {
        perror("Cannot open image file");
        return 1;
    }

    uint8_t sector[SECTOR_SIZE];
    int found = 0;
    long backup_sector_pos = 0;

    printf("[*] Searching for backup Boot Record in first %d sectors...\n", MAX_SEARCH_SECTORS);

    for (int i = 1; i < MAX_SEARCH_SECTORS; i++) {
        fseek(fp, i * SECTOR_SIZE, SEEK_SET);
        fread(sector, 1, SECTOR_SIZE, fp);

        if (is_valid_signature(sector) && sane_bpb(sector)) {
            printf("[+] Possible valid BR found at sector %d\n", i);
            found = 1;
            backup_sector_pos = i;
            break;
        }
    }

    if (!found) {
        printf("[-] No valid backup BR found. Cannot recover.\n");
        fclose(fp);
        return 1;
    }

    // Đọc bản sao BR hợp lệ
    fseek(fp, backup_sector_pos * SECTOR_SIZE, SEEK_SET);
    fread(sector, 1, SECTOR_SIZE, fp);

    // Ghi đè vào sector 0 (BR chính)
    fseek(fp, 0, SEEK_SET);
    fwrite(sector, 1, SECTOR_SIZE, fp);
    fflush(fp);

    printf("[✓] Recovered main Boot Record from backup sector %ld successfully.\n", backup_sector_pos);

    fclose(fp);
    return 0;
}
