- Phục hồi từ Bản sao:
    Giải pháp: Ưu tiên tìm kiếm bản sao MBR (được giả định lưu ở sector cuối cùng của đĩa).
    Nếu tìm thấy bản sao hợp lệ (dựa trên signature 55 AA), sao chép 64 bytes Bảng Phân Vùng
    từ bản sao trở lại MBR (Sector 0).

- Tái tạo từ Boot Record (BR):
    Giải pháp: Quét toàn bộ đĩa (từ Sector 1) để tìm Boot Record (BR) của phân vùng FAT32.
    Từ đó, trích xuất thông tin LBA Start và Total Sectors để xây dựng lại Bảng Phân Vùng.

- Chương trình:
    fat32_recovery_case1.c (thực hiện khôi phục dữ liệu)
    create_image.sh (tạo file ảnh)
    run_scenario1.sh (chương trình chạy tự động TH. phục hồi từ bản sao)
    run_scenario2.sh (chương trình chạy tự động TH. tái tạo từ boot record)

--------------------------------------------------------
                    HƯỚNG DẪN SỬ DỤNG
--------------------------------------------------------

Bước 1: Sử dụng WSL, di chuyển đến thư mục case1_partition_err
    ```bash
    cd /mnt/<đường dẫn nơi lưu thư mục>/FAT32-Recovery-ATPHDL34/source/case1_partition_err

    # Ex:
    # cd /mnt/d/HK1_2025_2026/Gky_ATPHDL/FAT32-Recovery-ATPHDL34/source/case1_partition_err
    ```

Bước 2: Biên dịch chương trình
    ```bash
    gcc fat32_recovery_case1.c -o recovery_tool.exe
    ```
--------------------------------------------------------
Có thể bỏ qua bước 3 và bước 4 (do nhóm đã chuẩn bị sẵn)
--------------------------------------------------------

Bước 3: Tạo file ảnh và copy dữ liệu:
    Tạo file fat32_partition_err.img 50MB và copy toàn bộ nội dung từ thư mục `../../base_images/data` vào file ảnh

    ```bash
    sudo ./create_image.sh fat32_partition_err.img 50 ../../base_images/data ../../base_images
    ```

Bước 4: Copy file ảnh thành 2 file ảnh để test 2 trường hợp
    - Phục hồi từ bản sao (A): fat32_partition_errA.img
    - Tái tại từ Boot Record (B): fat32_partition_errB.img

    ```bash
    cp ../../base_images/fat32_partition_err.img ../../base_images/fat32_partition_errA.img
    cp ../../base_images/fat32_partition_err.img ../../base_images/fat32_partition_errB.img
    ```

Bước 5: Chạy chương trình
    - Trường hợp Phục hồi từ bản sao (A)
    ```bash
    sudo ./recovery_tool.exe ../../base_images/fat32_partition_errA.img
    ```

    - Trường hợp Phục hồi từ bản sao (B)
    ```bash
    sudo ./recovery_tool.exe ../../base_images/fat32_partition_errB.img
    ```
    --------------------------------------------------------
    Chọn các option:
        1. Xem 64 bytes từ 1BE đến 1FD
        2. Tạo backup
        3. Ghi dè 64 bytes từ 1BE đến 1FD bằng 0
        4. Phục hồi
        5. Mount ổ đĩa
        6. Umount ổ đĩa
        0. Thoát chương trình

    Có thể mount ổ đĩa trước để kiểm tra.
    - Phục hồi từ bản sao: 1 -> 2 -> 3 -> 4 -> 5 -> 6 (hoặc 0)
    - Tái tạo từ Boot Record: 1 -> 3 -> 4 -> 5 -> 6 (hoặc 0)
    --------------------------------------------------------

    # Mount (option 5) để xem nội dung
    Cần nhập tên thư mục để mount tới. Ví dụ: fat32_test
    Kết quả:
    --- DANH SACH FILE TRONG FOLDER: ./fat32_test ---
    total 1
    -rwxrwxrwx 1 root root 26 Nov 19 18:31 test.txt

    Để xem nội dung file, ta dùng lệnh cat:
    ```bash
    cat ./<ten_thu_muc>/<ten_file>

    # Ex: cat ./fat32_test/test.txt
    ```
    

    # Umount thủ công
    ```bash
    sudo umount <ten folder>
    rm -rf <ten folder>

    # Ex:
    # sudo umount fat32_test
    # rm -rf fat32_test
    ```

------------------------------------------
HƯỚNG DẪN SỬ DỤNG CÁCH CHẠY TỰ ĐỘNG HÓA
------------------------------------------

TƯƠNG TỰ CÁC BƯỚC 1, 2, 3, 4 phía trên


* A - Phục hồi từ Bản sao:

    - Kịch bản test: Chọn option 1 -> 2 -> 3 -> 4 -> 5 -> 0
    - Chạy:
        ```bash
        ./run_scenario1.sh ../../base_images/fat32_partition_errA.img
        ```

* B - Tái tạo từ Boot Record (BR):

    - Kịch bản test: Chọn option 1 -> 3 -> 4 -> 5 -> 0
    - Chạy:
        ```bash
        ./run_scenario2.sh ../../base_images/fat32_partition_errB.img
        ```
