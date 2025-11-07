// File: create_backup.c
// Muc dich: Doc Sector 0 (MBR) va chep vao Sector cuoi cung cua file anh.

#include <stdio.h>
#include <stdlib.h>

#define SECTOR_SIZE 512

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Su dung: %s <ten-file-anh>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "rb+");
    if (!file)
    {
        perror("Loi mo file");
        return 1;
    }

    unsigned char buffer[SECTOR_SIZE];

    // 1. Doc Sector 0 (MBR)
    fseek(file, 0, SEEK_SET);
    if (fread(buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("Loi doc Sector 0");
        fclose(file);
        return 1;
    }

    // 2. Di den cuoi file de xac dinh kich thuoc
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);

    // 3. Tinh vi tri sector cuoi cung
    long last_sector_offset = file_size - SECTOR_SIZE;

    // 4. Ghi buffer (chua MBR) vao sector cuoi cung
    fseek(file, last_sector_offset, SEEK_SET);
    if (fwrite(buffer, 1, SECTOR_SIZE, file) != SECTOR_SIZE)
    {
        perror("Loi ghi vao sector cuoi cung");
        fclose(file);
        return 1;
    }

    printf("Da sao chep MBR (Sector 0) vao sector cuoi cung (Offset %ld).\n", last_sector_offset);
    fclose(file);
    return 0;
}