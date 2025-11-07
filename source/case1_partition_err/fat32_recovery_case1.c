// File: fat32_recovery_case1.c (Đã cập nhật)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SECTOR_SIZE 512
#define MBR_PARTITION_TABLE_OFFSET 0x1BE // 446
#define PARTITION_TABLE_SIZE 0x40        // 64 bytes
#define MBR_SIGNATURE_OFFSET 510         // 512 - 2

#pragma pack(push, 1)
typedef struct
{
    uint8_t status;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba_start;
    uint32_t total_sectors;
} PartitionEntry;
#pragma pack(pop)

// -----------------------------------------------------------------
// GIẢI PHÁP CASE B: Phục hồi từ Bản sao
// -----------------------------------------------------------------
int recover_from_copy(FILE *file)
{
    printf("[Case B] Dang thu khoi phuc tu ban sao (sector cuoi cung)...\n");
    long file_size;
    long last_sector_offset;
    unsigned char last_sector_buffer[SECTOR_SIZE];
    unsigned char mbr_buffer[SECTOR_SIZE];

    // 1. Lấy kích thước file để xác định sector cuối cùng
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    if (file_size < SECTOR_SIZE * 2)
    {
        printf("[Case B] Loi: File anh qua nho.\n");
        return 0; // Thất bại
    }
    last_sector_offset = file_size - SECTOR_SIZE;

    // 2. Đọc Sector Cuối cùng (Bản sao MBR/PT)
    fseek(file, last_sector_offset, SEEK_SET);
    if (fread(last_sector_buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Case B] Loi: Khong the doc sector cuoi cung");
        return 0; // Thất bại
    }

    // 3. Xác thực Bản sao bằng Signature (55 AA)
    if (last_sector_buffer[MBR_SIGNATURE_OFFSET] != 0x55 || last_sector_buffer[MBR_SIGNATURE_OFFSET + 1] != 0xAA)
    {
        printf("[Case B] Khong tim thay Signature MBR (55 AA) hop le o sector cuoi cung.\n");
        return 0; // Thất bại
    }

    // 4. Xác thực thêm: byte đầu tiên của Bảng Phân Vùng phải là 0x80 hoặc 0x00
    uint8_t status_byte = last_sector_buffer[MBR_PARTITION_TABLE_OFFSET];
    if (status_byte != 0x80 && status_byte != 0x00)
    {
        printf("[Case B] Ban sao MBR/PT o sector cuoi cung khong hop le (Status byte sai: 0x%02X).\n", status_byte);
        return 0; // Thất bại
    }
    printf("[Case B] Da tim thay ban sao hop le tai sector cuoi cung!\n");

    // 5. Đọc MBR bị hỏng (Sector 0)
    fseek(file, 0, SEEK_SET);
    if (fread(mbr_buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Case B] Loi: Khong the doc MBR goc");
        return 0; // Thất bại
    }

    // 6. Sao chép 64 bytes Bảng Phân Vùng từ bản sao vào MBR
    memcpy(&mbr_buffer[MBR_PARTITION_TABLE_OFFSET],
           &last_sector_buffer[MBR_PARTITION_TABLE_OFFSET],
           PARTITION_TABLE_SIZE);

    // 7. Ghi MBR đã sửa đổi trở lại Sector 0
    fseek(file, 0, SEEK_SET);
    if (fwrite(mbr_buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Case B] Loi: Khong the ghi MBR da sua vao file");
        return 0; // Thất bại
    }

    printf("[Case B] Thanh cong! Bang Phan Vung da duoc khoi phuc tu ban sao.\n");
    return 1; // Thành công
}

// -----------------------------------------------------------------
// GIẢI PHÁP CASE A: Tái tạo từ Boot Record (BR)
// -----------------------------------------------------------------
int recover_from_br(FILE *file)
{
    printf("[Case A] Dang thu khoi phuc bang cach tai tao tu Boot Record (BR)...\n");
    unsigned char buffer[SECTOR_SIZE];
    uint32_t lba_start = 0;
    uint32_t total_sectors = 0;
    int found_br = 0;

    // Bắt đầu quét từ Sector 1
    for (uint32_t sector_num = 1;; sector_num++)
    {
        if (fseek(file, (long)sector_num * SECTOR_SIZE, SEEK_SET) != 0)
        {
            break; // Hết file
        }
        if (fread(buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
        {
            break; // Hết file
        }

        // 1. Kiểm tra 55 AA
        if (buffer[MBR_SIGNATURE_OFFSET] == 0x55 && buffer[MBR_SIGNATURE_OFFSET + 1] == 0xAA)
        {
            // 2. Kiểm tra chuỗi "FAT32"
            if (strncmp((char *)&buffer[0x52], "FAT32   ", 8) == 0)
            {
                printf("[Case A] Da tim thay FAT32 Boot Record (BR) tai Sector: %u\n", sector_num);
                lba_start = sector_num;
                total_sectors = *(uint32_t *)&buffer[0x20];
                printf("[Case A]  -> Vi tri bat dau (LBA Start): %u\n", lba_start);
                printf("[Case A]  -> Tong so Sector: %u\n", total_sectors);
                found_br = 1;
                break;
            }
        }
    }

    if (!found_br)
    {
        printf("[Case A] Loi: Khong tim thay Boot Record (BR) hop le. Khong the tai tao.\n");
        return 0; // Thất bại
    }

    // Tái tạo lại MBR
    fseek(file, 0, SEEK_SET);
    if (fread(buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Case A] Loi: Khong the doc MBR");
        return 0; // Thất bại
    }

    PartitionEntry *entry = (PartitionEntry *)&buffer[MBR_PARTITION_TABLE_OFFSET];
    entry->status = 0x80;
    entry->type = 0x0C;
    entry->lba_start = lba_start;
    entry->total_sectors = total_sectors;

    fseek(file, 0, SEEK_SET);
    if (fwrite(buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Case A] Loi: Khong the ghi MBR da sua vao file");
        return 0; // Thất bại
    }

    printf("[Case A] Thanh cong! Bang Phan Vung da duoc tai tao tu BR.\n");
    return 1; // Thành công
}

// -----------------------------------------------------------------
// HÀM MAIN CHÍNH
// -----------------------------------------------------------------
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Su dung: %s <ten-file-anh>\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    FILE *file = fopen(filename, "rb+");
    if (!file)
    {
        perror("Loi: Khong the mo file anh");
        return 1;
    }

    // Ưu tiên chạy Case B trước (Phục hồi từ bản sao)
    if (!recover_from_copy(file))
    {
        // Nếu Case B thất bại, chuyển sang Case A (Tái tạo từ BR)
        printf("-----------------------------------------------------------\n");
        printf("Phuong phap khoi phuc tu ban sao (Case B) that bai.\n");
        printf("Chuyen sang phuong phap tai tao tu Boot Record (Case A)...\n");
        printf("-----------------------------------------------------------\n");

        if (!recover_from_br(file))
        {
            printf("Tat ca cac phuong phap khoi phuc deu that bai!\n");
        }
    }

    fclose(file);
    return 0;
}