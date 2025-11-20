// File: fat32_recovery_case1.c
// Phiên bản: Root Access Version + Custom Mount Point + Unmount Option
// Cách chạy:
// 1. gcc fat32_recovery_case1.c -o recovery_tool
// 2. sudo ./recovery_tool ../../base_images/fat32_partition_errA.img  <-- QUAN TRỌNG: Phải chạy bằng sudo
// 3. Thuc hien cac chuc nang theo menu

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h> // Thu vien de dung geteuid()

// ==========================================================
// ĐỊNH NGHĨA VÀ CẤU TRÚC
// ==========================================================
#define SECTOR_SIZE 512
#define MBR_PARTITION_TABLE_OFFSET 0x1BE
#define PARTITION_TABLE_SIZE 0x40
#define MBR_SIGNATURE_OFFSET 510

// BIẾN TOÀN CỤC ĐỂ QUẢN LÝ TRẠNG THÁI MOUNT
char current_mount_point[256] = "";
int is_mounted = 0; // 0: Chua mount, 1: Da mount

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

// ==========================================================
// Chuc nang 1: Xem 64 bytes
// ==========================================================
void view_partition_table(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        perror("[Chuc nang 1] Loi mo file");
        return;
    }
    fseek(f, MBR_PARTITION_TABLE_OFFSET, SEEK_SET);
    unsigned char buffer[PARTITION_TABLE_SIZE];
    size_t read_bytes = fread(buffer, 1, PARTITION_TABLE_SIZE, f);

    printf("--- [Chuc nang 1] Dang xem 64 bytes tai 0x1BE ---\n\n");
    if (read_bytes == PARTITION_TABLE_SIZE)
    {
        for (int i = 0; i < PARTITION_TABLE_SIZE; i++)
        {
            printf("%02X ", buffer[i]);
            if ((i + 1) % 16 == 0)
                printf("\n");
        }
    }
    else
    {
        printf("Loi: Doc khong du 64 bytes.\n");
    }
    printf("--------------------------------------------------\n");
    fclose(f);
}

// ==========================================================
// Chuc nang 2: Tao backup
// ==========================================================
void create_backup(const char *filename)
{
    FILE *file = fopen(filename, "rb+");
    if (!file)
    {
        perror("[Chuc nang 2] Loi mo file\n");
        return;
    }
    unsigned char buffer[SECTOR_SIZE];
    fseek(file, 0, SEEK_SET);
    if (fread(buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Chuc nang 2] Loi doc Sector 0\n");
        fclose(file);
        return;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    long last_sector_offset = file_size - SECTOR_SIZE;
    if (file_size < SECTOR_SIZE * 2)
    {
        printf("[Chuc nang 2] Loi: File qua nho de tao backup.\n\n");
        fclose(file);
        return;
    }
    fseek(file, last_sector_offset, SEEK_SET);
    if (fwrite(buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Chuc nang 2] Loi ghi vao sector cuoi cung\n");
        fclose(file);
        return;
    }
    printf("[Chuc nang 2] Thanh cong: Da sao chep MBR (Sector 0) vao sector cuoi cung (Offset %ld).\n", last_sector_offset);
    fclose(file);
}

// ==========================================================
// Chuc nang 3: Ghi de (lam hong) 64 bytes
// ==========================================================
void corrupt_partition_table(const char *filename)
{
    FILE *f = fopen(filename, "r+b");
    if (!f)
    {
        perror("[Chuc nang 3] Loi mo file\n");
        return;
    }
    if (fseek(f, MBR_PARTITION_TABLE_OFFSET, SEEK_SET) != 0)
    {
        printf("[Chuc nang 3] Loi: fseek that bai!\n\n");
        fclose(f);
        return;
    }
    unsigned char empty_data[PARTITION_TABLE_SIZE];
    memset(empty_data, 0, PARTITION_TABLE_SIZE);
    size_t written_bytes = fwrite(empty_data, 1, PARTITION_TABLE_SIZE, f);
    if (written_bytes == PARTITION_TABLE_SIZE)
    {
        printf("[Chuc nang 3] Thanh cong: Da ghi de 64 bytes tai 0x1BE thanh 0.\n\n");
    }
    else
    {
        printf("[Chuc nang 3] That bai: Khong the ghi du 64 bytes.\n");
    }
    fclose(f);
}

// ==========================================================
// CÁC HÀM PHỤC HỒI
// ==========================================================

int recover_from_copy(FILE *file)
{
    printf("[Case A] Dang thu khoi phuc tu ban sao (sector cuoi cung)...\n");
    long file_size;
    long last_sector_offset;
    unsigned char last_sector_buffer[SECTOR_SIZE];
    unsigned char mbr_buffer[SECTOR_SIZE];

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    if (file_size < SECTOR_SIZE * 2)
    {
        printf("[Case A] Loi: File anh qua nho.\n");
        return 0;
    }
    last_sector_offset = file_size - SECTOR_SIZE;

    fseek(file, last_sector_offset, SEEK_SET);
    if (fread(last_sector_buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Case A] Loi: Khong the doc sector cuoi cung");
        return 0;
    }

    if (last_sector_buffer[MBR_SIGNATURE_OFFSET] != 0x55 || last_sector_buffer[MBR_SIGNATURE_OFFSET + 1] != 0xAA)
    {
        printf("[Case A] Khong tim thay Signature MBR (55 AA) hop le o sector cuoi cung.\n");
        return 0;
    }

    uint8_t status_byte = last_sector_buffer[MBR_PARTITION_TABLE_OFFSET];
    if (status_byte != 0x80 && status_byte != 0x00)
    {
        printf("[Case A] Ban sao MBR/PT o sector cuoi cung khong hop le (Status byte sai: 0x%02X).\n", status_byte);
        return 0;
    }
    printf("[Case A] Da tim thay ban sao hop le tai sector cuoi cung!\n");

    fseek(file, 0, SEEK_SET);
    if (fread(mbr_buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Case A] Loi: Khong the doc MBR goc");
        return 0;
    }

    memcpy(&mbr_buffer[MBR_PARTITION_TABLE_OFFSET],
           &last_sector_buffer[MBR_PARTITION_TABLE_OFFSET],
           PARTITION_TABLE_SIZE);

    fseek(file, 0, SEEK_SET);
    if (fwrite(mbr_buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Case A] Loi: Khong the ghi MBR da sua vao file");
        return 0;
    }

    printf("[Case A] Thanh cong! Bang Phan Vung da duoc khoi phuc tu ban sao.\n");
    return 1;
}

int recover_from_br(FILE *file)
{
    printf("[Case B] Dang thu khoi phuc bang cach tai tao tu Boot Record (BR)...\n");
    unsigned char buffer[SECTOR_SIZE];
    uint32_t lba_start = 0;
    uint32_t total_sectors = 0;
    int found_br = 0;

    for (uint32_t sector_num = 1;; sector_num++)
    {
        if (fseek(file, (long)sector_num * SECTOR_SIZE, SEEK_SET) != 0)
            break;
        if (fread(buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
            break;

        if (buffer[MBR_SIGNATURE_OFFSET] == 0x55 && buffer[MBR_SIGNATURE_OFFSET + 1] == 0xAA)
        {
            if (strncmp((char *)&buffer[0x52], "FAT32   ", 8) == 0)
            {
                printf("[Case B] Da tim thay FAT32 Boot Record (BR) tai Sector: %u\n", sector_num);
                lba_start = sector_num;
                total_sectors = *(uint32_t *)&buffer[0x20];
                printf("[Case B]  -> Vi tri bat dau (LBA Start): %u\n", lba_start);
                printf("[Case B]  -> Tong so Sector: %u\n", total_sectors);
                found_br = 1;
                break;
            }
        }
    }

    if (!found_br)
    {
        printf("[Case B] Loi: Khong tim thay Boot Record (BR) hop le. Khong the tai tao.\n");
        return 0;
    }

    fseek(file, 0, SEEK_SET);
    if (fread(buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Case B] Loi: Khong the doc MBR");
        return 0;
    }

    PartitionEntry *entry = (PartitionEntry *)&buffer[MBR_PARTITION_TABLE_OFFSET];
    entry->status = 0x80;
    entry->type = 0x0C;
    entry->lba_start = lba_start;
    entry->total_sectors = total_sectors;

    fseek(file, 0, SEEK_SET);
    if (fwrite(buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("[Case B] Loi: Khong the ghi MBR da sua vao file");
        return 0;
    }

    printf("[Case B] Thanh cong! Bang Phan Vung da duoc tai tao tu BR.\n");
    return 1;
}

void run_recovery(const char *filename)
{
    printf("--- [Chuc nang 4] Bat dau qua trinh phuc hoi ---\n\n");

    FILE *file = fopen(filename, "rb+");
    if (!file)
    {
        perror("[Chuc nang 4] Loi: Khong the mo file anh\n");
        return;
    }

    if (!recover_from_copy(file))
    {
        printf("-----------------------------------------------------------\n");
        printf("Phuong phap khoi phuc tu ban sao (Case A) that bai.\n");
        printf("Chuyen sang phuong phap tai tao tu Boot Record (Case B)...\n");
        printf("-----------------------------------------------------------\n");

        rewind(file);

        if (!recover_from_br(file))
        {
            printf("Tat ca cac phuong phap khoi phuc deu that bai!\n");
        }
    }

    fclose(file);
    printf("--- [Chuc nang 4] Qua trinh phuc hoi hoan tat ---\n");
}

// ==========================================================
// Chuc nang 5: Mount file anh (Custom Input)
// ==========================================================
void mount_image(const char *filename)
{
    // Kiem tra xem da mount chua
    if (is_mounted)
    {
        printf("\n[Canh bao] Ban dang co mot folder da mount tai: %s\n", current_mount_point);
        printf("Vui long Unmount (Chon option 6) truoc khi mount cai moi.\n");
        return;
    }

    printf("\n--- [Chuc nang 5] Chuan bi Mount file anh ---\n");

    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        perror("[Chuc nang 5] Khong the mo file de doc MBR");
        return;
    }

    unsigned char mbr[SECTOR_SIZE];
    if (fread(mbr, 1, SECTOR_SIZE, f) != SECTOR_SIZE)
    {
        printf("[Chuc nang 5] Loi doc MBR.\n");
        fclose(f);
        return;
    }
    fclose(f);

    if (mbr[MBR_SIGNATURE_OFFSET] != 0x55 || mbr[MBR_SIGNATURE_OFFSET + 1] != 0xAA)
    {
        printf("[Chuc nang 5] Loi: MBR Signature khong hop le (Khong phai 55 AA).\n");
        printf("Luu y: Hay chay Chuc nang 4 (Phuc hoi) truoc khi Mount.\n");
        return;
    }

    PartitionEntry *pe = (PartitionEntry *)&mbr[MBR_PARTITION_TABLE_OFFSET];
    uint32_t start_sector = pe->lba_start;
    long long offset = (long long)start_sector * SECTOR_SIZE;

    printf(" -> LBA Start: %u\n", start_sector);
    printf(" -> Offset (bytes): %lld\n", offset);

    if (start_sector == 0)
    {
        printf("[Chuc nang 5] Loi: LBA Start = 0. Bang phan vung bi hong.\n");
        return;
    }

    // --- NHAP TEN THU MUC MOUNT ---
    char user_folder_name[100];
    printf("\nNhap ten thu muc ban muon mount (VD: my_data): ");
    scanf("%s", user_folder_name);

    // Tao duong dan folder
    sprintf(current_mount_point, "./%s", user_folder_name);

    char prepare_cmd[1024];
    // XOA SUDO o lenh mkdir
    sprintf(prepare_cmd, "mkdir -p %s", current_mount_point);
    system(prepare_cmd);

    char command[1024];
    // XOA SUDO o lenh mount, giu umask=000
    sprintf(command, "mount -t vfat -o loop,offset=%lld,umask=000,utf8 \"%s\" %s", offset, filename, current_mount_point);

    printf("\n[System] Dang thuc thi lenh: \n%s\n", command);

    int result = system(command);

    if (result == 0)
    {
        is_mounted = 1; // Danh dau da mount thanh cong
        printf("\n[THANH CONG] ----------------------------------\n");
        printf("File da duoc mount tai thu muc: %s\n", current_mount_point);

        printf("\n--- DANH SACH FILE TRONG FOLDER: %s ---\n", current_mount_point);
        char ls_cmd[1024];
        sprintf(ls_cmd, "ls -l %s", current_mount_point);
        system(ls_cmd);

        printf("\n[HUONG DAN] De xem noi dung file, hay dung lenh cat:\n");
        printf("   cat %s/<ten_file>\n", current_mount_point);
        printf("Vi du: cat %s/nhat_ky.txt\n", current_mount_point);

        printf("-----------------------------------------------\n");
    }
    else
    {
        printf("\n[THAT BAI] Mount khong thanh cong.\n");
        // Reset lai bien toan cuc neu that bai
        current_mount_point[0] = '\0';
        is_mounted = 0;
    }
}

// ==========================================================
// Chuc nang 6: Unmount Folder (MOI)
// ==========================================================
void unmount_folder()
{
    printf("\n--- [Chuc nang 6] Unmount Folder ---\n");

    if (is_mounted == 0)
    {
        printf("[Loi] Ban chua thuc hien mount (Option 5) hoac da unmount roi.\n");
        printf("Khong co gi de unmount ca.\n");
        return;
    }

    printf("Dang tien hanh go bo thu muc: %s\n", current_mount_point);

    char cmd[1024];
    // Unmount truoc
    sprintf(cmd, "umount %s", current_mount_point);
    int res = system(cmd);

    if (res == 0)
    {
        // Xoa thu muc rong
        sprintf(cmd, "rmdir %s", current_mount_point);
        system(cmd);

        printf("[Thanh cong] Da unmount va xoa thu muc tam.\n");

        // Reset trang thai
        is_mounted = 0;
        current_mount_point[0] = '\0';
    }
    else
    {
        printf("[Loi] Khong the unmount. Co the ban dang mo thu muc do o terminal khac?\n");
    }
}

// ==========================================================
// HÀM MAIN CHÍNH
// ==========================================================
int main(int argc, char *argv[])
{
    // BUOC KIEM TRA QUYEN ROOT (BAT BUOC)
    if (geteuid() != 0)
    {
        printf("Loi: Chuong trinh nay can phai chay bang quyen root de thuc hien lenh mount.\n");
        printf("Vui long chay lai bang lenh: sudo %s <ten-file>\n", argv[0]);
        return 1;
    }

    if (argc != 2)
    {
        printf("Su dung: sudo %s <ten-file-anh>\n", argv[0]);
        printf("Vi du: sudo %s ../../base_images/case1_err_A.img\n", argv[0]);
        return 1;
    }

    const char *disk_image = argv[1];
    int choice = 0;

    while (1)
    {
        printf("\n==========================================\n");
        printf("CONG CU PHUC HOI FAT32 (LINUX - ROOT MODE)\n");
        printf("File anh: %s\n", disk_image);
        if (is_mounted)
        {
            printf("TRANG THAI: DA MOUNT TAI [%s]\n", current_mount_point);
        }
        printf("------------------------------------------\n");
        printf("1. Xem 64 bytes (Partition Table)\n");
        printf("2. Tao backup (Ghi Sector 0 vao cuoi file)\n");
        printf("3. Ghi de 64 bytes (Lam hong Partition Table)\n");
        printf("4. CHAY PHUC HOI (Case A -> Case B)\n");
        printf("5. Mount file anh (Nhap ten folder)\n");
        printf("6. Unmount folder (Go bo mount)\n");
        printf("0. Thoat\n");
        printf("------------------------------------------\n");
        printf("Nhap lua chon cua ban: ");

        if (scanf("%d", &choice) != 1)
        {
            while (getchar() != '\n')
                ;
            choice = -1;
        }

        switch (choice)
        {
        case 1:
            view_partition_table(disk_image);
            break;
        case 2:
            create_backup(disk_image);
            break;
        case 3:
            corrupt_partition_table(disk_image);
            view_partition_table(disk_image);
            break;
        case 4:
            run_recovery(disk_image);
            view_partition_table(disk_image);
            break;
        case 5:
            mount_image(disk_image);
            break;
        case 6:
            unmount_folder();
            break;
        case 0:
            // Tu dong unmount neu nguoi dung quen
            if (is_mounted)
            {
                printf("Phat hien ban chua unmount. Dang tu dong unmount...\n");
                unmount_folder();
            }
            printf("Da thoat.\n");
            return 0;
        default:
            printf("Lua chon khong hop le. Vui long chon tu 0-6.\n");
            break;
        }
    }

    return 0;
}