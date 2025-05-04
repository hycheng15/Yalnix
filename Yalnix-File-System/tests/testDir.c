#include <stdio.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>

int main() {
    int status;
    int fd;
    
    printf("=== 目錄創建與刪除測試 ===\n");
    
    // 測試 1: 創建基本目錄
    printf("\n1. 測試創建目錄...\n");
    status = MkDir("/testdir");
    printf("創建目錄 /testdir 狀態：%d (應該是 0)\n", status);
    
    // 測試 2: 創建嵌套目錄
    printf("\n2. 測試創建嵌套目錄...\n");
    status = MkDir("/testdir/subdir");
    printf("創建嵌套目錄 /testdir/subdir 狀態：%d (應該是 0)\n", status);
    
    // 測試 3: 嘗試創建已存在的目錄
    printf("\n3. 測試創建已存在的目錄...\n");
    status = MkDir("/testdir");
    printf("創建已存在目錄結果：%d (應該返回錯誤)\n", status);
    
    // 測試 4: 在目錄中創建文件
    printf("\n4. 測試在目錄中創建文件...\n");
    fd = Create("/testdir/testfile");
    printf("在目錄中創建文件結果：%d (應該返回有效的文件描述符)\n", fd);
    if (fd > 0) {
        Write(fd, "This is a test file in directory", 32);
        Close(fd);
    }
    
    // 測試 5: 嘗試刪除非空目錄
    printf("\n5. 測試刪除非空目錄...\n");
    status = RmDir("/testdir");
    printf("刪除非空目錄結果：%d (應該返回錯誤)\n", status);
    
    // 測試 6: 刪除檔案以清空目錄
    printf("\n6. 刪除檔案以清空目錄...\n");
    status = Unlink("/testdir/testfile");
    printf("刪除檔案結果：%d (應該是 0)\n", status);
    
    // 測試 7: 刪除嵌套子目錄
    printf("\n7. 測試刪除嵌套子目錄...\n");
    status = RmDir("/testdir/subdir");
    printf("刪除子目錄結果：%d (應該是 0)\n", status);
    
    // 測試 8: 刪除已經為空的目錄
    printf("\n8. 測試刪除已清空的目錄...\n");
    status = RmDir("/testdir");
    printf("刪除空目錄結果：%d (應該是 0)\n", status);
    
    // 測試 9: 嘗試刪除不存在的目錄
    printf("\n9. 測試刪除不存在的目錄...\n");
    status = RmDir("/nonexistent");
    printf("刪除不存在目錄結果：%d (應該返回錯誤)\n", status);
    
    // 測試 10: 嘗試刪除根目錄
    printf("\n10. 測試刪除根目錄...\n");
    status = RmDir("/");
    printf("刪除根目錄結果：%d (應該返回錯誤)\n", status);
    
    // 測試 11: 嘗試直接刪除 . 或 ..
    printf("\n11. 測試直接刪除 . 目錄...\n");
    status = MkDir("/dottest");
    ChDir("/dottest");
    status = RmDir(".");
    printf("直接刪除 . 目錄結果：%d (應該返回錯誤)\n", status);
    ChDir("/");
    RmDir("/dottest");

    // 測試 12: 以尾隨 "/" 視同 "/."
    printf("\n12. 測試以尾隨 \"/\" 的 MkDir 行為...\n");
    // 先用不含尾隨斜線的形式創建 /foo
    status = MkDir("/foo");
    printf("創建目錄 /foo 狀態：%d (應該是 0)\n", status);

    // 再嘗試尾隨斜線，應當等同於 MkDir("/foo/.")，而失敗
    status = MkDir("/foo/");
    printf("創建目錄 /foo/ 結果：%d (應該返回錯誤 -1)\n", status);

    // 清理
    status = RmDir("/foo");
    printf("刪除目錄 /foo 狀態：%d (應該是 0)\n", status);

    // 測試 13: RmDir 直接刪除 . 目錄
    printf("\n13. RmDir 直接刪除 . 目錄...\n");
    status = RmDir(".");
    printf("直接刪除 . 目錄結果：%d (應該返回錯誤)\n", status);
    ChDir("/");
    
    printf("\n=== 目錄測試完成 ===\n");
    Sync();
    Shutdown();
    return 0;
}