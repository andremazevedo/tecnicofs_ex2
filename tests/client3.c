#include "../client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*  Append simple */

int main(int argc, char **argv) {

    char *str1 = "AAA";
    char *str2 = "BBBB";
    char *str3 = "AAABBBB";
    char *path = "/f7";
    char buffer[40];

    int f;
    ssize_t r;

    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }

    assert(tfs_mount(argv[1], argv[2]) == 0);

    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_write(f, str1, strlen(str1));
    assert(r == strlen(str1));

    assert(tfs_close(f) != -1);

    f = tfs_open(path, TFS_O_APPEND);
    assert(f != -1);

    r = tfs_write(f, str2, strlen(str2));
    assert(r == strlen(str2));

    assert(tfs_close(f) != -1);

    f = tfs_open(path, 0);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str3));

    buffer[r] = '\0';
    assert(strcmp(buffer, str3) == 0);

    assert(tfs_close(f) != -1);

    assert(tfs_unmount() == 0);

    printf("Successful test.\n");

    return 0;
}