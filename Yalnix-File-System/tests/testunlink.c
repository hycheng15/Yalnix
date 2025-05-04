#include <stdio.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int status, fd1, fd2;
    struct Stat stat_buf;

    printf("=== Unlink 函數測試 ===\n\n");

    // 1. 建立測試檔案
    printf("1. 建立測試檔案...\n");
    fd1 = Create("/testfile");
    if (fd1 == ERROR) {
        printf("錯誤：無法建立 /testfile\n");
        Exit(1);
    }
    printf("成功建立檔案 /testfile，檔案描述符：%d\n", fd1);

    // 寫入一些資料
    // const char *test_content = "這是測試 Unlink 的檔案內容";
    // status = Write(fd1, (void *)test_content, strlen(test_content));
    // printf("寫入 %d 位元組到 /testfile\n", status);
    
    // 關閉檔案
    // Close(fd1);
    
    // 2. 建立到測試檔案的硬連結
    printf("\n2. 建立到測試檔案的硬連結...\n");
    status = Link("/testfile", "/testlink");
    if (status == ERROR) {
        printf("錯誤：無法建立連結 /testlink\n");
    } else {
        printf("成功建立連結 /testlink\n");
    }
    
    // 3. 檢查檔案的連結數
    printf("\n3. 檢查檔案的連結數...\n");
    if (Stat("/testfile", &stat_buf) != ERROR) {
        printf("檔案 /testfile 的連結數：%d (應該是 2)\n", stat_buf.nlink);
    }
    
    // 4. 移除原始檔案
    printf("\n4. 移除原始檔案...\n");
    status = Unlink("/testfile");
    if (status == ERROR) {
        printf("錯誤：無法移除 /testfile\n");
    } else {
        printf("成功移除 /testfile\n");
    }
    
    // 5. 嘗試開啟已移除的檔案
    printf("\n5. 嘗試開啟已移除的檔案...\n");
    fd1 = Open("/testfile");
    if (fd1 == ERROR) {
        printf("符合預期：無法開啟已移除的檔案 /testfile\n");
    } else {
        printf("意外結果：能夠開啟已移除的檔案，檔案描述符：%d\n", fd1);
        Close(fd1);
    }
    
    // 6. 嘗試通過連結開啟檔案 (應該成功，因為連結仍然存在)
    printf("\n6. 嘗試通過連結開啟檔案...\n");
    fd2 = Open("/testlink");
    if (fd2 == ERROR) {
        printf("錯誤：無法通過連結 /testlink 開啟檔案\n");
    } else {
        printf("成功通過連結 /testlink 開啟檔案，檔案描述符：%d\n", fd2);
        
        // 讀取內容以確認檔案內容仍然存在
        char buffer[100];
        memset(buffer, 0, sizeof(buffer));
        status = Read(fd2, buffer, sizeof(buffer));
        printf("從 /testlink 讀取內容：\"%s\"\n", buffer);
        Close(fd2);
    }
    
    // 7. 檢查連結檔案的狀態
    printf("\n7. 檢查連結檔案的狀態...\n");
    if (Stat("/testlink", &stat_buf) != ERROR) {
        printf("/testlink 的連結數：%d (應該是 1)\n", stat_buf.nlink);
        printf("/testlink 的 inode 號碼：%d\n", stat_buf.inum);
    } else {
        printf("錯誤：無法獲取 /testlink 狀態\n");
    }
    
    // 8. 移除最後一個連結
    printf("\n8. 移除最後一個連結...\n");
    status = Unlink("/testlink");
    if (status == ERROR) {
        printf("錯誤：無法移除 /testlink\n");
    } else {
        printf("成功移除 /testlink\n");
    }
    
    // 9. 檢查檔案是否已完全移除
    printf("\n9. 確認檔案已完全移除...\n");
    fd2 = Open("/testlink");
    if (fd2 == ERROR) {
        printf("符合預期：無法開啟已移除的檔案 /testlink\n");
    } else {
        printf("意外結果：能夠開啟已移除的檔案，檔案描述符：%d\n", fd2);
        Close(fd2);
    }
    
    // 10. 測試錯誤情況：解除連結目錄
    printf("\n10. 測試解除連結目錄...\n");
    status = MkDir("/testdir");
    printf("建立目錄 /testdir 狀態：%d\n", status);
    
    status = Unlink("/testdir");
    printf("嘗試解除連結目錄結果：%d (應該返回錯誤)\n", status);
    
    // 11. 測試錯誤情況：解除連結不存在的檔案
    printf("\n11. 測試解除連結不存在的檔案...\n");
    status = Unlink("/nonexistent");
    printf("嘗試解除連結不存在檔案結果：%d (應該返回錯誤)\n", status);
    
    printf("\n=== Unlink 測試完成 ===\n");
    Shutdown();
    return 0;
}