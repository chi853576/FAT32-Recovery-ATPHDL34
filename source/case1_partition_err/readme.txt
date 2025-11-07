Case 1: Mô tả sai về Phân vùng (Partition Error)

Chứa 2 chương trình:
    - file create_backup.c để tạo một bản sao MBR ở cuối file ảnh
    - Chương trình fat32_recovery_case1.c đã triển khai cả hai giải pháp được mô tả trong tài liệu: Tái tạo từ Boot Record (BR) và Phục hồi từ Bản sao

Vấn đề: Bảng Phân Vùng (Partition Table) trong MBR (Sector 0, offset 1BE) bị hỏng hoặc bị xóa,
khiến hệ điều hành không thể nhận diện được phân vùng.

Chương trình fat32_recovery_case1.c đã triển khai cả hai giải pháp được mô tả trong tài liệu:

* A - Tái tạo từ Boot Record (BR):

    - Giải pháp: Quét toàn bộ đĩa (từ Sector 1) để tìm Boot Record (BR) của phân vùng FAT32.
    Từ đó, trích xuất thông tin LBA Start và Total Sectors để xây dựng lại Bảng Phân Vùng.

    - Kịch bản test: case1_corrupted.img (MBR bị xóa, không có bản sao).

* B - Phục hồi từ Bản sao:

    - Giải pháp: Ưu tiên tìm kiếm bản sao MBR (được giả định lưu ở sector cuối cùng của đĩa).
    Nếu tìm thấy bản sao hợp lệ (dựa trên signature 55 AA), sao chép 64 bytes Bảng Phân Vùng
    từ bản sao trở lại MBR (Sector 0).

    - Kịch bản test: case1_corrupted_B.img (MBR bị xóa, nhưng có bản sao ở sector cuối).

----------------------------------------------------
Nguyên tắc quan trọng trong phục hồi dữ liệu: Luôn ưu tiên khôi phục từ bản sao (backup)
trước khi cố gắng tái tạo (rebuild).

    - Case B (Phục hồi từ Bản sao): Đây là phương pháp an toàn và chính xác nhất. 
    Lấy lại chính xác Bảng Phân Vùng gốc đã được sao lưu.

    - Case A (Tái tạo từ BR): Đây là phương pháp "dự đoán" hoặc "xây dựng lại".
    Nó quét đĩa để tìm Boot Record (BR) và tái tạo Bảng Phân Vùng dựa trên thông tin tìm được
    (LBA Start, Total Sectors). Đây là giải pháp cuối cùng khi không còn bản sao.

----------------------------------------------------
----------------------------------------------------
Đây là các bước để biên dịch và chạy lại kịch bản Case 1 (A và B).

* Yêu cầu Công cụ
    - Trình biên dịch C: GCC (MinGW-w64).
    - Trình tạo ảnh đĩa: OSFMount (dùng để tạo file ảnh và kiểm tra kết quả).
    - Trình chỉnh sửa Hex: Bất kỳ Hex Editor nào (ví dụ: HxD) để mô phỏng lỗi.


Bước 1: Tạo File Ảnh Đĩa Đầy Đủ (Full Disk Image)
    1. Mở OSFMount.
    2. Chọn "Mount new...".
    3. Chọn "Empty RAM drive".
    4. Nhập Drive size (Kích thước): Tối thiểu 260 MB (do OSFMount yêu cầu để format FAT32).
    5. Tích chọn "Initialize Partition Table" và chọn "MBR".
    6. Tích chọn "Format drive" và chọn File System là "FAT32".
    7. Ở Drive emulation chọn Physical Disk Emulation.
    8. Nhấp "Mount" và hoàn tất.
    9. Mở ổ đĩa ảo mới (ví dụ: E:), tạo một file sample_test.txt với nội dung "Hello FAT32 recovery test".
    10. Trong OSFMount, chọn ổ đĩa ảo và nhấp "Save to Image File..."
    11. Lưu file vào base_images/ với tên case1_full_disk_image.img
    12. Chọn Dismount all & Exit
    13. Copy case1_full_disk_image.img và đổi tên thành case1_full_disk.img. (để test được cả 2 trường hợp)

----------------------------------------------------
------- VỚI A - Tái tạo từ Boot Record (BR) --------
----------------------------------------------------
Bước 2: Chuẩn bị kịch bản lỗi
    1. Tạo file hỏng (Case A)
        - Vào thư mục base_images/.
        - Copy case1_full_disk.img và đổi tên thành case1_corrupted.img.
        - Mở case1_corrupted.img bằng Hex Editor.
        - Đi đến offset 1BE.
        - Ghi đè 64 bytes (từ 1BE đến 1FD) bằng giá trị 00.
        - Lưu file.

Bước 3: Biên dịch và Khôi phục
    1. Trong terminal (vẫn ở source/case1_partition_err/), biên dịch chương trình khôi phục chính:
        gcc fat32_recovery_case1.c -o recover_case1 -Wl,-subsystem,console
    2. Chạy chương trình trên file đã bị làm hỏng:
        recover_case1 ../../base_images/case1_corrupted.img

Bước 4: Kiểm tra kết quả
    1. Mở OSFMount.
    2. Mount file case1_corrupted.img.
    3. Chọn Select all
    4. Ở Drive emulation chọn Physical Disk Emulation.
    5. Nếu khôi phục thành công, OSFMount sẽ mount được phân vùng.
    6. Mở ổ đĩa ảo và kiểm tra file sample_test.txt còn nguyên vẹn.

----------------------------------------------------
------------- B - Phục hồi từ Bản sao --------------
----------------------------------------------------
Bước 2: Chuẩn bị kịch bản lỗi 
    1. Mở Terminal (PowerShell/CMD) và di chuyển đến source/case1_partition_err/
    2. Tạo bản sao MBR: Biên dịch và chạy file create_backup.c để tạo một bản sao MBR ở cuối file ảnh
    (chuẩn bị cho Case B).
        Biên dịch: gcc create_backup.c -o create_backup -Wl,-subsystem,console
        Chạy     : create_backup ../../base_images/case1_full_disk.img
    3. Tạo file hỏng (Case B):
        - Vào thư mục base_images/.
        - Copy case1_full_disk.img và đổi tên thành case1_corrupted_B.img.
        - Mở case1_corrupted_B.img bằng Hex Editor.
        - Đi đến offset 1BE.
        - Ghi đè 64 bytes (từ 1BE đến 1FD) bằng giá trị 00.
        - Lưu file.

Bước 3: Biên dịch và Khôi phục
    1. Trong terminal (vẫn ở source/case1_partition_err/), biên dịch chương trình khôi phục chính:
        gcc fat32_recovery_case1.c -o recover_case1 -Wl,-subsystem,console
    2. Chạy chương trình trên file đã bị làm hỏng:
        recover_case1 ../../base_images/case1_corrupted_B.img

Bước 4: Kiểm tra kết quả
    1. Mở OSFMount.
    2. Mount file case1_corrupted_B.img.
    3. Chọn Select all
    4. Ở Drive emulation chọn Physical Disk Emulation.
    5. Nếu khôi phục thành công, OSFMount sẽ mount được phân vùng.
    6. Mở ổ đĩa ảo và kiểm tra file sample_test.txt còn nguyên vẹn.