Đoạn code này thực hiện khôi phục file sau khi file đã bị xóa nhưng vẫn còn thông tin file system 
fat32.

Chứa 2 chương trình: 
    fat32_info.c (in thông tin cần thiết của hệ thống tập tin Fat32)
    fat32_recovery_cas4.c (thực hiện khôi phục dữ liệu)

Cách chạy chương trình:
*Ubuntu:
1 Biên dịch chương trình:
gcc -o fat32_recovery_cas4 fat32_recovery_cas4.c

2. Test với img lỗi: fat32_case4.img
./fat32_recovery_cas4 fat32_case4

Nếu thành công hệ thống sẽ tạo file được khôi phục.
