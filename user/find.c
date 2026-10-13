#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

// run_cmd(cmd, ncmd, file):
//     if ncmd + 2 > MAXARG: print error, return
//     argv = cmd[0..ncmd-1] + file + NULL
//     fork
//         if fork fails: print error, return
//         child:  exec(argv[0], argv); if exec returns, print error, exit(1)
//         parent: wait for the child to finish
static void
run_cmd(char **cmd, int ncmd, char *file)
{
    char *argv[MAXARG];

    if (ncmd + 2 > MAXARG) {
        fprintf(2, "find: too many arguments for exec\n");
        return;
    }

    for (int i = 0; i < ncmd; i++) {
        argv[i] = cmd[i];
    }
    argv[ncmd] = file;
    argv[ncmd + 1] = 0;

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "find: fork failed\n");
        return;
    }

    if (pid == 0) {
        exec(argv[0], argv);
        fprintf(2, "find: exec %s failed\n", argv[0]);
        exit(1);
    }

    wait(0);
}

// find(path, target, cmd, ncmd):
//     open path, get its type using fstat
//     if open fails: print error, return
//
//     if path is a FILE:
//         do nothing
//     if path is a DIR:
//         for each entry de in the directory:
//             skip if inum == 0, or name is "." or ".."
//             build newpath = path + "/" + de.name
//             if de.name == target:
//                 if -exec was given (ncmd > 0): run_cmd(cmd, ncmd, newpath)
//                 else:                          print newpath
//             if newpath is a directory: find(newpath, target, cmd, ncmd)
//     close fd
void
find(char *path, char *target, char **cmd, int ncmd)
{
    char buf[MAXPATH], *p;
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
    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
            fprintf(2, "find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;

            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
                continue;

            if (strcmp(p, target) == 0) {
                if (ncmd > 0) {
                    run_cmd(cmd, ncmd, buf);
                } else {
                    printf("%s\n", buf);
                }
            }

            if (stat(buf, &st) < 0) {
                fprintf(2, "find: cannot stat %s\n", buf);
                continue;
            }

            if (st.type == T_DIR) {
                find(buf, target, cmd, ncmd);
            }
        }
        break;

    case T_FILE:
    default:
        break;
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
    if (argc == 3) {
        find(argv[1], argv[2], 0, 0);
        exit(0);
    }
    if (argc >= 5 && strcmp(argv[3], "-exec") == 0) {
        find(argv[1], argv[2], argv + 4, argc - 4);
        exit(0);
    }
    fprintf(2, "Usage: find <path> <target> [-exec <cmd> [args...]]\n");
    exit(1);
}