# FAT32-Recovery-ATPHDL34
Data recovery for FAT32 logical damage scenarios

Chương trình triển khai: ngôn ngữ C

Cấu trúc đồ án:

FAT32-Recovery-ATPHDL34/
├── .gitignore
├── README.md
├── base_images/
│   ├── fat32_base.img
│   └── checksum_original.txt
└── source/ 
    ├── case1_partition_err/
    ├── case2_volume_err/
    ├── case3_dir_cluster_err/
    └── case4_deleted_file/

base-images: 
    fat32_base.img: file img mẫu ban đâu, dữ liệu là 1 file sample_test.txt nội dung "Hello FAT32 recovery test"
    checksum_original.txt: dữ liệu ban đầu được hash, phần này có thể dùng để kiểm tra kết quả phục hồi.
source: 4 folder cho 4 tính năng.

=====================
Bắt đầu làm việc:
Tạo branch mới cho tính năng của bạn.
Chỉ Commit và push trên branch này.
Tạo pull requests
Vd:
Tạo branch: git checkout -b feature/case1-delete-file
commit, push...
Tạo pull requests: Trên GitHub website vào repo → tab Pull requests → New pull request → chọn base main, compare feature/case1-delete-file → Create Pull Request.

=====================

Kế hoạch kiểm thử:
Cách 1: Tự tạo file image Fat32, mô phỏng trường hợp lỗi và phục hồi.

Cách 2: Thực hiện mô phỏng lỗi trên image có sẵn (Fat32_base.img)

    -Tạo 1 bản copy cho fat32_base.img để tránh thao tác trên img ban đầu. 
    * Nếu sử dụng linux: copy bằng lệnh sau:
    cp fat32_base.img tên_fle.img

    -Thao tác trên file img vừa copy được để mô phỏng lỗi.

    -Viết chương trình C thực hiện khôi phục dữ liệu.

    -Kiểm tra file được khôi phục với file checksum mẫu.

    -Mô tả kịch bản cụ thể.

=======================
Lưu ý: để xem bên trong img có những file nào

-Windows không hỗ trợ mount file img fat32 mẫu, có thể dùng OSFMount để mount thử và xem dữ liệu bên trong. Link dowload: https://www.osforensics.com/tools/mount-disk-images.html

-Trên linux chỉ cần chạy các lệnh sau:
sudo mkdir /mnt/fat32_test

sudo mount -o loop fat32_base.img /mnt/fat32_test

ls -l /mnt/fat32_test 

cat /mnt/fat32_test/sample_test.txt
 => Kết quả đúng: Hello FAT32 recovery test

sudo umount /mnt/fat32_test
