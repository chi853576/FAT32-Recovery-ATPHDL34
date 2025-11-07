#include <stdio.h>
#include <stdint.h>

#pragma pack(push, 1)
struct BootSector {
    uint8_t  jmpBoot[3];
    uint8_t  OEMName[8];
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint16_t reservedSectorCount;
    uint8_t  numFATs;
    uint16_t rootEntryCount;
    uint16_t totalSectors16;
    uint8_t  media;
    uint16_t FATSize16;
    uint16_t sectorsPerTrack;
    uint16_t numHeads;
    uint32_t hiddenSectors;
    uint32_t totalSectors32;
    uint32_t FATSize32;
    uint16_t extFlags;
    uint16_t FSVersion;
    uint32_t rootCluster;
    uint16_t FSInfo;
    uint16_t backupBootSector;
    uint8_t  reserved[12];
    uint8_t  driveNumber;
    uint8_t  reserved1;
    uint8_t  bootSignature;
    uint32_t volumeID;
    uint8_t  volumeLabel[11];
    uint8_t  fileSystemType[8];
};
#pragma pack(pop)

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Cách dùng: %s <tên_file.img>\n", argv[0]);
        printf("Ví dụ: %s fat32_case4.img\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Không mở được file ảnh");
        return 1;
    }

    struct BootSector bpb;
    fread(&bpb, sizeof(struct BootSector), 1, fp);
    fclose(fp);

    printf("=== Thông tin Boot Sector (FAT32) ===\n");
    printf("File ảnh: %s\n", filename);
    printf("Bytes per sector      : %u\n", bpb.bytesPerSector);
    printf("Sectors per cluster   : %u\n", bpb.sectorsPerCluster);
    printf("Reserved sectors      : %u\n", bpb.reservedSectorCount);
    printf("Number of FATs        : %u\n", bpb.numFATs);
    printf("Sectors per FAT       : %u\n", bpb.FATSize32);
    printf("Root cluster          : %u\n", bpb.rootCluster);
    printf("Hidden sectors        : %u\n", bpb.hiddenSectors);
    printf("Total sectors         : %u\n", bpb.totalSectors32);

    uint32_t bytesPerCluster = bpb.bytesPerSector * bpb.sectorsPerCluster;
    uint32_t fatRegionStart = bpb.reservedSectorCount * bpb.bytesPerSector;
    uint32_t dataRegionStart = (bpb.reservedSectorCount + (bpb.numFATs * bpb.FATSize32)) * bpb.bytesPerSector;

    printf("\n=== Tính toán vị trí ===\n");
    printf("Kích thước mỗi cluster: %u bytes\n", bytesPerCluster);
    printf("Offset bắt đầu vùng FAT1 : %u bytes\n", fatRegionStart);
    printf("Offset bắt đầu vùng Data : %u bytes\n", dataRegionStart);
    printf("Cluster 2 bắt đầu tại offset: %u bytes\n", dataRegionStart + (2 - 2) * bytesPerCluster);

    return 0;
}
