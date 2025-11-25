========================================================
             ĐỒ ÁN PHỤC HỒI DỮ LIỆU FAT32
========================================================

1. GIỚI THIỆU GIẢI PHÁP
-----------------------
[Phục hồi từ bản sao - Case A]
    Giải pháp: Ưu tiên tìm kiếm bản sao MBR (được giả định lưu ở sector cuối cùng của đĩa).
    Nếu tìm thấy bản sao hợp lệ (dựa trên signature 55 AA), sao chép 64 bytes Bảng Phân Vùng
    từ bản sao trở lại MBR (Sector 0).

[Tái tạo từ Boot Record - Case B]
    Giải pháp: Quét toàn bộ đĩa (từ Sector 1) để tìm Boot Record (BR) của phân vùng FAT32.
    Từ đó, trích xuất thông tin LBA Start và Total Sectors để xây dựng lại bảng phân vùng.

2. DANH SÁCH FILE
-----------------
    fat32_recovery_case1.c  : Mã nguồn chính (Hỗ trợ build trên cả Windows & Linux)
    create_image.sh             : Script tạo file ảnh (Linux/WSL)
    run_scenario1.sh            : Script chạy tự động Case A (Linux/WSL)
    run_scenario2.sh            : Script chạy tự động Case B (Linux/WSL)
    fat32_partition_err.img     : file ảnh gốc
    fat32_partition_errA.img    : phục vụ cho case A (được copy từ fat32_partition_err.img)
    fat32_partition_errB.img    : phục vụ cho case B (được copy từ fat32_partition_err.img)


========================================================
        PHẦN A: HƯỚNG DẪN CHẠY TRÊN WINDOWS (VS CODE)
========================================================
Yêu cầu: 
- Đã cài đặt MinGW (GCC) và cấu hình biến môi trường.
- Công cụ OSFMount (để kiểm tra kết quả mount).

BƯỚC 1: Biên dịch chương trình
    Mở Terminal trong VS Code (Ctrl + `), đảm bảo đang ở thư mục chứa code:
    (<đường dẫn>/FAT32-Recovery-ATPHDL34/source/case1_partition_err)
    
    gcc fat32_recovery_case1.c -o recovery_tool.exe

BƯỚC 2: Chạy chương trình
    
    .\recovery_tool.exe <đường_dẫn_file_img>

    Ví dụ:
    case A: .\recovery_tool.exe ..\..\base_images\fat32_partition_errA.img 
    case B: .\recovery_tool.exe ..\..\base_images\fat32_partition_errB.img
BƯỚC 3: Thao tác gây lỗi và phục hồi
    Quy trình chuẩn:
    - Phục hồi từ bản sao (case A): 1 -> 2 -> 3 -> 4
    - Tái tạo từ Boot Record (case B): 1 -> 3 -> 4

    Chọn các option theo thứ tự logic:
    1. Xem Partition Table (để thấy nó đang bị lỗi hoặc bình thường).
    2. Tạo backup (Nếu muốn test Case A).
    3. Ghi đè/Làm hỏng (Để giả lập sự cố).
    4. CHẠY PHỤC HỒI (Tool sẽ tự thử Case A, nếu không được sẽ qua Case B).

BƯỚC 4: Kiểm tra kết quả (Mount)
    *Lưu ý:* Trên Windows, Option 5 trong tool sẽ không mount tự động được.
    
    Cách làm thủ công:
    1. Mở phần mềm OSFMount.
    2. Chọn "Mount new...".
    3. Chọn file .img vừa phục hồi.
    4. Chọn phân vùng (Partition 0) -> OK.
    5. Mở ổ đĩa ảo vừa tạo trong "This PC" để xem dữ liệu.
    6. Chọn Umount

Sau đó chọn option 0 để thoát chương trình

========================================================
        PHẦN B: HƯỚNG DẪN CHẠY TRÊN LINUX / WSL
========================================================

BƯỚC 1: Di chuyển đến thư mục source
    
    cd /mnt/<đường dẫn>/FAT32-Recovery-ATPHDL34/source/case1_partition_err

    ví dụ: cd /mnt/d/HK1_2025_2026/Gky_ATPHDL/FAT32-Recovery-ATPHDL34/source/case1_partition_err

BƯỚC 2: Biên dịch (Khuyên dùng file mới để ổn định)
    
    gcc fat32_recovery_case1.c -o recovery_tool.exe

BƯỚC 3: Chạy chương trình (Cần quyền ROOT)
    
    case A: sudo ./recovery_tool.exe ../../base_images/fat32_partition_errA.img
    case B: sudo ./recovery_tool.exe ../../base_images/fat32_partition_errB.img

BƯỚC 4: Các Option
    
    Quy trình chuẩn:
    - Phục hồi từ bản sao: 1 -> 2 -> 3 -> 4 -> 5 -> 6 (hoặc 0)
    - Tái tạo từ Boot Record: 1 -> 3 -> 4 -> 5 -> 6 (hoặc 0)

    *Lưu ý:* Trên Linux, Option 5 sẽ tự động mount vào thư mục bạn đặt tên.
    Cần nhập tên thư mục để mount tới. Ví dụ: fat32_test
    Kết quả:
    --- DANH SACH FILE TRONG FOLDER: ./fat32_test ---
    total 1
    -rwxrwxrwx 1 root root 26 Nov 19 18:31 test.txt

    # Xem file bằng lệnh:
    Crtl + C
    cat ./<ten_thu_muc>/<ten_file>
    
    ví dụ: cat ./fat32_test/test.txt

    # Umount thủ công
    sudo umount <ten folder>
    rm -rf <ten folder>

    Ví dụ:
    sudo umount fat32_test
    rm -rf fat32_test

========================================================
        PHẦN C: CHẠY TỰ ĐỘNG (CHỈ DÀNH CHO LINUX/WSL)
========================================================
Sử dụng các script .sh có sẵn để chạy demo nhanh:
Di chuyển đến thư mục chứa source

* Kịch bản A - Phục hồi từ Bản sao:
    ./run_scenario1.sh ../../base_images/fat32_partition_errA.img

* Kịch bản B - Tái tạo từ Boot Record (BR):
    ./run_scenario2.sh ../../base_images/fat32_partition_errB.img