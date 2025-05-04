#include <stdio.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>

int main() {
    int status;
    int fd;
    char buf[100];
    
    printf("=== 目錄切換功能測試 ===\n\n");
    
    // 測試 1: 創建測試目錄結構
    printf("1. 創建測試目錄結構...\n");
    status = MkDir("/testdir");
    printf("   創建 /testdir 結果: %d (應為 0 表示成功)\n", status);
    
    status = MkDir("/testdir/subdir1");
    printf("   創建 /testdir/subdir1 結果: %d\n", status);
    
    status = MkDir("/testdir/subdir2");
    printf("   創建 /testdir/subdir2 結果: %d\n", status);
    
    // 測試 2: 在各個目錄建立測試檔案
    printf("\n2. 在各個目錄建立測試檔案...\n");
    fd = Create("/testdir/file1.txt");
    if (fd >= 0) {
        Write(fd, "This is in /testdir", 19);
        Close(fd);
        printf("   在 /testdir 創建檔案成功\n");
    }
    
    fd = Create("/testdir/subdir1/file2.txt");
    if (fd >= 0) {
        Write(fd, "This is in /testdir/subdir1", 27);
        Close(fd);
        printf("   在 /testdir/subdir1 創建檔案成功\n");
    }
    
    // 測試 3: 切換到 /testdir 目錄
    printf("\n3. 測試 ChDir 到 /testdir...\n");
    status = ChDir("/testdir");
    printf("   ChDir 到 /testdir 結果: %d\n", status);
    
    // 測試 4: 用相對路徑建立檔案，驗證目前工作目錄
    printf("\n4. 用相對路徑測試目前工作目錄...\n");
    fd = Create("testfile.txt");
    if (fd >= 0) {
        Write(fd, "Testing relative path in current directory", 41);
        Close(fd);
        printf("   在當前目錄創建檔案成功\n");
        
        // 開啟剛才建立的檔案，驗證目錄確實有變更
        fd = Open("testfile.txt");
        if (fd >= 0) {
            int bytes = Read(fd, buf, 41);
            buf[bytes] = '\0';
            printf("   讀取檔案內容: %s\n", buf);
            Close(fd);
        }
    }
    
    // 測試 5: 用相對路徑切換到子目錄
    printf("\n5. 用相對路徑切換到子目錄...\n");
    status = ChDir("subdir1");
    printf("   ChDir 到 subdir1 結果: %d\n", status);
    
    // 測試 6: 驗證相對路徑在新目錄下工作正常
    printf("\n6. 驗證在 subdir1 中的相對路徑...\n");
    fd = Open("file2.txt");
    if (fd >= 0) {
        int bytes = Read(fd, buf, 27);
        buf[bytes] = '\0';
        printf("   讀取檔案內容: %s\n", buf);
        Close(fd);
    } else {
        printf("   無法開啟 file2.txt，ChDir 可能未正確運作\n");
    }
    
    // 測試 7: 切換回根目錄
    printf("\n7. 測試切換回根目錄...\n");
    status = ChDir("/");
    printf("   ChDir 到 / 結果: %d\n", status);
    
    // 測試 8: 從根目錄用絕對路徑訪問檔案
    printf("\n8. 從根目錄用絕對路徑訪問檔案...\n");
    fd = Open("/testdir/subdir1/file2.txt");
    if (fd >= 0) {
        int bytes = Read(fd, buf, 27);
        buf[bytes] = '\0';
        printf("   讀取檔案內容: %s\n", buf);
        Close(fd);
    }
    
    // 測試 9: 錯誤情況測試 - 嘗試切換到不存在的目錄
    printf("\n9. 測試錯誤情況 - 切換到不存在的目錄...\n");
    status = ChDir("/nonexistent");
    printf("   ChDir 到 /nonexistent 結果: %d (應為 ERROR=-1)\n", status);
    
    // 測試 10: 錯誤情況測試 - 嘗試切換到檔案而非目錄
    printf("\n10. 測試錯誤情況 - 切換到檔案而非目錄...\n");
    status = ChDir("/testdir/file1.txt");
    printf("   ChDir 到 /testdir/file1.txt 結果: %d (應為 ERROR=-1)\n", status);
    
    printf("\n=== 目錄切換功能測試完成 ===\n");
    
    // 清理
    status = Unlink("/testdir/subdir1/file2.txt");
    status = Unlink("/testdir/file1.txt");
    status = Unlink("/testdir/testfile.txt");
    status = RmDir("/testdir/subdir1");
    status = RmDir("/testdir/subdir2");
    status = RmDir("/testdir");
    
    return 0;
}