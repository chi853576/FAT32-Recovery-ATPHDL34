#!/bin/bash

# ============================================================
# CẤU HÌNH
# ============================================================
SOURCE_FILE="fat32_recovery_case1.c"
EXE_FILE="recovery_tool.exe"
# File ảnh mặc định
IMAGE_FILE="${1:-../../base_images/fat32_partition_errB.img}"
# Tên folder sẽ nhập vào chương trình C
MOUNT_NAME="data_restore_B"

# ============================================================
# BƯỚC 1: BIÊN DỊCH
# ============================================================
echo "[SCENARIO 2] Kich ban: Tai tao tu Boot Record (Case B)"
echo "---------------------------------------------------"

if gcc "$SOURCE_FILE" -o "$EXE_FILE"; then
    echo " -> Bien dich OK."
else
    echo " -> Bien dich LOI."
    exit 1
fi

# ============================================================
# BƯỚC 2: CHẠY TOOL
# ============================================================
echo " -> Dang chay tool voi quyen ROOT..."
echo " -> Flow: 1(Xem) -> 3(Ghi de) -> 4(Phuc hoi) -> 5(Mount) -> 0(Thoat)"

# Input tự động: KHÔNG CÓ SỐ 2
sudo ./$EXE_FILE "$IMAGE_FILE" <<EOF
1
3
4
5
$MOUNT_NAME
0
EOF

echo "---------------------------------------------------"
echo "HOAN TAT SCENARIO 2."