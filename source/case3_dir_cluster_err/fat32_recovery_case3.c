#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>
#include <locale.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

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
    wchar_t wname[256];
    int wpos = 0;
    int found_lfn = 0;
    memset(wname, 0, sizeof(wname));

    for (int i = start_idx; i >= 0 && wpos < 250; i--)
    {
        // nếu không phải LFN entry thì dừng
        if (buffer[i * 32 + 11] != 0x0F)
            break;

        uint8_t *p = buffer + i * 32;
        uint8_t order = p[0];
        uint16_t name1[5];
        uint16_t name2[6];
        uint16_t name3[2];

        for (int k = 0; k < 5; k++)
            name1[k] = (uint16_t)(p[1 + k * 2] | ((uint16_t)p[2 + k * 2] << 8));
        for (int k = 0; k < 6; k++)
            name2[k] = (uint16_t)(p[14 + k * 2] | ((uint16_t)p[15 + k * 2] << 8));
        for (int k = 0; k < 2; k++)
            name3[k] = (uint16_t)(p[28 + k * 2] | ((uint16_t)p[29 + k * 2] << 8));

        if (order & 0x40)
            found_lfn = 1; // entry đầu (highest order)

        for (int j = 0; j < 5 && wpos < 250; j++)
            if (name1[j] != 0xFFFF)
                wname[wpos++] = name1[j];
        for (int j = 0; j < 6 && wpos < 250; j++)
            if (name2[j] != 0xFFFF)
                wname[wpos++] = name2[j];
        for (int j = 0; j < 2 && wpos < 250; j++)
            if (name3[j] != 0xFFFF)
                wname[wpos++] = name3[j];
    }

    if (!found_lfn)
        return 0;

    wname[wpos] = L'\0';
    // Chuyển wchar -> multibyte (theo locale)
    wcstombs(long_name, wname, 256);
    return 1;
}

// Khôi phục nội dung file từ cluster bắt đầu (giả sử không phân mảnh)
void recover_file_from_cluster(FILE *img, Fat32Info *info, uint32_t start_cluster, const char *filename, uint32_t size, uint8_t *visited)
{
    FILE *out = fopen(filename, "wb");
    if (!out)
    {
        perror("fopen out");
        return;
    }

    uint8_t *buf = malloc(info->cluster_size_bytes);
    if (!buf)
    {
        perror("malloc");
        fclose(out);
        return;
    }

    uint32_t cluster = start_cluster;
    uint32_t remaining = size;

    while (remaining > 0 && cluster >= 2 && cluster < info->total_clusters)
    {
        uint32_t sector = cluster_to_sector(info, cluster);
        if (fseek(img, sector * 512, SEEK_SET) != 0)
        {
            printf("  Warning: fseek failed at cluster %u\n", cluster);
            break;
        }
        size_t read_sz = (remaining > info->cluster_size_bytes) ? info->cluster_size_bytes : remaining;
        if (fread(buf, 1, read_sz, img) != read_sz)
        {
            printf("  Warning: Read failed at cluster %u\n", cluster);
            break;
        }
        fwrite(buf, 1, read_sz, out);
        remaining -= read_sz;
        cluster++; // GIẢ SỬ file clusters là liên tiếp
    }

    free(buf);
    fclose(out);
    printf("    Recovered file: %s (%u bytes)\n", filename, size);
}

// Đệ quy quét một directory (SDET) bắt đầu tại start_cluster và khôi phục cấu trúc bên trong
void recover_directory(FILE *img, Fat32Info *info, uint32_t start_cluster, const char *out_path, int depth, uint8_t *visited)
{
    if (depth > 32)
    {
        printf("    Max recursion depth reached at %s\n", out_path);
        return;
    }

    #ifdef _WIN32
    _mkdir(out_path);
    #else
    mkdir(out_path, 0755);
    #endif

    uint8_t *cluster_buf = malloc(info->cluster_size_bytes);
    if (!cluster_buf)
    {
        perror("malloc");
        return;
    }

    int entries_per_cluster = info->cluster_size_bytes / 32;
    uint32_t cluster = start_cluster;

    // Duyệt các cluster của directory. Giả sử directory clusters liêp tiếp nhau (không phân mảnh).
    while (cluster >= 2 && cluster < info->total_clusters)
    {
        if (visited[cluster])
        {
            printf("      Cluster %u already visited, skip\n", cluster);
            break;
        }
        visited[cluster] = 1;
        uint32_t sector = cluster_to_sector(info, cluster);
        if (fseek(img, sector * 512, SEEK_SET) != 0)
        {
            printf("    Warning: fseek failed reading dir cluster %u\n", cluster);
            break;
        }
        if (fread(cluster_buf, 1, info->cluster_size_bytes, img) != info->cluster_size_bytes)
        {
            printf("    Warning: Read error at directory cluster %u (sector %u)\n", cluster, sector);
            break;
        }

        // Duyệt từng entry trong cluster
        for (int i = 0; i < entries_per_cluster; i++)
        {
            uint8_t *entry_raw = cluster_buf + i * 32;
            DirEntry *entry = (DirEntry *)entry_raw;

            // nếu entry trống => end of directory entries (theo FAT spec)
            if (entry->name[0] == 0x00)
            {
                // không cần đọc các cluster tiếp theo (nếu có) vì thư mục kết thúc
                free(cluster_buf);
                return;
            }

            // Bỏ entry xóa
            if (entry->name[0] == 0xE5)
                continue;

            // Bỏ các entry system/volume label nếu muốn
            // Nếu LFN entry thì sẽ được xử lý trước khi entry 8.3 tương ứng
            if (entry->attr == 0x0F)
                continue;

            // Lấy tên LFN nếu có (LFN nằm ngay phía trước entry 8.3)
            char long_name[256] = "";
            int has_lfn = 0;
            if (i > 0 && cluster_buf[(i - 1) * 32 + 11] == 0x0F)
            {
                has_lfn = read_lfn(cluster_buf, i - 1, entries_per_cluster, long_name);
            }

            // Nếu entry là thư mục
            if (entry->attr & 0x10)
            {
                // skip "." and ".."
                char shortname[13];
                print_short_name(entry->name, shortname);
                if (strcmp(shortname, ".") == 0 || strcmp(shortname, "..") == 0)
                    continue;

                // quyết định tên thư mục cuối cùng: ưu tiên LFN nếu có
                char final_name[256];
                if (has_lfn && strlen(long_name) > 0)
                    strncpy(final_name, long_name, sizeof(final_name));
                else
                    strncpy(final_name, shortname, sizeof(final_name));
                final_name[sizeof(final_name)-1] = '\0';

                // tạo đường dẫn out_path/final_name
                char new_out[1024];
                snprintf(new_out, sizeof(new_out), "%s/%s", out_path, final_name);

                printf("    Directory: %s (cluster %u)\n", new_out, (entry->cluster_high << 16) | entry->cluster_low);

                uint32_t dir_cluster = (entry->cluster_high << 16) | entry->cluster_low;
                if (dir_cluster >= 2 && dir_cluster < info->total_clusters)
                {
                    // đệ quy quét thư mục con
                    recover_directory(img, info, dir_cluster, new_out, depth + 1, visited);
                }
                else
                {
                    printf("      Warning: invalid cluster for directory %s\n", new_out);
                }

                continue;
            }

            // Nếu entry là file
            if (entry->attr & 0x20)
            {
                char shortname[13];
                print_short_name(entry->name, shortname);
                char final_name[256];
                if (has_lfn && strlen(long_name) > 0)
                    strncpy(final_name, long_name, sizeof(final_name));
                else
                    strncpy(final_name, shortname, sizeof(final_name));
                final_name[sizeof(final_name)-1] = '\0';

                // chuẩn bị đường dẫn output file
                char out_file[1024];
                snprintf(out_file, sizeof(out_file), "%s/%s", out_path, final_name);

                uint32_t file_cluster = (entry->cluster_high << 16) | entry->cluster_low;
                printf("    File: %s (cluster %u, size %u)\n", out_file, file_cluster, entry->file_size);

                if (file_cluster >= 2 && file_cluster < info->total_clusters)
                {
                    recover_file_from_cluster(img, info, file_cluster, out_file, entry->file_size, visited);
                }
                else
                {
                    printf("Warning: invalid cluster for file %s\n", out_file);
                }
                continue;
            }

        }

        // move to next cluster of directory (GIẢ SỬ liên tiếp)
        cluster++;
    }

    free(cluster_buf);
}

int main()
{
    setlocale(LC_ALL, "");

    const char *img_name = "case3_deleted_file_test.img";
    FILE *img = fopen(img_name, "rb");
    if (!img)
    {
        perror("open img");
        return 1;
    }

    fseek(img, 0, SEEK_END);
    long img_size = ftell(img);
    fseek(img, 0, SEEK_SET);
    uint32_t total_sectors = (uint32_t)(img_size / 512);

    Fat32Info info = {0};
    uint8_t boot[512];
    if (fread(boot, 1, 512, img) != 512)
    {
        printf("Error: Cannot read boot sector\n");
        fclose(img);
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
    info.total_clusters = (total_sectors - info.data_start_sector) / info.sectors_per_cluster + 2;

    printf("=== FAT32 INFO ===\n");
    printf("Image size: %ld bytes (%u sectors)\n", img_size, total_sectors);
    printf("Reserved: %u, FATs: %u, Sectors/FAT: %lu\n", info.reserved_sectors, info.num_fats, info.sectors_per_fat);
    printf("Sectors per cluster: %u\n", info.sectors_per_cluster);
    printf("Data start: sector %u\n", info.data_start_sector);
    printf("Total clusters: %u\n\n", info.total_clusters);

    // tạo thư mục recovered
    system("mkdir recovered");

    // Duyệt các cluster tìm SDET (cluster có entry 0 = "." và entry 1 = "..")
    uint8_t *cluster_buf = malloc(info.cluster_size_bytes);
    if (!cluster_buf)
    {
        perror("malloc");
        fclose(img);
        return 1;
    }
    int entries_per_cluster = info.cluster_size_bytes / 32;

    printf("Scanning clusters 2 to %u for SDET...\n", info.total_clusters - 1);
    uint8_t *visited = calloc(info.total_clusters, 1);

    for (uint32_t cluster = 2; cluster < info.total_clusters; cluster++)
    {
        if (visited[cluster]) continue;
        uint32_t sector = cluster_to_sector(&info, cluster);
        if (sector >= total_sectors)
        {
            printf("Stop: Sector %u out of range\n", sector);
            break;
        }

        if (fseek(img, sector * 512, SEEK_SET) != 0)
        {
            printf("Warning: fseek error at cluster %u\n", cluster);
            break;
        }

        if (fread(cluster_buf, 1, info.cluster_size_bytes, img) != info.cluster_size_bytes)
        {
            printf("Warning: Read error at cluster %u (sector %u)\n", cluster, sector);
            break;
        }

        // Kiểm tra signature directory: entry 0 = ".", entry 1 = ".."
        DirEntry *entry0 = (DirEntry *)cluster_buf;
        DirEntry *entry1 = (DirEntry *)(cluster_buf + 32);
        char dot_name[13], dotdot_name[13];
        print_short_name(entry0->name, dot_name);
        print_short_name(entry1->name, dotdot_name);

        if (entry0->attr == 0x10 && strcmp(dot_name, ".") == 0 &&
            entry1->attr == 0x10 && strcmp(dotdot_name, "..") == 0)
        {
            printf("Found potential SDET cluster %u\n", cluster);
            // tạo thư mục chứa SDET này
            char dir_path[1024];
            snprintf(dir_path, sizeof(dir_path), "recovered/Dir_%u", cluster);
            #ifdef _WIN32
            _mkdir(dir_path);
            #else
            mkdir(dir_path, 0755);
            #endif

            // gọi recover_directory trên cluster này (đệ quy)
            recover_directory(img, &info, cluster, dir_path, 0, visited);
        }
    }

    free(cluster_buf);
    fclose(img);

    printf("\nDone! Check 'recovered/' folder.\n");
    return 0;
}
