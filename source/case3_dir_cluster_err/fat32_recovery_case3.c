#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>
#include <locale.h>
#include <stddef.h>

typedef struct
{
    uint8_t name[11];
    uint8_t attr;
    uint8_t reserved;
    uint8_t create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed)) DirEntry;

typedef struct __attribute__((packed))
{
    uint8_t order;
    uint16_t name1[5];
    uint8_t attr;
    uint8_t type;
    uint8_t checksum;
    uint16_t name2[6];
    uint16_t zero;
    uint16_t name3[2];
} LfnEntry;

typedef struct
{
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint32_t sectors_per_fat;
    uint16_t root_dir_sectors;
    uint8_t sectors_per_cluster;
    uint32_t data_start_sector;
    uint32_t cluster_size_bytes;
    uint32_t total_clusters;
} Fat32Info;

uint32_t cluster_to_sector(Fat32Info *info, uint32_t cluster)
{
    return info->data_start_sector + (cluster - 2) * info->sectors_per_cluster;
}

int is_valid_name(uint8_t *name)
{
    if (name[0] == 0x00 || name[0] == 0xE5)
        return 0;
    return 1;
}

void print_short_name(uint8_t *name, char *out)
{
    int i, j = 0;
    for (i = 0; i < 8 && name[i] != ' '; i++)
        out[j++] = name[i];
    if (name[8] != ' ')
    {
        out[j++] = '.';
        for (i = 8; i < 11 && name[i] != ' '; i++)
            out[j++] = name[i];
    }
    out[j] = '\0';
}

int read_lfn(uint8_t *buffer, int start_idx, int entries_per_cluster, char *long_name)
{
    wchar_t wname[256] = {0};
    int wpos = 0;
    int found_lfn = 0;

    for (int i = start_idx; i >= 0 && wpos < 250; i--)
    {
        if (buffer[i * 32 + 11] != 0x0F)
            break;
        LfnEntry temp;
        LfnEntry *lfn = &temp;
        uint8_t *p = buffer + i * 32;
        lfn->order = p[0];
        for (int k = 0; k < 5; k++)
            lfn->name1[k] = (uint16_t)(p[1 + k * 2] | ((uint16_t)p[2 + k * 2] << 8));
        lfn->attr = p[11];
        lfn->type = p[12];
        lfn->checksum = p[13];

        for (int k = 0; k < 6; k++)
            lfn->name2[k] = (uint16_t)(p[14 + k * 2] | ((uint16_t)p[15 + k * 2] << 8));
        lfn->zero = (uint16_t)(p[26] | ((uint16_t)p[27] << 8));
        for (int k = 0; k < 2; k++)
            lfn->name3[k] = (uint16_t)(p[28 + k * 2] | ((uint16_t)p[29 + k * 2] << 8));
        // LfnEntry *lfn = (LfnEntry *)(buffer + i * 32);

        // order
        printf("order: 0x%02x (byte 0)\n", lfn->order);

        // name1
        printf("name1:\n");
        printf("sizeof(LfnEntry) = %zu\n", sizeof(LfnEntry));
        printf("offsetof(name1) = %zu\n", offsetof(LfnEntry, name1));
        for (int k = 0; k < 5; k++)
        {
            printf("  [%d]: 0x%04x ('%c') (bytes %d-%d)\n", k, lfn->name1[k], (char)lfn->name1[k], 1 + k * 2, 2 + k * 2);
        }

        // attr, type, checksum
        printf("attr: 0x%02x (byte 11)\n", lfn->attr);
        printf("type: 0x%02x (byte 12)\n", lfn->type);
        printf("checksum: 0x%02x (byte 13)\n", lfn->checksum);

        // name2
        printf("name2:\n");
        for (int k = 0; k < 6; k++)
        {
            printf("  [%d]: 0x%04x ('%c') (bytes %d-%d)\n", k, lfn->name2[k], (char)lfn->name2[k], 14 + k * 2, 15 + k * 2);
        }

        // zero
        printf("zero: 0x%04x (bytes 26-27)\n", lfn->zero);

        // name3
        printf("name3:\n");
        for (int k = 0; k < 2; k++)
        {
            printf("  [%d]: 0x%04x ('%c') (bytes %d-%d)\n", k, lfn->name3[k], (char)lfn->name3[k], 28 + k * 2, 29 + k * 2);
        }
        if (lfn->order & 0x40)
            found_lfn = 1;

        for (int j = 0; j < 5 && wpos < 250; j++)
            if (lfn->name1[j] != 0xFFFF)
                wname[wpos++] = lfn->name1[j];
        for (int j = 0; j < 6 && wpos < 250; j++)
            if (lfn->name2[j] != 0xFFFF)
                wname[wpos++] = lfn->name2[j];
        for (int j = 0; j < 2 && wpos < 250; j++)
            if (lfn->name3[j] != 0xFFFF)
                wname[wpos++] = lfn->name3[j];
    }

    if (!found_lfn)
        return 0;
    wname[wpos] = L'\0';
    wcstombs(long_name, wname, 256);
    return 1;
}

void recover_file_from_cluster(FILE *img, Fat32Info *info, uint32_t start_cluster, const char *filename, uint32_t size)
{
    FILE *out = fopen(filename, "wb");
    if (!out)
    {
        perror("fopen out");
        return;
    }

    uint8_t *buf = malloc(info->cluster_size_bytes);
    uint32_t cluster = start_cluster;
    uint32_t remaining = size;

    while (remaining > 0 && cluster >= 2 && cluster < info->total_clusters)
    {
        uint32_t sector = cluster_to_sector(info, cluster);
        fseek(img, sector * 512, SEEK_SET);
        size_t read_sz = (remaining > info->cluster_size_bytes) ? info->cluster_size_bytes : remaining;
        if (fread(buf, 1, read_sz, img) != read_sz)
        {
            printf("  Warning: Read failed at cluster %u\n", cluster);
            break;
        }
        fwrite(buf, 1, read_sz, out);
        remaining -= read_sz;
        cluster++; // giả sử không phân mảnh
    }
    free(buf);
    fclose(out);
    printf("  Recovered: %s (%u bytes)\n", filename, size);
}

// === MAIN ===
int main()
{
    setlocale(LC_ALL, "");

    FILE *img = fopen("case3_dir_cluster_err.img", "rb"); // Thay đổi tên file ảnh ổ đĩa nếu cần
    if (!img)
    {
        perror("open img");
        return 1;
    }

    // LẤY KÍCH THƯỚC FILE ẢNH
    fseek(img, 0, SEEK_END);
    long img_size = ftell(img);
    fseek(img, 0, SEEK_SET);
    uint32_t total_sectors = img_size / 512;

    Fat32Info info = {0};
    uint8_t boot[512];
    if (fread(boot, 1, 512, img) != 512)
    {
        printf("Error: Cannot read boot sector\n");
        return 1;
    }

    info.reserved_sectors = *(uint16_t *)(boot + 0x0E);
    info.num_fats = boot[0x10];
    info.sectors_per_fat = *(uint32_t *)(boot + 0x24);
    info.sectors_per_cluster = boot[0x0D];
    uint16_t root_entries = *(uint16_t *)(boot + 0x11);
    info.root_dir_sectors = ((root_entries * 32) + 511) / 512;
    info.data_start_sector = info.reserved_sectors + (info.num_fats * info.sectors_per_fat) + info.root_dir_sectors;
    info.cluster_size_bytes = info.sectors_per_cluster * 512;

    // TÍNH TỔNG SỐ CLUSTER TỪ KÍCH THƯỚC ẢNH
    info.total_clusters = (total_sectors - info.data_start_sector) / info.sectors_per_cluster + 2;

    printf("=== FAT32 INFO ===\n");
    printf("Image size: %ld bytes (%u sectors)\n", img_size, total_sectors);
    printf("Reserved: %u, FATs: %u, Sectors/FAT: %lu\n", info.reserved_sectors, info.num_fats, info.sectors_per_fat);
    printf("Sectors per cluster: %u\n", info.sectors_per_cluster);
    printf("Data start: sector %u\n", info.data_start_sector);
    printf("Total clusters: %u\n\n", info.total_clusters);

    system("mkdir -p recovered");

    uint8_t *cluster_buf = malloc(info.cluster_size_bytes);
    if (!cluster_buf)
    {
        perror("malloc");
        return 1;
    }
    int entries_per_cluster = info.cluster_size_bytes / 32;

    printf("Scanning clusters 2 to %u...\n", info.total_clusters - 1);

    for (uint32_t cluster = 2; cluster < info.total_clusters; cluster++)
    {
        uint32_t sector = cluster_to_sector(&info, cluster);
        if (sector >= total_sectors)
        {
            printf("Stop: Sector %u out of range\n", sector);
            break;
        }

        fseek(img, sector * 512, SEEK_SET);
        if (fread(cluster_buf, 1, info.cluster_size_bytes, img) != info.cluster_size_bytes)
        {
            printf("Warning: Read error at cluster %u (sector %u)\n", cluster, sector);
            break;
        }

        // TÌM SIGNATURE DIRECTORY: Entry 0 = ".", Entry 1 = ".."
        DirEntry *entry0 = (DirEntry *)cluster_buf;
        DirEntry *entry1 = (DirEntry *)(cluster_buf + 32);
        char dot_name[13];
        char dotdot_name[13];
        print_short_name(entry0->name, dot_name);
        print_short_name(entry1->name, dotdot_name);

        if (entry0->attr == 0x10 && strcmp(dot_name, ".") == 0 &&
            entry1->attr == 0x10 && strcmp(dotdot_name, "..") == 0)
        {
            printf("Found potential SDET cluster %u (signature . and .. found)\n", cluster);

            // THÊM DEBUG: In hex of first 128 bytes of directory cluster
            printf("  Directory cluster hex debug (first 128 bytes):\n");
            for (int k = 0; k < 128; k++)
            {
                printf("%02x ", cluster_buf[k]);
                if ((k + 1) % 16 == 0)
                    printf("\n");
            }
            printf("\n");

            // THÊM DEBUG: In tất cả entry trong directory
            int valid_entries = 0;
            for (int i = 2; i < entries_per_cluster; i++)
            {
                uint8_t *entry_raw = cluster_buf + i * 32;
                DirEntry *entry = (DirEntry *)entry_raw;
                if (is_valid_name(entry->name))
                {
                    char entry_name[13];
                    print_short_name(entry->name, entry_name);
                    printf("  Entry %d: Name '%s', Attr 0x%02x, Cluster %u, Size %u\n", i, entry_name, entry->attr, (entry->cluster_high << 16) | entry->cluster_low, entry->file_size);
                    valid_entries++;
                }
            }
            printf("  Found %d valid entries in this directory cluster\n", valid_entries);

            // Xử lý entry file
            for (int i = 2; i < entries_per_cluster; i++)
            {
                uint8_t *entry_raw = cluster_buf + i * 32;

                char long_name[256] = "";
                int has_lfn = 0;
                // THÊM DEBUG: Kiểm tra LFN
                if (i > 2 && (cluster_buf[(i - 1) * 32 + 11] == 0x0F))
                {
                    has_lfn = read_lfn(cluster_buf, i - 1, entries_per_cluster, long_name);
                    if (has_lfn)
                        printf("  - LFN found for entry %d: '%s'\n", i, long_name);
                }

                DirEntry *entry = (DirEntry *)entry_raw;
                if (!is_valid_name(entry->name))
                    continue;
                if (entry->attr != 0x20)
                {
                    printf("  - Skip entry %d: Not a file (attr 0x%02x)\n", i, entry->attr);
                    continue;
                }

                char short_name[13];
                print_short_name(entry->name, short_name);
                char final_name[256];
                strcpy(final_name, has_lfn && strlen(long_name) ? long_name : short_name);

                char dir_path[1024];
                snprintf(dir_path, sizeof(dir_path), "recovered/Dir_%u", cluster);
                char mkdir_cmd[1100];
                snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", dir_path);
                system(mkdir_cmd);

                char out_path[1024];
                snprintf(out_path, sizeof(out_path), "%s/%s", dir_path, final_name);

                uint32_t file_cluster = (entry->cluster_high << 16) | entry->cluster_low;
                printf("  - Recovering file '%s' from cluster %u, size %u\n", final_name, file_cluster, entry->file_size);
                recover_file_from_cluster(img, &info, file_cluster, out_path, entry->file_size);
            }
        }
    }

    free(cluster_buf);
    fclose(img);
    printf("\nDone! Check 'recovered/' folder.\n");
    return 0;
}