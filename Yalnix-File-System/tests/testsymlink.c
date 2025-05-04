#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int status, fd1, fd2, fd3;
    char buffer[100];
    struct Stat stat_buf;
    
    printf("=== 符號連結 (SymLink) 測試程式 ===\n\n");
    
    // 1. 建立測試檔案
    printf("1. 建立測試檔案 /srcfile...\n");
    fd1 = Create("/srcfile");
    if (fd1 == ERROR) {
        printf("錯誤: 無法建立 /srcfile\n");
        Exit(1);
    }
    
    // 寫入內容到原始檔案
    const char *test_content = "這是測試符號連結的內容";
    Write(fd1, (void *)test_content, strlen(test_content));
    printf("寫入內容到 /srcfile: \"%s\"\n", test_content);
    Close(fd1);
    
    // 2. 建立符號連結
    printf("\n2. 建立符號連結 /symlink -> /srcfile...\n");
    status = SymLink("/srcfile", "/symlink");
    if (status == ERROR) {
        printf("錯誤: 無法建立符號連結\n");
        Exit(1);
    }
    printf("符號連結建立成功 (狀態碼: %d)\n", status);
    
    // 3. 通過符號連結讀取內容
    printf("\n3. 通過符號連結開啟檔案...\n");
    fd2 = Open("/symlink");
    if (fd2 == ERROR) {
        printf("錯誤: 無法通過符號連結開啟檔案\n");
        Exit(1);
    }
    
    // 讀取內容
    memset(buffer, 0, sizeof(buffer));
    Read(fd2, buffer, sizeof(buffer));
    printf("通過符號連結讀取內容: \"%s\"\n", buffer);
    Close(fd2);
    
    // 4. 修改原始檔案內容
    printf("\n4. 修改原始檔案內容...\n");
    fd1 = Open("/srcfile");
    if (fd1 == ERROR) {
        printf("錯誤: 無法開啟原始檔案\n");
        Exit(1);
    }
    
    const char *new_content = "這是修改後的內容";
    Seek(fd1, 0, SEEK_SET);
    Write(fd1, (void *)new_content, strlen(new_content));
    Close(fd1);
    printf("原始檔案內容已修改為: \"%s\"\n", new_content);
    
    // 5. 再次通過符號連結讀取修改後的內容
    printf("\n5. 再次通過符號連結讀取內容...\n");
    fd2 = Open("/symlink");
    if (fd2 == ERROR) {
        printf("錯誤: 無法通過符號連結開啟檔案\n");
        Exit(1);
    }
    
    memset(buffer, 0, sizeof(buffer));
    Read(fd2, buffer, sizeof(buffer));
    printf("通過符號連結讀取修改後的內容: \"%s\"\n", buffer);
    Close(fd2);
    
    // 6. 刪除原始檔案，檢查符號連結行為
    printf("\n6. 刪除原始檔案後測試符號連結...\n");
    status = Unlink("/srcfile");
    if (status == ERROR) {
        printf("錯誤: 無法刪除原始檔案\n");
    } else {
        printf("原始檔案已刪除\n");
        
        // 嘗試通過符號連結開啟檔案 (預期失敗)
        fd3 = Open("/symlink");
        if (fd3 == ERROR) {
            printf("符合預期: 無法通過符號連結開啟已刪除的檔案\n");
        } else {
            printf("意外結果: 能夠通過符號連結開啟已刪除的檔案\n");
            Close(fd3);
        }
    }
    
    // 7. 使用 ReadLink 讀取符號連結目標
    printf("\n7. 讀取符號連結目標...\n");
    memset(buffer, 0, sizeof(buffer));
    status = ReadLink("/symlink", buffer, sizeof(buffer));
    if (status == ERROR) {
        printf("錯誤: 無法讀取符號連結目標\n");
    } else {
        printf("符號連結目標: \"%s\"\n", buffer);
    }
    
    // 8. 獲取符號連結的 Stat 資訊
    printf("\n8. 獲取符號連結的 Stat 資訊...\n");
    if (Stat("/symlink", &stat_buf) == ERROR) {
        printf("錯誤: 無法獲取符號連結的 Stat 資訊\n");
    } else {
        printf("符號連結資訊:\n");
        printf("  inode 號碼: %d\n", stat_buf.inum);
        printf("  類型: %d (應該是 3 表示符號連結)\n", stat_buf.type);
        printf("  大小: %d 位元組 (應為目標路徑長度)\n", stat_buf.size);
        printf("  連結數: %d\n", stat_buf.nlink);
    }
    
    // 9. 測試建立指向不存在檔案的符號連結
    printf("\n9. 建立指向不存在檔案的符號連結...\n");
    status = SymLink("/nonexistent", "/symlink2");
    if (status == ERROR) {
        printf("錯誤: 無法建立符號連結到不存在的檔案\n");
    } else {
        printf("成功建立指向不存在檔案的符號連結 (符合預期，SymLink 不檢查目標存在性)\n");
        
        // 嘗試開啟該符號連結
        fd3 = Open("/symlink2");
        if (fd3 == ERROR) {
            printf("符合預期: 無法開啟指向不存在檔案的符號連結\n");
        } else {
            printf("意外結果: 能夠開啟指向不存在檔案的符號連結\n");
            Close(fd3);
        }
    }
    
    // 10. 測試錯誤案例: 建立空的符號連結
    printf("\n10. 測試建立空的符號連結 (應該失敗)...\n");
    status = SymLink("", "/symlink_empty");
    if (status == ERROR) {
        printf("符合預期: 無法建立空的符號連結\n");
    } else {
        printf("意外結果: 能夠建立空的符號連結\n");
    }
    
    printf("\n=== 符號連結測試完成 ===\n");
    
    // 清理
    Unlink("/symlink");
    Unlink("/symlink2");
    
    return 0;
}