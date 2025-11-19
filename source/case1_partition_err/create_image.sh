#!/bin/bash

# =================================================================
# CẤU HÌNH (Nhận tham số từ dòng lệnh)
# =================================================================
# Tham số 1: Tên file ảnh (Ví dụ: disk.img)
IMG_FILENAME="${1:-fat32_clean.img}"

# Tham số 2: Dung lượng MB (Mặc định: 50)
IMG_SIZE_MB="${2:-50}"

# Tham số 3: Thư mục Input (Mặc định: Rỗng - tức là không copy)
INPUT_DIR="${3:-}"

# Tham số 4: Thư mục Output (Mặc định: . - Thư mục hiện tại)
OUTPUT_DIR="${4:-.}"

# [QUAN TRỌNG] Thống nhất tên biến thư mục Mount
MOUNT_DIR="./mnt_temp_fat32"

# Xử lý đường dẫn file ảnh đầy đủ
# Loại bỏ dấu / ở cuối OUTPUT_DIR nếu có để tránh lỗi đường dẫn (vd: dir//file.img)
OUTPUT_DIR=${OUTPUT_DIR%/}
FULL_IMG_PATH="${OUTPUT_DIR}/${IMG_FILENAME}"

echo "=== CAU HINH ==="
echo "Ten File:     $IMG_FILENAME"
echo "Duong dan:    $FULL_IMG_PATH"
echo "Dung luong:   $IMG_SIZE_MB MB"

if [ -n "$INPUT_DIR" ]; then
    echo "Input Dir:    $INPUT_DIR"
else
    echo "Input Dir:    (Khong copy du lieu)"
fi
echo "Mount Point:  $MOUNT_DIR"
echo "================"

# =================================================================
# KIỂM TRA QUYỀN ROOT
# =================================================================
if [ "$EUID" -ne 0 ]; then
  echo "Loi: Script nay can chay bang quyen root (sudo)."
  exit 1
fi

# =================================================================
# BƯỚC 0: CHUẨN BỊ MÔI TRƯỜNG
# =================================================================
echo ""
echo "--- [Buoc 0] Chuan bi moi truong ---"

# Kiem tra va cai dat dosfstools
if ! command -v mkfs.vfat &> /dev/null; then
    echo " -> Dang cai dat dosfstools..."
    apt-get update && apt-get install -y dosfstools
else
    echo " -> dosfstools da duoc cai dat."
fi

# Tao thu muc Output neu chua co
if [ ! -d "$OUTPUT_DIR" ]; then
    echo " -> Tao thu muc output: $OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR"
fi

# =================================================================
# BƯỚC 1: TẠO FILE RỖNG
# =================================================================
echo ""
echo "--- [Buoc 1] Tao file anh rong ($IMG_SIZE_MB MB) ---"
# Sử dụng FULL_IMG_PATH thay vì IMG_NAME
dd if=/dev/zero of="$FULL_IMG_PATH" bs=1M count="$IMG_SIZE_MB" status=progress

# =================================================================
# BƯỚC 2: TẠO BẢNG PHÂN VÙNG MBR
# =================================================================
echo ""
echo "--- [Buoc 2] Tao MBR Partition Table ---"
# Sử dụng sed để loại bỏ khoảng trắng thừa trong here-doc
sed -e 's/\s*\([\+0-9a-zA-Z]*\).*/\1/' << EOF | fdisk "$FULL_IMG_PATH"
  o # Tao MBR moi
  n # New partition
  p # Primary
  1 # Partition 1
    # Default start
    # Default end
  t # Change type
  c # W95 FAT32 (LBA)
  w # Write
EOF

# =================================================================
# BƯỚC 3: FORMAT FAT32
# =================================================================
echo ""
echo "--- [Buoc 3] Gan Loop Device va Format FAT32 ---"

# Tìm và gắn loop device vào FULL_IMG_PATH
LOOP_DEV=$(losetup -fP --show "$FULL_IMG_PATH")

if [ -z "$LOOP_DEV" ]; then
    echo "Loi: Khong the gan thiet bi loop."
    exit 1
fi

echo " -> Da gan file anh vao thiet bi: $LOOP_DEV"
TARGET_PART="${LOOP_DEV}p1"
echo " -> Phan vung dich la: $TARGET_PART"

sleep 1

# Format
mkfs.vfat -F 32 -n "MY_DATA" "$TARGET_PART"

# =================================================================
# BƯỚC 4: MOUNT VÀ COPY DỮ LIỆU
# =================================================================
echo ""
echo "--- [Buoc 4] Xu ly du lieu ---"

if [ -n "$INPUT_DIR" ]; then
    echo " -> Phat hien tham so Input Dir: $INPUT_DIR"
    
    if [ -z "$MOUNT_DIR" ]; then MOUNT_DIR="./mnt_temp_fat32"; fi

    echo " -> Tao thu muc mount: $MOUNT_DIR"
    mkdir -p "$MOUNT_DIR"

    echo " -> Dang mount $TARGET_PART vao $MOUNT_DIR"
    mount "$TARGET_PART" "$MOUNT_DIR"

    if [ -d "$INPUT_DIR" ]; then
        if [ "$(ls -A $INPUT_DIR)" ]; then
            echo " -> Dang copy file..."
            cp -r "$INPUT_DIR"/* "$MOUNT_DIR"/
            echo " -> Da copy xong. Danh sach file trong img:"
            ls -l "$MOUNT_DIR"
        else
            echo " -> Canh bao: Thu muc Input rong."
        fi
    else
        echo " -> Loi: Thu muc Input '$INPUT_DIR' khong ton tai."
    fi

    echo " -> Dang unmount..."
    umount "$MOUNT_DIR"
    rmdir "$MOUNT_DIR"
    echo " -> Da unmount thanh cong."
else
    echo " -> Khong co Input Dir, bo qua buoc copy."
fi

# =================================================================
# BƯỚC 5: NGẮT KẾT NỐI LOOP
# =================================================================
echo ""
echo "--- [Buoc 5] Ngat ket noi Loop Device ---"
losetup -d "$LOOP_DEV"
echo " -> Da ngat ket noi $LOOP_DEV."

echo ""
echo "=== HOAN TAT! FILE NAM TAI: '$FULL_IMG_PATH' ==="