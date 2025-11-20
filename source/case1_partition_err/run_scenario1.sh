#!/bin/bash

# ============================================================
# CẤU HÌNH
# ============================================================
SOURCE_FILE="fat32_recovery_case1.c"
EXE_FILE="recovery_tool.exe"
# File ảnh mặc định (nếu không truyền tham số thì dùng file này)
IMAGE_FILE="${1:-../../base_images/fat32_partition_errA.img}"
# Tên folder sẽ nhập vào chương trình C
MOUNT_NAME="data_restore_A"

# ============================================================
# BƯỚC 1: BIÊN DỊCH
# ============================================================
echo "[SCENARIO 1] Kich ban: Phuc hoi tu Ban sao (Case A)"
echo "---------------------------------------------------"

if gcc "$SOURCE_FILE" -o "$EXE_FILE"; then
    echo " -> Bien dich OK."
else
    echo " -> Bien dich LOI. Kiem tra lai file $SOURCE_FILE"
    exit 1
fi

# ============================================================
# BƯỚC 2: CHẠY TOOL
# ============================================================
echo " -> Dang chay tool voi quyen ROOT..."
echo " -> Flow: 1(Xem) -> 2(Backup) -> 3(Ghi de) -> 4(Phuc hoi) -> 5(Mount) -> 0(Thoat)"

# Input tự động cho chương trình C
sudo ./$EXE_FILE "$IMAGE_FILE" <<EOF
1
2
3
4
5
$MOUNT_NAME
0
EOF

echo "---------------------------------------------------"
echo "HOAN TAT SCENARIO 1."