Hướng dẫn cách chạy demo cho trường hợp 3.
Cách 1: Tải file ổ đĩa `case3_dir_cluster_err.img` và `file fat32_recovery_case3.c` xuống máy (lưu ý để cùng 1 thư mục)
        Sau đó dùng vscode, truy cập vào `file fat32_recovery_case3.c` và nhấn Run Code (hoặc `Ctrl + Alt + N`)


Cách 2: Tự tạo 1 ổ đĩa lỗi

Các bước thực hiện như sau:

B1: Sử dụng WSL, di chuyển tới thư mục bạn muốn lưu trữ (giả sử là `cd /mnt/d/NAM_4/DataRecovery_and_Safety/Seminar
`), tạo một file trống 10MB để làm ổ đĩa ảo
    `dd if=/dev/zero of=fat32_base.img bs=1M count=10`
    
B2: Định dạng FAT32
    `mkfs.vfat -F 32 fat32_base.img`
    
B3: Tạo thư mục mount
    `sudo mkdir -p /mnt/fat32_test`
    
B4: Thực hiện việc mount file image:
    `sudo mount -o loop /mnt/d/NAM_4/DataRecovery_and_Safety/Seminar/fat32_base.img /mnt/fat32_test`
    Sau đó gỡ mount:
    `sudo umount /mnt/fat32_test`
    
B5: Tạo checksum:
    `sha256sum fat32_base.img > checksum_original.txt`
    
B6: Sử dụng công cụ OSFMount trên máy thật của bạn, thực hiện mount tệp tin .img vừa tạo, LƯU Ý: Bỏ phần chọn Read Only. Sau đó tùy ý thêm các tệp tin và thư mục vào
    Để demo tính năng này, tốt nhất bạn nên tạo ít nhất 1 thư mục và trong thư mục đó có ít nhất 1 tệp tin, không nên chỉ tạo các tệp tin lưu thẳng vào ổ đĩa.
    
B7: Quay lại màn hình WSL, sử dụng công cụ Hexedit để tiến hành "phá" các khu vực sau: 2 bảng FAT và vùng RDET.

B8: Tải tệp tin `file fat32_recovery_case3.c` xuống, để cùng thư mục với đĩa `.img` bạn vừa phá.
    Tại dòng lệnh số 193, thay thế tên tệp tin `case3_dir_cluster_err.img` bằng tên tệp tin của đĩa `.img` của bạn.
    
B9: Chạy chương trình bằng cách nhấn Run Code hoặc `Ctrl + Alt + N`
