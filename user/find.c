#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

// find(path, target):
//     mở path, lấy type bằng fstat
//     nếu mở lỗi: báo lỗi, return

//     nếu path là FILE:
//         do nothing
//     nếu path là DIR:
//         với mỗi entry de trong thư mục:
//             bỏ qua nếu inum == 0, hoặc tên là "." hoặc ".."
//             tạo newpath = path + "/" + de.name
//             nếu de.name == target:   -> in newpath
//             nếu newpath là thư mục:  -> find(newpath, target)   (đệ quy)
//     đóng fd

void find(char *path, char *target) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
    case T_FILE:
        // Do nothing for file
        break;

    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;
            
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;

            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            if (strcmp(de.name, target) == 0) {
                printf("%s\n", buf);
            }

            if (stat(buf, &st) < 0) {
                printf("find: cannot stat %s\n", buf);
                continue;
            }

            if (st.type == T_DIR) {
                find(buf, target);
            }
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(2, "Usage: find <path> <target>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}
