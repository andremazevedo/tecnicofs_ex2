#include "../client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*  Double Write */

int main(int argc, char **argv) {
    int f;

    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }

    assert(tfs_mount(argv[1], argv[2]) == 0);

    f = tfs_open("/f10", TFS_O_CREAT);
    assert(f != -1);

    assert(tfs_close(f) != -1);

    assert(tfs_shutdown_after_all_closed() != -1);

    printf("Successful test.\n");

    return 0;
}