#include "tecnicofs_client_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int session_id;
int serverFd, clientFd;
char client_pipe_name[40];

// helper function to open a pipe
int open_pipe(const char *pipename, int flags) {
    int fd = -1;

    while (fd < 0) {
        fd = open(pipename, flags);
        if ((fd == -1) && (errno != EINTR)) {
            return -1;
        }
    }

    return fd;
}

// helper function to close a pipe
int close_pipe(int fd) {
    int ret = -1;

    while (ret < 0) {
        ret = close(fd);
        if ((ret == -1) && (errno != EINTR)) {
            return -1;
        }
    }

    return ret;
}

// helper function to send messages
// only sends messages atomically, returns -1 otherwise
ssize_t send_msg(int fd, const void *buf, size_t count) {
    size_t written = 0;

    while (written < count) {
        ssize_t ret = write(fd, buf + written, count - written);
        if ((ret == -1) && (errno != EINTR)) {
            return -1;
        } else if (ret != count) {
            return -1;
        }

        written += (size_t)ret;
    }

    return (ssize_t)written;
}

// helper function to receive messages
// returns -1 if server pipe is closed
ssize_t receive_msg(int fd, void *buf, size_t count) {
    size_t readBytes = 0;

    while (readBytes < count) {
        ssize_t ret = read(fd, buf + readBytes, count - readBytes);
        if ((ret == -1) && (errno != EINTR)) {
            return -1;
        } else if (ret == 0) {
            return -1;
        }

        readBytes += (size_t)ret;
    }

    return (ssize_t)readBytes;
}



int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    char tfs_op_code = TFS_OP_CODE_MOUNT;
    char request[41];

    unlink(client_pipe_path);

    if (mkfifo (client_pipe_path, 0644) == -1) {
        return -1;
    }

    if ((serverFd = open_pipe(server_pipe_path, O_WRONLY)) == -1) {
        return -1;
    }
   
    memcpy(request, &tfs_op_code, sizeof(char));
    memset(request + 1, '\0', sizeof(char) * 40);
    memcpy(request + 1, client_pipe_path, sizeof(char) * strlen(client_pipe_path));

    if (send_msg(serverFd, request, sizeof(request)) == -1) {
        return -1;
    }

    if ((clientFd = open_pipe(client_pipe_path, O_RDONLY)) == -1) {
        return -1;
    }

    if (receive_msg(clientFd, &session_id, sizeof(int)) == -1) {
        return -1;
    }

    if (session_id < 0) 
        return -1;

    memset(client_pipe_name, '\0', sizeof(char) * 40);
    memcpy(client_pipe_name, client_pipe_path, sizeof(char) * strlen(client_pipe_path));

    return 0;
}

int tfs_unmount() {
    char tfs_op_code = TFS_OP_CODE_UNMOUNT;
    char request[5];
    int response;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request + 1, &session_id, sizeof(int));

    if (send_msg(serverFd, request, sizeof(request)) == -1) {
        return -1;
    }

    if (receive_msg(clientFd, &response, sizeof(int)) == -1) {
        return -1;
    }

    if (close_pipe(clientFd) == -1) {
        return -1;
    }

    if (close_pipe(serverFd) == -1) {
        return -1;
    }

    if(unlink(client_pipe_name) != 0) {
        return -1;
    }

    return response;
}

int tfs_open(char const *name, int flags) {
    char tfs_op_code = TFS_OP_CODE_OPEN;
    char request[49];
    int fd;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request +  1, &session_id, sizeof(int));
    memset(request +  5, '\0', sizeof(char) * 40);
    memcpy(request +  5, name, sizeof(char) * strlen(name));
    memcpy(request + 45, &flags, sizeof(int));

    if (send_msg(serverFd, request, sizeof(request)) == -1) {
        return -1;
    }

    if (receive_msg(clientFd, &fd, sizeof(int)) == -1) {
        return -1;
    }

    return fd;
}

int tfs_close(int fhandle) {
    char tfs_op_code = TFS_OP_CODE_CLOSE;
    char request[9];
    int response;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request + 1, &session_id, sizeof(int));
    memcpy(request + 5, &fhandle, sizeof(int));

    if (send_msg(serverFd, request, sizeof(request)) == -1) {
        return -1;
    }

    if (receive_msg(clientFd, &response, sizeof(int)) == -1) {
        return -1;
    }

    return response;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    char tfs_op_code = TFS_OP_CODE_WRITE;
    char request[17 + len];
    ssize_t r;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request +  1, &session_id, sizeof(int));
    memcpy(request +  5, &fhandle, sizeof(int));
    memcpy(request +  9, &len, sizeof(size_t));
    memcpy(request + 17, buffer, sizeof(char) * len);

    if (send_msg(serverFd, request, sizeof(request)) == -1) {
        return -1;
    }

    if (receive_msg(clientFd, &r, sizeof(ssize_t)) == -1) {
        return -1;
    }

    return r;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    char tfs_op_code = TFS_OP_CODE_READ;
    char request[17];
    ssize_t r;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request + 1, &session_id, sizeof(int));
    memcpy(request + 5, &fhandle, sizeof(int));
    memcpy(request + 9, &len, sizeof(size_t));

    if (send_msg(serverFd, request, sizeof(request)) == -1) {
        return -1;
    }

    if (receive_msg(clientFd, &r, sizeof(ssize_t)) == -1) {
        return -1;
    }

    if (r == -1) {
        return -1;
    }

    if (receive_msg(clientFd, buffer, sizeof(char) * (size_t)r) == -1) {
        return -1;
    }

    return r;
}

int tfs_shutdown_after_all_closed() {
    char tfs_op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    char request[5];
    int response;

    memcpy(request, &tfs_op_code, sizeof(char));
    memcpy(request + 1, &session_id, sizeof(int));

    if (send_msg(serverFd, request, sizeof(request)) == -1) {
        return -1;
    }
    
    if (receive_msg(clientFd, &response, sizeof(int)) == -1) {
        return -1;
    }

    return response;
}