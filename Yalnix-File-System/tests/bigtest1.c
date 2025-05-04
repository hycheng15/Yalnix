#include <stdio.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <string.h>
#include <comp421/filesystem.h>

#define MAX_PATH MAXPATHNAMELEN

void test_path_edge_cases() {
    int result;
    
    printf("=== 路徑邊界情況測試 ===\n\n");
    
    // 測試 1: 測試非常長的路徑名
    char long_path[MAX_PATH + 1];
    memset(long_path, 'a', MAX_PATH + 1);
    long_path[0] = '/';
    long_path[MAX_PATH] = '\0';
    
    printf("1. 測試超長路徑: %s\n", long_path);
    result = MkDir(long_path);
    printf("   結果: %d (應為 ERROR)\n", result);
    
    // 測試 2: 測試空路徑
    printf("\n2. 測試空路徑\n");
    result = MkDir("");
    printf("   結果: %d (應為 ERROR)\n", result);
    
    // 測試 3: 測試NULL路徑
    printf("\n3. 測試NULL路徑\n");
    result = MkDir(NULL);
    printf("   結果: %d (應為 ERROR)\n", result);
    
    // 測試 4: 測試多斜線路徑
    printf("\n4. 測試連續多斜線路徑\n");
    result = MkDir("/test");
    printf("   結果: %d\n", result);
    char *multi_slash_path = "/////test//////dir";
    printf("   測試路徑: %s\n", multi_slash_path);
    result = MkDir(multi_slash_path);
    printf("   結果: %d\n", result);
    result = RmDir("/////test//////dir");
    printf("   刪除結果: %d\n", result);
    
    // 測試 5: 測試特殊字符路徑
    printf("\n5. 測試特殊字符路徑\n");
    result = MkDir("/test.dir");
    printf("   結果: %d\n", result);
    result = MkDir("/test-dir");
    printf("   結果: %d\n", result);
    result = MkDir("/test_dir");
    printf("   結果: %d\n", result);
    
    // 清理
    printf("\n6. 清理測試目錄\n");
    printf("   測試rmdir路徑: /test.dir, /test-dir, /test_dir\n");
    result = RmDir("/test.dir");
    printf("   刪除結果: %d\n", result);
    result = RmDir("/test-dir");
    printf("   刪除結果: %d\n", result);
    result = RmDir("/test_dir");
    printf("   刪除結果: %d\n", result);

    // 測試 6: 測試刪除特殊字符檔案
    printf("    測試unlink特殊字符檔案\n");
    result = Unlink("/test.dir");
    printf("   刪除檔案結果: %d\n", result);
    result = Unlink("/test-dir");
    printf("   刪除檔案結果: %d\n", result);
    result = Unlink("/test_dir");
    printf("   刪除檔案結果: %d\n", result);

}

void test_nested_directories() {
    int result, i;
    char path[MAX_PATH];
    
    printf("\n=== 深度嵌套目錄測試 ===\n\n");
    
    // 測試 1: 建立非常深的目錄結構
    strcpy(path, "/deep");
    
    printf("1. 建立深度嵌套目錄\n");
    result = MkDir(path);
    printf("   建立 %s: %d\n", path, result);
    
    for (i = 1; i <= 10; i++) {
        sprintf(path, "/deep");
        int j;
        for (j = 1; j <= i; j++) {
            char level[10];
            sprintf(level, "/level%d", j);
            strcat(path, level);
        }
        
        result = MkDir(path);
        printf("   建立 %s: %d\n", path, result);
        
        // 測試在深層目錄中ChDir
        if (i == 10) {
            printf("\n2. 切換到最深層目錄\n");
            result = ChDir(path);
            printf("   ChDir 結果: %d\n", result);
            
            // 從深層目錄使用相對路徑
            int fd = Create("deep_file.txt");
            printf("   在深層目錄創建檔案結果: %d\n", fd);
            if (fd >= 0) Close(fd);
            
            // 返回根目錄
            result = ChDir("/");
            printf("   返回根目錄結果: %d\n", result);
        }
    }
    
    // 清理時會測試深層目錄移除
    printf("\n3. 刪除深層目錄結構\n");
    for (i = 10; i >= 1; i--) {
        sprintf(path, "/deep");
        int j;
        for (j = 1; j <= i; j++) {
            char level[10];
            sprintf(level, "/level%d", j);
            if (strlen(path) + strlen(level) < MAX_PATH - 1) {
                strcat(path, level);
            } else {
                printf("   路徑太長，無法繼續: %s\n", path);
                break;
            }
        }
        
        if (i == 10) {
            char file_path[MAX_PATH];
            // 檢查連接後的字符串不會溢出
            if (strlen(path) + 15 < MAX_PATH) { // 15 是 "/deep_file.txt" 的長度加 1
                strcpy(file_path, path);
                strcat(file_path, "/deep_file.txt");
                result = Unlink(file_path);
                printf("   刪除 %s: %d\n", file_path, result);
            } else {
                printf("   文件路徑太長，跳過刪除操作\n");
            }
        }
        
        result = RmDir(path);
        printf("   刪除 %s: %d\n", path, result);
    }
    
    result = RmDir("/deep");
    printf("   刪除 /deep: %d\n", result);
}

void test_dot_and_dotdot() {
    int result;
    int fd;
    char buf[100];
    
    printf("\n=== 測試 '.' 和 '..' 路徑 ===\n\n");
    
    // 設置測試目錄結構
    MkDir("/dot_test");
    MkDir("/dot_test/subdir");
    
    printf("1. 使用 '.' 路徑\n");
    
    // 創建相對路徑檔案
    result = ChDir("/dot_test");
    printf("   切換到 /dot_test 結果: %d\n", result);
    
    fd = Create("./file1.txt");
    if (fd >= 0) {
        Write(fd, "Dot path test", 13);
        Close(fd);
    }
    printf("   創建 ./file1.txt 結果: %d\n", fd);
    
    // 讀取使用 . 路徑
    fd = Open("./file1.txt");
    if (fd >= 0) {
        int bytes = Read(fd, buf, 13);
        buf[bytes] = '\0';
        printf("   讀取 ./file1.txt: %s\n", buf);
        Close(fd);
    }
    
    printf("\n2. 使用 '..' 路徑\n");
    result = ChDir("/dot_test/subdir");
    printf("   切換到 /dot_test/subdir 結果: %d\n", result);
    
    // 使用 .. 來訪問上層目錄檔案
    fd = Open("../file1.txt");
    if (fd >= 0) {
        int bytes = Read(fd, buf, 13);
        buf[bytes] = '\0';
        printf("   讀取 ../file1.txt: %s\n", buf);
        Close(fd);
    } else {
        printf("   無法開啟 ../file1.txt: %d\n", fd);
    }
    
    // 嘗試使用 ../.. 訪問更上層
    fd = Open("../../dot_test/file1.txt");
    printf("   開啟 ../../dot_test/file1.txt 結果: %d\n", fd);

    fd = Open("../../../../../dot_test/file1.txt");
    printf("   開啟 ../../../../../dot_test/file1.txt 結果: %d (應為 0, tbd?)\n", fd);

    fd = Open("../../../../../dot_test/file1.txt");
    printf("   開啟 ../../../../../dot_test/file1.txt 結果: %d (應為 0, tbd?)\n", fd);

    fd = Open("../../../../../dot_test/file1.txt");
    printf("   開啟 ../../../../../dot_test/file1.txt 結果: %d (應為 0, tbd?)\n", fd);

    // if (fd >= 0) Close(fd);
    fd = 0;
    while(fd < MAX_OPEN_FILES) {
        Close(fd);
        fd++;
    }
    fd = 0;
    
    // 嘗試在 . 或 .. 上執行操作
    printf("\n3. 對 '.' 和 '..' 進行操作\n");
    
    // 嘗試創建名為 . 的目錄
    result = MkDir(".");
    printf("   創建名為 '.' 的目錄結果: %d (應為 ERROR)\n", result);
    
    // 嘗試刪除 . 目錄
    result = RmDir(".");
    printf("   刪除 '.' 目錄結果: %d (應為 ERROR)\n", result);
    
    // 嘗試刪除 .. 目錄
    result = RmDir("..");
    printf("   刪除 '..' 目錄結果: %d (應為 ERROR)\n", result);
    
    // 嘗試對 . 創建硬連結
    result = Link("../file1.txt", ".");
    printf("   創建硬連結到 '.' 結果: %d (應為 ERROR)\n", result);
    
    // 嘗試重複切換目錄來測試路徑解析
    printf("\n4. 複雜的 '.' 和 '..' 組合路徑\n");
    result = ChDir("/dot_test/./subdir/.././subdir/.");
    printf("   切換到 /dot_test/./subdir/.././subdir/. 結果: %d\n", result);
    
    result = ChDir("/");
    
    // 清理
    result = Unlink("/dot_test/file1.txt");
    printf("   刪除檔案 /dot_test/file1.txt 結果: %d\n", result);
    result = RmDir("/dot_test/subdir");
    printf("   刪除目錄 /dot_test/subdir 結果: %d\n", result);
    result = RmDir("/dot_test");
    printf("   刪除目錄 /dot_test 結果: %d\n", result);
}

void test_file_operations() {
    int fd1, fd2, fd3;
    int result;
    char buf[1024];
    
    printf("\n=== 檔案操作邊界情況測試 ===\n\n");
    
    // 建立測試目錄
    MkDir("/file_test");
    
    printf("1. 多次開啟同一個檔案\n");
    fd1 = Create("/file_test/multi.txt");
    Write(fd1, "Test data", 9);
    
    fd2 = Open("/file_test/multi.txt");
    fd3 = Open("/file_test/multi.txt");
    
    printf("   fd1=%d, fd2=%d, fd3=%d\n", fd1, fd2, fd3);
    
    // 從不同的檔案描述符讀寫同一個檔案
    Write(fd2, " more data", 10);
    
    int bytes = Read(fd3, buf, 100);
    buf[bytes] = '\0';
    printf("   從fd3讀取: '%s'\n", buf);
    
    // 嘗試關閉並再次關閉
    printf("\n2. 重複關閉檔案\n");
    result = Close(fd1);
    printf("   首次關閉fd1結果: %d\n", result);
    result = Close(fd1);
    printf("   再次關閉fd1結果: %d (應為 ERROR)\n", result);
    
    Close(fd2);
    Close(fd3);
    
    // --------------------
    // 測試 Seek 邊界情況
    printf("\n3. Seek 邊界情況\n");
    fd1 = Open("/file_test/multi.txt");
    
    // 合法的 seek
    result = Seek(fd1, 5, SEEK_SET);
    bytes = Read(fd1, buf, 10);
    buf[bytes] = '\0';
    printf("   Seek(5, SEEK_SET) 後讀取: '%s'\n", buf);
    
    // 不合法的 seek (負數偏移)
    result = Seek(fd1, -10, SEEK_SET);
    printf("   Seek(-10, SEEK_SET) 結果: %d (應為 ERROR)\n", result);

    bytes = Read(fd1, buf, 1);
    buf[bytes] = '\0';
    printf("   讀取負數偏移後的bytes: '%s'\n", buf);
    
    // seek 到文件尾部之後
    result = Seek(fd1, 100, SEEK_SET);
    printf("   Seek(100, SEEK_SET) 結果: %d\n", result);
    
    bytes = Read(fd1, buf, 10);
    printf("   讀取超出檔案尾部的bytes: %d (應為 0)\n", bytes);
    
    // 不合法的 whence
    result = Seek(fd1, 0, 9999);
    printf("   Seek(0, 9999) 結果: %d (應為 ERROR)\n", result);
    
    Close(fd1);
    
    // 清理
    Unlink("/file_test/multi.txt");
    RmDir("/file_test");
}

// TODO: could add dangling symlink, then create that symlink
void test_symbolic_links() {
    int result, fd;
    char buf[100];
    
    printf("\n=== 符號連結測試 ===\n\n");

    
    
    MkDir("/symlink_test");
    result = SymLink("/symlink_test/target.txt", "/symlink_test/loop1");
    printf("  creating loop1 result: %d\n", result);
    
    fd = Open("/symlink_test/dangling_link");
    printf("   open danglingining: %d (-1)\n", fd);

    
    result = Create("/symlink_test/target.txt");
    printf("   creating from symlink: %d\n", result);
    fd = Open("/symlink_test/loop1");
    printf("   open from symlink after create from symlink: %d (1)\n", fd);






    fd = Create("/symlink_test/target.txt");
    Write(fd, "Target file content", 19);
    Close(fd);
    
    printf("1. 基本符號連結\n");
    
    // 創建基本符號連結
    result = SymLink("/symlink_test/target.txt", "/symlink_test/link1");
    printf("   創建符號連結結果: %d\n", result);
    
    // 通過連結訪問檔案
    fd = Open("/symlink_test/link1");
    printf("   開啟連結檔案結果: %d\n", fd);
    int bytes = Read(fd, buf, 100);
    if (bytes > 0) {
        buf[bytes] = '\0';
        printf("   通過連結讀取檔案: '%s'\n", buf);
    }
    Close(fd);
    
    // 測試 ReadLink
    printf("\n2. 測試 ReadLink\n");
    bytes = ReadLink("/symlink_test/link1", buf, 100);
    if (bytes > 0) {
        buf[bytes] = '\0';
        printf("   ReadLink 結果: '%s'\n", buf);
    }
    
    // 循環符號連結
    printf("\n3. 循環符號連結\n");
    result = SymLink("/symlink_test/loop3", "/symlink_test/loop2");
    printf("   創建 loop1->loop2 結果: %d\n", result);
    
    result = SymLink("/symlink_test/loop2", "/symlink_test/loop3");
    printf("   創建 loop2->loop1 結果: %d\n", result);
    
    // 嘗試訪問循環連結
    fd = Open("/symlink_test/loop2");
    printf("   開啟循環連結結果: %d (應為 ERROR)\n", fd);
    
    // 連續多層符號連結
    printf("\n4. 多層符號連結\n");
    SymLink("/symlink_test/target.txt", "/symlink_test/link_level1");
    SymLink("/symlink_test/link_level1", "/symlink_test/link_level2");
    SymLink("/symlink_test/link_level2", "/symlink_test/link_level3");
    
    fd = Open("/symlink_test/link_level3");
    if (fd >= 0) {
        bytes = Read(fd, buf, 100);
        buf[bytes] = '\0';
        printf("   通過3層連結讀取檔案: '%s'\n", buf);
        Close(fd);
    } else {
        printf("   無法通過多層連結開啟檔案: %d\n", fd);
    }
    
    // 清理
    Unlink("/symlink_test/link1");
    Unlink("/symlink_test/loop1");
    Unlink("/symlink_test/loop2");
    Unlink("/symlink_test/loop3");
    Unlink("/symlink_test/link_level1");
    Unlink("/symlink_test/link_level2");
    Unlink("/symlink_test/link_level3");
    Unlink("/symlink_test/target.txt");
    RmDir("/symlink_test");
}

void test_concurrent_operations() {
    int i, status;
    char path[MAX_PATH];
    
    printf("\n=== 大量文件操作測試 ===\n\n");
    
    printf("1. 創建大量文件和目錄\n");
    
    // 創建10個目錄
    for (i = 0; i < 10; i++) {
        sprintf(path, "/mass_dir_%d", i);
        status = MkDir(path);
        printf("   創建目錄 %s: %s (返回代碼: %d)\n", path, 
               (status == 0) ? "成功" : "失敗", status);
    }
    
    // 在每個目錄中創建5個檔案
    int fd;
    for (i = 0; i < 10; i++) {
        int j;
        printf("\n   開始在目錄 /mass_dir_%d 中創建檔案...\n", i);
        for (j = 0; j < 5; j++) {
            sprintf(path, "/mass_dir_%d/file_%d.txt", i, j);
            fd = Create(path);
            if (fd >= 0) {
                int bytes = Write(fd, "Mass test content", 17);
                printf("   創建檔案 %s: 成功 (fd: %d, 寫入字節: %d)\n", path, fd, bytes);
                Close(fd);
            } else {
                printf("   創建檔案 %s: 失敗 (返回代碼: %d)\n", path, fd);
            }
        }
        printf("   目錄 /mass_dir_%d 中成功創建了5個檔案\n", i);
    }
    
    printf("\n2. 刪除部分文件和目錄\n");
    
    // 刪除文件 (前5個目錄中的所有檔案)
    for (i = 0; i < 5; i++) {
        int j;
        printf("\n   開始刪除目錄 /mass_dir_%d 中的檔案...\n", i);
        for (j = 0; j < 5; j++) {
            sprintf(path, "/mass_dir_%d/file_%d.txt", i, j);
            status = Unlink(path);
            printf("   刪除檔案 %s: %s (返回代碼: %d)\n", path, 
                   (status == 0) ? "成功" : "失敗", status);
        }
        printf("   已從目錄 /mass_dir_%d 成功刪除5個檔案\n", i);
    }
    
    // 嘗試讀取剛剛刪除的檔案
    printf("\n3. 嘗試訪問已刪除的檔案\n");
    sprintf(path, "/mass_dir_0/file_0.txt");
    fd = Open(path);
    printf("   嘗試開啟已刪除的檔案 %s: %s (返回代碼: %d)\n", path, 
           (fd >= 0) ? "成功 (異常)" : "失敗 (預期)", fd);
    
    // 嘗試讀取未刪除的檔案
    printf("\n4. 嘗試訪問未刪除的檔案\n");
    sprintf(path, "/mass_dir_5/file_0.txt");
    fd = Open(path);
    if (fd >= 0) {
        char buf[20];
        int bytes = Read(fd, buf, 17);
        buf[bytes] = '\0';
        printf("   開啟未刪除的檔案 %s: 成功 (fd: %d)\n", path, fd);
        printf("   讀取內容: '%s' (字節數: %d)\n", buf, bytes);
        Close(fd);
    } else {
        printf("   開啟未刪除的檔案 %s: 失敗 (返回代碼: %d)\n", path, fd);
    }
    
    // 刪除目錄 (嘗試刪除20個目錄，包括不存在的)
    printf("\n5. 刪除目錄\n");
    for (i = 0; i < 20; i++) {
        sprintf(path, "/mass_dir_%d", i);
        status = RmDir(path);
        if (i < 5) {
            // 前5個目錄應該可以直接刪除 (已清空)
            printf("   刪除空目錄 %s: %s (返回代碼: %d)\n", path, 
                   (status == 0) ? "成功" : "失敗", status);
        } else if (i < 10) {
            // 目錄5-9包含檔案，應該刪除失敗
            printf("   刪除非空目錄 %s: %s (返回代碼: %d)\n", path, 
                   (status != 0) ? "失敗 (預期)" : "成功 (異常)", status);
        } else {
            // 目錄10-19不存在
            printf("   刪除不存在的目錄 %s: %s (返回代碼: %d)\n", path, 
                   (status != 0) ? "失敗 (預期)" : "成功 (異常)", status);
        }
    }
    
    // 清理剩餘檔案 (目錄5-9中的檔案)
    printf("\n6. 清理剩餘檔案以便完全刪除目錄\n");
    for (i = 5; i < 10; i++) {
        int j;
        printf("   清理目錄 /mass_dir_%d 中的檔案...\n", i);
        for (j = 0; j < 5; j++) {
            sprintf(path, "/mass_dir_%d/file_%d.txt", i, j);
            status = Unlink(path);
            printf("   刪除檔案 %s: %s (返回代碼: %d)\n", path, 
                   (status == 0) ? "成功" : "失敗", status);
        }
        
        // 現在目錄應該為空，可以刪除
        sprintf(path, "/mass_dir_%d", i);
        status = RmDir(path);
        printf("   刪除空目錄 %s: %s (返回代碼: %d)\n", path, 
               (status == 0) ? "成功" : "失敗", status);
    }
    
    printf("\n=== 大量文件操作測試完成 ===\n");
}

void test_edge_cases() {
    int fd1, fd2, result;
    char buf[100];
    
    printf("\n=== 其他邊界情況測試 ===\n\n");
    
    printf("1. 嘗試移除根目錄\n");
    result = RmDir("/");
    printf("   結果: %d (應為 ERROR)\n", result);
    
    printf("\n2. 嘗試移除不存在的目錄\n");
    result = RmDir("/nonexistent_dir");
    printf("   結果: %d (應為 ERROR)\n", result);
    
    printf("\n3. 嘗試移除非空目錄\n");
    MkDir("/nonempty");
    fd1 = Create("/nonempty/file.txt");
    Close(fd1);
    
    result = RmDir("/nonempty");
    printf("   結果: %d (應為 ERROR)\n", result);
    
    Unlink("/nonempty/file.txt");
    RmDir("/nonempty");
    
    printf("\n4. 超出檔案描述符上限測試\n");
    MkDir("/fd_test");
    
    int fds[MAX_OPEN_FILES + 5];
    int successful_opens = 0;
    
    for (int i = 0; i < MAX_OPEN_FILES + 5; i++) {
        char filename[32];
        sprintf(filename, "/fd_test/file%d.txt", i);
        
        fd1 = Create(filename);
        if (fd1 >= 0) {
            fds[successful_opens++] = fd1;
            if (i % 5 == 0) {
                printf("   打開第 %d 個檔案，fd = %d\n", i, fd1);
            }
        } else {
            printf("   無法打開更多檔案，達到上限: %d 檔案\n", i);
            break;
        }
    }
    
    // 關閉所有已開啟的檔案
    printf("   關閉所有 %d 個開啟的檔案\n", successful_opens);
    for (int i = 0; i < successful_opens; i++) {
        char filename[32];
        sprintf(filename, "/fd_test/file%d.txt", i);
        int status = Unlink(filename);
        printf("   刪除檔案 %s 結果: %d\n", filename, status);
        Close(fds[i]);
        
    }
    
    int status = RmDir("/fd_test");
    printf("   刪除目錄 /fd_test 結果: %d\n", status);
    
    printf("\n5. 嘗試使用非法檔案描述符\n");
    result = Close(-5);
    printf("   Close(-5) 結果: %d (應為 ERROR)\n", result);
    
    result = Read(-5, buf, 10);
    printf("   Read(-5) 結果: %d (應為 ERROR)\n", result);
}

int main() {
    printf("=== 開始檔案系統邊界情況測試 ===\n\n");
    
    test_path_edge_cases();
    test_nested_directories();
    test_dot_and_dotdot();
    test_file_operations();
    test_symbolic_links();
    test_concurrent_operations();
    test_edge_cases();
    
    printf("\n=== 邊界情況測試完成 ===\n");
    printf("測試用例可能產生一些預期內的錯誤，這些是測試邊界條件的一部分\n");
    
    Sync();
    return 0;
}

/*
路徑邊界情況測試：

測試超長路徑名
測試空路徑和NULL路徑
測試連續多斜線路徑
測試包含特殊字符的路徑
深度嵌套目錄測試：

創建多達10層的嵌套目錄結構
測試在深層目錄中進行各種操作
測試刪除深層目錄結構
"." 和 ".." 路徑測試：

使用 "." 和 ".." 進行相對路徑操作
嘗試對 "." 和 ".." 自身進行操作
測試複雜的路徑組合
檔案操作邊界情況測試：

多次開啟同一檔案
重複關閉同一檔案
Seek 操作的各種極端情況
符號連結測試：

基本符號連結操作
測試循環符號連結
測試多層符號連結
大量文件操作測試：

創建大量文件和目錄
對大量文件執行操作
刪除大量文件和目錄
其他邊界情況：

嘗試移除根目錄
移除非空目錄
超出檔案描述符上限
使用非法檔案描述符
*/