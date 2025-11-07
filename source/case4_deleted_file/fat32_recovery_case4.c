
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push,1)
typedef struct {
uint8_t  jmpBoot[3];
    char     OEMName[8];
    uint16_t bytesPerSec;
    uint8_t  secPerCluster;
    uint16_t reservedSectorCount;
    uint8_t  numFATs;
    uint16_t rootEntryCount;      // for FAT12/FAT16 (0 for FAT32)
    uint16_t totalSectors16;
    uint8_t  media;
    uint16_t fatSize16;
    uint16_t secPerTrack;
    uint16_t numHeads;
    uint32_t hiddenSectors;
    uint32_t totalSectors32;

    // FAT32 extended
    uint32_t fatSize32;
    uint16_t extFlags;
    uint16_t fsVersion;
    uint32_t rootCluster;
    uint16_t fsInfo;
    uint16_t bkBootSec;
    uint8_t  reserved[12];
    uint8_t  driveNum;
    uint8_t  reserved1;
    uint8_t  bootSig;
    uint32_t volumeID;
    char     volumeLabel[11];
    char     fsType[8];
} BPBStruct;
#pragma pack(pop)

#define DIR_ENTRY_SIZE 32

// Directory entry offsets of interest within the 32 bytes
#define OFF_NAME 0        // 11 bytes (0..10)
#define OFF_ATTR 11
#define OFF_CRT_TENTHS 13
#define OFF_CRT_TIME 14
#define OFF_CRT_DATE 16
#define OFF_LAST_ACC_DATE 18
#define OFF_FIRST_CLUSTER_HIGH 20 // 2 bytes
#define OFF_LAST_WRITE_TIME 22
#define OFF_LAST_WRITE_DATE 24
#define OFF_FIRST_CLUSTER_LOW 26  // 2 bytes
#define OFF_FILE_SIZE 28          // 4 bytes

// Attributes
#define ATTR_LONG_NAME 0x0F
#define ATTR_VOLUME_ID 0x08

// FAT32 cluster marking
#define FAT32_EOC_MIN 0x0FFFFFF8u


// Hàm đọc 2 byte từ vùng nhớ p và chuyển thành giá trị 16-bit (uint16_t)
static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

// Hàm đọc 4 byte từ vùng nhớ p và chuyển thành giá trị 32-bit (uint32_t)
static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}


// Chuyển số cluster thành vị trí byte (offset) tương ứng trong file ảnh FAT32 (.img)
// Dùng để xác định vị trí thực tế của dữ liệu một cluster trong image
long cluster_to_offset(uint32_t cluster, const BPBStruct *bpb, long dataRegionOffset) {
    uint64_t clusterIndex = (uint64_t)(cluster - 2);
    uint64_t bytesPerCluster = (uint64_t)bpb->bytesPerSec * (uint64_t)bpb->secPerCluster;
    uint64_t off = (uint64_t)dataRegionOffset + clusterIndex * bytesPerCluster;
    return (long)off;
}

// Đọc giá trị FAT entry tương ứng với một cluster
// Dùng để biết cluster tiếp theo trong chuỗi dữ liệu của file (chain of clusters)
uint32_t read_fat_entry(FILE *img, uint32_t cluster, const BPBStruct *bpb, long fatOffset) {
    uint64_t entryOffset = (uint64_t)fatOffset + (uint64_t)cluster * 4ULL;
    uint8_t buf[4];
    if (fseek(img, (long)entryOffset, SEEK_SET) != 0) return 0x0;
    if (fread(buf,1,4,img) != 4) return 0x0;
    uint32_t val = read_u32_le(buf) & 0x0FFFFFFF; // lower 28 bits used
    return val;
}

// Đọc một vùng dữ liệu bất kỳ từ file ảnh FAT32 (.img) tại vị trí xác định
size_t pread_img(FILE *img, void *buf, size_t cnt, long offset) {
    if (fseek(img, offset, SEEK_SET) != 0) return 0;
    return fread(buf,1,cnt,img);
}
// Chuyển đổi tên file từ entry FAT32 sang chuỗi hợp lệ để lưu trên hệ thống hiện tại
// FAT32 dùng định dạng tên 8.3 (8 ký tự tên + 3 ký tự phần mở rộng)
void sanitize_name_for_fs(char *out, const uint8_t *entry) {
    char name[9]; memset(name,0,9);
    char ext[4];  memset(ext,0,4);
    for (int i=0;i<8;i++){
        uint8_t c = entry[i];
        if (c == 0x20) break;
        if (c == 0xE5) name[i] = '_';
        else name[i] = (char)c;
    }
    for (int i=0;i<3;i++){
        uint8_t c = entry[8+i];
        if (c == 0x20) break;
        if (c == 0xE5) ext[i] = '_';
        else ext[i] = (char)c;
    }
    if (ext[0] != 0) snprintf(out, 64, "%s.%s", name, ext);
    else snprintf(out, 64, "%s", name);
}

// Kiểm tra xem tên 8.3 trong entry FAT32 có hợp lệ hay không (chỉ gồm ký tự in được)
int is_printable_shortname(const uint8_t *entry) {
    for (int i=0;i<11;i++){
        uint8_t c = entry[i];
        if (c == 0x00) return 0; // unused end
        if (c == 0xE5 || c == 0x20) continue;
        if (c < 0x20 || c > 0x7E) return 0;
    }
    return 1;
}


// Phục hồi một file bị xóa từ một entry trong thư mục FAT32
// Trả về 1 nếu phục hồi thành công, 0 nếu bỏ qua hoặc thất bại

int recover_file_from_entry(FILE *img, const BPBStruct *bpb, long dataRegionOffset, long fatOffset, const uint8_t *entry) {
// 1. Bỏ qua các entry không phải file dữ liệu
    uint8_t attr = entry[OFF_ATTR];
    if ((attr & ATTR_LONG_NAME) == ATTR_LONG_NAME) return 0;
    if ((attr & ATTR_VOLUME_ID) == ATTR_VOLUME_ID) return 0;

    // 2. Kiểm tra entry có bị xóa không
    if (entry[0] != 0xE5) return 0; // not deleted

    if (!is_printable_shortname(entry)) return 0; // avoid garbage
// 3. Đọc thông tin cụ thể của file (từ entry)
    uint16_t high = read_u16_le(entry + OFF_FIRST_CLUSTER_HIGH);
    uint16_t low  = read_u16_le(entry + OFF_FIRST_CLUSTER_LOW);
    uint32_t startCluster = ((uint32_t)high << 16) | (uint32_t)low;
    uint32_t fileSize = read_u32_le(entry + OFF_FILE_SIZE);

    if (startCluster == 0 || fileSize == 0) {
        // nothing to recover or start cluster invalid
        return 0;
    }
  // 4. Tạo tên file xuất ra ngoài (đã xử lý ký tự xóa)
    char fname[64];
    sanitize_name_for_fs(fname, entry);
    char outname[128];
    snprintf(outname, sizeof(outname), "recovered_%s.bin", fname);

    printf("[+] Deleted entry: %s  startCluster=%u  size=%u bytes -> %s\n", fname, startCluster, fileSize, outname);
    // 5. Mở file đích để ghi dữ liệu phục hồi
    FILE *out = fopen(outname, "wb");
    if (!out) {
        perror("fopen output");
        return 0;
    }

    uint32_t bytesPerCluster = bpb->bytesPerSec * bpb->secPerCluster;
    uint32_t remaining = fileSize;
    uint32_t currentCluster = startCluster;
    int usedClusters = 0;
// 6. Đọc dữ liệu từng cluster và ghi ra file
    while (remaining > 0) {
        long clusterOffset = cluster_to_offset(currentCluster, bpb, dataRegionOffset);
        uint32_t toRead = (remaining > bytesPerCluster) ? bytesPerCluster : remaining;
        uint8_t *buf = malloc(bytesPerCluster);
        if (!buf) { fclose(out); return 0; }

        size_t rr = pread_img(img, buf, toRead, clusterOffset);
        if (rr != toRead) {
            free(buf);
            break;
        }
        fwrite(buf,1,toRead,out);
        free(buf);

        remaining -= toRead;
        usedClusters++;

 // 7. Lần theo chuỗi cluster bằng bảng FAT
 // FAT entry trống → giả định file được lưu liên tục (contiguous)
        uint32_t next = read_fat_entry(img, currentCluster, bpb, fatOffset);
        if (next == 0x0) {
            currentCluster++;
        } else if (next >= FAT32_EOC_MIN) {
            // end-of-chain
            break;
        } else {
            currentCluster = next;
        }
    }

    fclose(out);
    printf("    -> recovered %u bytes using %d clusters\n", fileSize - remaining, usedClusters);
    return 1;
}


//Duyệt qua một thư mục FAT32 (thường bắt đầu tại root directory cluster).
//Đọc từng entry (mục) trong thư mục đó (mỗi entry = 32 byte).
//Nếu phát hiện entry bị xóa (0xE5), nó gọi recover_file_from_entry() để phục hồi file.
//Nếu gặp subdirectory, nó đệ quy để quét tiếp
int traverse_directory_cluster(FILE *img, const BPBStruct *bpb, long dataRegionOffset, long fatOffset, uint32_t startCluster) {
    uint32_t bytesPerCluster = bpb->bytesPerSec * bpb->secPerCluster;
    uint8_t *clusterBuf = malloc(bytesPerCluster);
    if (!clusterBuf) return 0;

    // We'll follow cluster chain if FAT indicates it; else read only startCluster (but directories usually contiguous)
    uint32_t currentCluster = startCluster;
    int done = 0;
    int foundAny = 0;
    int visited = 0;
    while (!done) {
        long off = cluster_to_offset(currentCluster, bpb, dataRegionOffset);
        size_t rr = pread_img(img, clusterBuf, bytesPerCluster, off);
        if (rr != bytesPerCluster) break;

        // scan entries in this cluster
        for (uint32_t pos = 0; pos + DIR_ENTRY_SIZE <= bytesPerCluster; pos += DIR_ENTRY_SIZE) {
            const uint8_t *entry = clusterBuf + pos;
            uint8_t first = entry[0];
            if (first == 0x00) {
                // no more entries in this directory cluster
                continue;
            }
            // If deleted marker
            if (first == 0xE5) {
                // try to recover
                if (recover_file_from_entry(img, bpb, dataRegionOffset, fatOffset, entry)) {
                    foundAny++;
                }
            } else {
                // active entry: if it's a directory (and not . and ..), we could recurse.
                uint8_t attr = entry[OFF_ATTR];
                if ((attr & 0x10) && !(attr & ATTR_VOLUME_ID)) {
                    // subdirectory; compute its starting cluster and recursively scan
                    uint16_t high = read_u16_le(entry + OFF_FIRST_CLUSTER_HIGH);
                    uint16_t low  = read_u16_le(entry + OFF_FIRST_CLUSTER_LOW);
                    uint32_t subStart = ((uint32_t)high << 16) | (uint32_t)low;
                    // skip "." and ".." names
                    char name[12]; memset(name,0,sizeof(name));
                    for (int i=0;i<11;i++) {
                        uint8_t c = entry[i];
                        name[i] = (c >= 0x20 && c <= 0x7e) ? (char)c : '?';
                    }
                    if (!(name[0]=='.')) {
                        // Recursively scan this subdirectory
                        // Warning: deep recursion possible; for small test images it's ok.
                        traverse_directory_cluster(img, bpb, dataRegionOffset, fatOffset, subStart);
                    }
                }
            }
        }

        // follow FAT to next cluster
        uint32_t next = read_fat_entry(img, currentCluster, bpb, fatOffset);
        if (next >= FAT32_EOC_MIN || next == 0x0) {
            done = 1;
        } else {
            currentCluster = next;
        }
        if (++visited > 65536) { // safety
            fprintf(stderr, "Directory chain too long, aborting traversal\n");
            break;
        }
    }

    free(clusterBuf);
    return foundAny;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <fat32_image.img>\n", argv[0]);
        return 1;
    }

    const char *imgpath = argv[1];
    FILE *img = fopen(imgpath, "rb");
    if (!img) { perror("fopen"); return 1; }

    // Đọc (BPB) — vùng đầu tiên trong FAT32 chứa thông tin cấu trúc đĩa
    BPBStruct bpb;
    if (fread(&bpb, sizeof(BPBStruct), 1, img) != 1) {
        fprintf(stderr, "Failed reading BPB\n");
        fclose(img);
        return 1;
    }

    // Basic validation
    if (bpb.bytesPerSec == 0 || bpb.secPerCluster == 0) {
        fprintf(stderr, "Not a valid FAT BPB (bad bytesPerSec or secPerCluster)\n");
        fclose(img);
        return 1;
    }

    // Lấy thông tin cơ bản từ BPB
    uint32_t bytesPerSector = bpb.bytesPerSec;
    uint32_t secPerCluster = bpb.secPerCluster;
    uint32_t reservedSectors = bpb.reservedSectorCount;
    uint32_t numFATs = bpb.numFATs;
    uint32_t sectorsPerFAT = (bpb.fatSize32 != 0) ? bpb.fatSize32 : bpb.fatSize16;
    uint32_t rootCluster = bpb.rootCluster;

    long fatOffset = (long)reservedSectors * (long)bytesPerSector;
    long dataRegionOffset = (long)(reservedSectors + numFATs * sectorsPerFAT) * (long)bytesPerSector;

    printf("BPB:\n");
    printf(" bytesPerSector=%u, secPerCluster=%u, reserved=%u, numFATs=%u, sectorsPerFAT=%u, rootCluster=%u\n",
           bytesPerSector, secPerCluster, reservedSectors, numFATs, sectorsPerFAT, rootCluster);
    printf(" FAT offset = %ld bytes, Data region offset = %ld bytes\n", fatOffset, dataRegionOffset);

    // Start scanning from root directory cluster
    printf("[*] Scanning directories starting from root cluster %u ...\n", rootCluster);
    int recovered = traverse_directory_cluster(img, &bpb, dataRegionOffset, fatOffset, rootCluster);
    if (recovered == 0) {
        printf("No deleted short-name files found (or none recovered).\n");
    } else {
        printf("Done. Recovered %d file(s).\n", recovered);
    }

    fclose(img);
    return 0;
}
