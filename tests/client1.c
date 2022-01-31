#include "../client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define COUNT 4
#define SIZE 256

/**
   This test fills in a new file up to 1 block via multiple writes, 
   each write always targeting only 1 block of the file, 
   then checks if the file contents are as expected
 */

int main(int argc, char **argv) {

    char *path = "/f4";

    char input[SIZE]; 
    memset(input, 'A', SIZE);

    char output [SIZE];

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

    for (int i = 0; i < COUNT; i++) {
        r = tfs_write(f, input, SIZE);
        assert(r == SIZE);
    }

    assert(tfs_close(f) != -1);

    f = tfs_open(path, 0);
    assert(f != -1);

    for (int i = 0; i < COUNT; i++) {
        r = tfs_read(f, output, SIZE);
        assert(r == SIZE);
        assert (memcmp(input, output, SIZE) == 0);
    }

    assert(tfs_close(f) != -1);

    assert(tfs_unmount() == 0);

    printf("Successful test.\n");

    return 0;
}
