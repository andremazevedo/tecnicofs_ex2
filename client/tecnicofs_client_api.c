#include "tecnicofs_client_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int session_id;
int serverFd, clientFd;
char client_pipe_name[40];

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    /* TODO: Implement this */
    char tfs_op_code = TFS_OP_CODE_MOUNT;
    char request[41];

    unlink(client_pipe_path);

    if (mkfifo (client_pipe_path, 0644) == -1) {
        perror("Erro no mkfifo");
        return -1;
    }

    if ((serverFd = open(server_pipe_path, O_WRONLY)) == -1) {
        perror("Erro ao abrir o pipe");
        return -1;
    }
   
    memcpy(request, &tfs_op_code, sizeof(char));
    memset(request + 1, '\0', sizeof(char) * 40);
    memcpy(request + 1, client_pipe_path, sizeof(char) * strlen(client_pipe_path));

    if (write (serverFd, request, sizeof(request)) == -1) {
        perror("Erro ao escrever no pipe");
        return -1;
    }

    if ((clientFd = open(client_pipe_path, O_RDONLY)) == -1) {
        perror("Erro ao abrir o pipe");
        return -1;
    }

    if (read(clientFd, &session_id, sizeof(int)) == -1) {
        perror("Erro na leitura");
        return -1;
    }

    if (session_id < 0) 
        return -1;

    memset(client_pipe_name, '\0', sizeof(char) * 40);
    memcpy(client_pipe_name, client_pipe_path, sizeof(char) * strlen(client_pipe_path));

    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    char tfs_op_code = TFS_OP_CODE_UNMOUNT;
    char request[5];
    int response;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request + 1, &session_id, sizeof(int));

    if (write (serverFd, request, sizeof(request)) == -1) {
        perror("Erro ao escrever no pipe");
        return -1;
    }

    if (read(clientFd, &response, sizeof(int)) == -1) {
        perror("Erro na leitura");
        return -1;
    }

    close(clientFd);
    close(serverFd);

    if(unlink(client_pipe_name) != 0) {
        return -1;
    }

    return response;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    char tfs_op_code = TFS_OP_CODE_OPEN;
    char request[49];
    int fd;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request +  1, &session_id, sizeof(int));
    memset(request +  5, '\0', sizeof(char) * 40);
    memcpy(request +  5, name, sizeof(char) * strlen(name));
    memcpy(request + 45, &flags, sizeof(int));

    if (write (serverFd, request, sizeof(request)) == -1) {
        perror("Erro ao escrever no pipe");
        return -1;
    }

    if (read(clientFd, &fd, sizeof(int)) == -1) {
        perror("Erro na leitura");
        return -1;
    }

    return fd;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    char tfs_op_code = TFS_OP_CODE_CLOSE;
    char request[9];
    int response;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request + 1, &session_id, sizeof(int));
    memcpy(request + 5, &fhandle, sizeof(int));

    if (write (serverFd, request, sizeof(request)) == -1) {
        perror("Erro ao escrever no pipe");
        return -1;
    }

    if (read(clientFd, &response, sizeof(int)) == -1) {
        perror("Erro na leitura");
        return -1;
    }

    return response;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    char tfs_op_code = TFS_OP_CODE_WRITE;
    char request[17 + len];
    ssize_t r;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request +  1, &session_id, sizeof(int));
    memcpy(request +  5, &fhandle, sizeof(int));
    memcpy(request +  9, &len, sizeof(size_t));
    memcpy(request + 17, buffer, sizeof(char) * len);

    if (write (serverFd, request, sizeof(request)) == -1) {
        perror("Erro ao escrever no pipe");
        return -1;
    }

    if (read(clientFd, &r, sizeof(ssize_t)) == -1) {
        perror("Erro na leitura");
        return -1;
    }

    return r;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    char tfs_op_code = TFS_OP_CODE_READ;
    char request[17];
    ssize_t r;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request + 1, &session_id, sizeof(int));
    memcpy(request + 5, &fhandle, sizeof(int));
    memcpy(request + 9, &len, sizeof(size_t));

    if (write (serverFd, request, sizeof(request)) == -1) {
        perror("Erro ao escrever no pipe");
        return -1;
    }

    if (read(clientFd, &r, sizeof(ssize_t)) == -1) {
        perror("Erro na leitura");
        return -1;
    }

    if (r == -1)
        return -1;

    if (read(clientFd, buffer, sizeof(char) * (size_t)r) == -1) {
        perror("Erro na leitura");
        return -1;
    }

    return r;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    char tfs_op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    char request[5];
    int response;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request + 1, &session_id, sizeof(int));

    if (write (serverFd, request, sizeof(request)) == -1) {
        perror("Erro ao escrever no pipe");
        return -1;
    }
    
    if (read(clientFd, &response, sizeof(int)) == -1) {
        perror("Erro na leitura");
        return -1;
    }

    return response;
}
