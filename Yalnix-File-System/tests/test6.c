// tests.c
#include <stdio.h>
#include <string.h>
#include <comp421/yalnix.h>
#include <comp421/iolib.h>

#define BUF_SIZE 256

static int failures = 0;

static void run_test(const char *name, int got, int expect_ok) {
    printf("%-40s : ", name);
    if (got == expect_ok) {
        printf("PASS\n");
    } else {
        printf("FAIL (got %d)\n", got);
        failures++;
    }
}

int main(void) {
    char buf[BUF_SIZE];

    // OPEN
    run_test("open normal (/foo)", Create("/foo") >= 0, 1);
    run_test("open error invalid path (\"\")", Open("") >= 0, 0);
    run_test("open error non-existent parent (/no/such)", Open("/no/such/file") >= 0, 0);
    run_test("open error non-existent file (/missing)", Open("/missing") >= 0, 0);

    // CREATE
    run_test("create normal new file (/a)", Create("/a") >= 0, 1);
    run_test("create normal existing regular (/a)", Create("/a") >= 0, 1);
    SymLink("/a", "/alink"); 
    run_test("create normal existing symlink", Create("/alink") >= 0, 1);
    run_test("create error existing dir (/)", Create("/") >= 0, 0);
    run_test("create error existing dir (/.)", Create("/.") >= 0, 0);
    run_test("create error invalid path (\"\")", Create("") >= 0, 0);
    run_test("create error non-existent parent (/x/y/z)", Create("/x/y/z") >= 0, 0);

    // READ
    int fd = Create("/rtest");
    Write(fd, "hello", 5);
    Seek(fd, 0, SEEK_SET);
    run_test("read normal", 
        Read(fd, buf, 5) == 5
    , 1);
    // simulate delete 
    Close(fd);
    Unlink("/rtest");
    int efd = Open("/rtest");
    run_test("read error deleted file", Read(efd, buf, 1) >= 0, 0);

    // WRITE
    fd = Create("/wtest");
    run_test("write normal", Write(fd, "abc", 3) == 3, 1);
    Close(fd);
    Unlink("/wtest");
    int wfd = Open("/wtest");
    // reuse old fd will have wrong reuse
    run_test("write error deleted file", 
        Write(wfd, "x", 1) >= 0, 0);

    // LINK
    Create("/linksrc");
    run_test("link normal", Link("/linksrc","/linkdest") == 0, 1);
    run_test("link error invalid old/new", Link("","/b") == 0, 0);
    run_test("link error non-existent old", Link("/nope","/b") == 0, 0);
    MkDir("/d");
    run_test("link error old is dir", Link("/d","/dlnk") == 0, 0);
    run_test("link error new exists", Link("/linksrc","/linkdest") == 0, 0);
    run_test("link error new is root", Link("/linksrc","/") == 0, 0);
    run_test("link error new ends /", Link("/linksrc","/foo/") == 0, 0);

    // UNLINK
    run_test("unlink normal", Unlink("/linkdest")==0, 1);
    run_test("unlink error invalid", Unlink("")==0, 0);
    run_test("unlink error root", Unlink("/")==0, 0);
    run_test("unlink error non-existent parent", Unlink("/x/y") == 0, 0);
    run_test("unlink error non-existent file", Unlink("/nofile")==0, 0);
    MkDir("/ud"); run_test("unlink error is dir", Unlink("/ud")==0, 0);
    run_test("unlink error ends /", Unlink("/ud/")==0, 0);

    // SYMLINK
    run_test("symlink normal", SymLink("/a","/s1")==0, 1);
    run_test("symlink error invalid", SymLink("","")==0, 0);
    run_test("symlink error new exists", SymLink("/a","/s1")==0, 0);
    run_test("symlink error new is root", SymLink("/a","/")==0, 0);
    run_test("symlink error ends /", SymLink("/a","/foo/")==0, 0);

    // READLINK
    run_test("readlink normal", ReadLink("/s1", buf, BUF_SIZE)>0, 1);
    run_test("readlink error invalid", ReadLink("", buf, BUF_SIZE)>0, 0);
    run_test("readlink error non-existent", ReadLink("/nosymlink", buf, BUF_SIZE)>0, 0);
    Create("/reg"); run_test("readlink error not symlink", ReadLink("/reg", buf, BUF_SIZE)>0, 0);

    // MKDIR
    run_test("mkdir normal", MkDir("/md")==0, 1);
    run_test("mkdir error invalid", MkDir("")==0, 0);
    run_test("mkdir error exists", MkDir("/md")==0, 0);
    run_test("mkdir error ends /", MkDir("/md/")==0, 0);

    // RMDIR
    run_test("rmdir normal", RmDir("/md")==0, 1);
    run_test("rmdir error invalid", RmDir("")==0, 0);
    run_test("rmdir error non-existent", RmDir("/nodir")==0, 0);
    Create("/f"); run_test("rmdir error not dir", RmDir("/f")==0, 0);
    run_test("rmdir error root", RmDir("/")==0, 0);
    MkDir("/d"); MkDir("/d/dd"); run_test("rmdir error root2", RmDir("/d/dd/./../")==0, 0);
    MkDir("/nonempty"); Create("/nonempty/x"); run_test("rmdir error not empty", RmDir("/nonempty")==0, 0);

    // CHDIR
    run_test("chdir normal", MkDir("/cd")==0 && ChDir("/cd")==0, 1);
    run_test("chdir error invalid", ChDir("")==0, 0);
    run_test("chdir error non-existent", ChDir("/nope")==0, 0);
    Create("/file"); run_test("chdir error not dir", ChDir("/file")==0, 0);

    printf("\n%d failure(s)\n", failures);
    Shutdown();
    return failures == 0 ? 0 : 1;
}