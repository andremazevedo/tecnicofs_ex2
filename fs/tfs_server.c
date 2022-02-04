#include "operations.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define S (20)

/*
 * Session
 */
typedef struct {
    int s_id;
    //char s_buffer[BLOCK_SIZE]; //qual devia ser o tamanho do buffer?
    char *s_buffer;
    pthread_mutex_t s_mutex;
    pthread_cond_t s_cond;
    int s_buffer_count;
    int s_clientFd;
} session_t;

typedef enum { AVAILABLE = 0, UNAVAILABLE = 1 } allocation_session_t;

/* Session table */
static session_t session_table[S];
static char free_session[S];

pthread_mutex_t free_session_mutex;

static inline bool valid_session(int session_id) {
    return session_id >= 0 && session_id < S;
}

/*
 * Returns a pointer to an existing session.
 * Input:
 *  - session_id: identifier of the session
 * Returns: pointer if successful, NULL if failed
 */
session_t *session_get(int session_id) {
    if (!valid_session(session_id)) {
        return NULL;
    }

    return &session_table[session_id];
}

/*
 * Initializes Server
 * Returns 0 if successful, -1 otherwise.
 */
int server_init() {
    for (int i = 0; i < S; i++) {
        free_session[i] = AVAILABLE;
    }

    for (int i = 0; i < S; i++) {
        session_table[i].s_id = i;

        if (pthread_mutex_init(&session_table[i].s_mutex, NULL) != 0) 
            return -1;

        if (pthread_cond_init(&session_table[i].s_cond, NULL) != 0)
            return -1;

        session_table[i].s_buffer_count = 0;
    }

    if (pthread_mutex_init(&free_session_mutex, NULL) != 0) 
        return -1;

    return 0;
}

/*
 * Destroys Server
 * Returns 0 if successful, -1 otherwise.
 */
int server_destroy() { 
    for (int i = 0; i < S; i++) {
        if (pthread_mutex_destroy(&session_table[i].s_mutex) != 0)
            return -1;

        if (pthread_cond_destroy(&session_table[i].s_cond) != 0)
            return -1;
    }

    if (pthread_mutex_destroy(&free_session_mutex) != 0)
        return -1;

    return 0;
}


int open_pipe(const char *pipename, int flags) {
    int fd = -1;

    while (fd < 0) {
        fd = open(pipename, flags);
        if ((fd == -1) && (errno != EINTR)) {
            fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));//to remove
            return -1;
        }
    }

    return fd;
}


int close_pipe(int fd) {
    int ret = -1;

    while (ret < 0) {
        ret = close(fd);
        if ((fd == -1) && (errno != EINTR)) {
            fprintf(stderr, "[ERR]: close failed: %s\n", strerror(errno));//to remove
            return -1;
        }
    }

    return ret;
}


// helper function to send messages
// retries to send whatever was not sent in the begginning
ssize_t send_msg(int fd, const void *buf, size_t count, session_t *session) {
    size_t written = 0;

    while (written < count) {
        ssize_t ret = write(fd, buf + written, count - written);
        if (ret == -1) {
            if (errno == EPIPE) {
                session->s_buffer_count = 0;//???
                if (close_pipe(session->s_clientFd) == -1) {
                    perror("Erro ao fechar o pipe");
                    return -1;
                }

                if (pthread_mutex_lock(&free_session_mutex) != 0) {
                    return -1;
                }
                free_session[session->s_id] = AVAILABLE;
                if (pthread_mutex_unlock(&free_session_mutex) != 0) {
                    return -1;
                }

                return 0;
            } else if(errno != EINTR){
                // if error is diferent from epipe and eintr 
                fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));//to remove
                return -1;
            }
        }

        written += (size_t)ret;
    }

    return (ssize_t)written;
}


// helper function to receive messages
// retries to receive whatever was not received in the begginning//to remove
ssize_t receive_msg(int fd, void *buf, size_t count) {
    size_t readBytes = 0;

    while (readBytes < count) {
        ssize_t ret = read(fd, buf + readBytes, count - readBytes);
        if (ret == -1) {
            if (errno != EINTR) {
                fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));//to remove
                return -1;
            }
        } else if (ret == 0) {
            return ret;
        }

        readBytes += (size_t)ret;
    }

    return (ssize_t)readBytes;
}





void *fnThread(void *arg) {
    session_t *session = (session_t*)arg;
    
    char tfs_op_code;
    for(;;) {
        if (pthread_mutex_lock(&session->s_mutex) != 0) {
            perror("Erro no lock");// to remove
            exit(-1);
        }

        //printf("\nsession_id on thread: %d\n\n", session->s_id);//to remove
        while (session->s_buffer_count == 0) {
            pthread_cond_wait(&session->s_cond, &session->s_mutex);
        }

        memcpy(&tfs_op_code, session->s_buffer,sizeof(char));
        //printf("tfs_op_code on thread %d: %d.\n", session->s_id, tfs_op_code);//to remove

        if (tfs_op_code == TFS_OP_CODE_UNMOUNT) {
            //printf("TFS_UNMOUNT on thread %d\n", session->s_id);//to remove
            int session_id;

            memcpy(&session_id, session->s_buffer + 1, sizeof(int));
            //printf("session_id on thread %d: %d.\n", session->s_id, session_id);//to remove

            if (pthread_mutex_lock(&free_session_mutex) != 0) {
                perror("Erro no lock");
                exit(-1);
            }

            if (!valid_session(session_id) || free_session[session_id] == AVAILABLE) {
                if (pthread_mutex_unlock(&free_session_mutex) != 0) {
                    perror("Erro no lock");
                    exit(-1);
                }
                int ret = -1;
                if (send_msg(session->s_clientFd, &ret, sizeof(int), session) == -1) {
                    exit(-1);
                }
                continue;
            }

            if (pthread_mutex_unlock(&free_session_mutex) != 0) {
                perror("Erro no lock");
                exit(-1);
            }

            int ret = 0;
            //printf("response on thread %d: %d.\n", session->s_id, ret);//to remove
            if (send_msg(session->s_clientFd, &ret, sizeof(int), session) == -1) {
                exit(-1);
            }

            if (pthread_mutex_lock(&free_session_mutex) != 0) {
                perror("Erro no lock");
                exit(-1);
            }

            if (close_pipe(session->s_clientFd) == -1) {
                perror("Erro ao fechar o pipe");
                exit(-1);
            }
            free_session[session->s_id] = AVAILABLE;

            if (pthread_mutex_unlock(&free_session_mutex) != 0) {
                perror("Erro no lock");
                exit(-1);
            }
        }

        if (tfs_op_code == TFS_OP_CODE_OPEN) {
            //printf("TFS_OPEN on thread %d\n", session->s_id);//to remove
            char path[40];
            int flags;

            memcpy(path, session->s_buffer + 5, sizeof(path));
            memcpy(&flags, session->s_buffer + 45, sizeof(int));
            //printf("path on thread %d: %s.\n", session->s_id, path);//to remove
            //printf("flags on thread %d: %d.\n", session->s_id, flags);//to remove

            int fd;
            fd = tfs_open(path, flags);
            //printf("fd on thread %d: %d.\n", session->s_id, fd);//to remove

            if (send_msg(session->s_clientFd, &fd, sizeof(int), session) == -1) {
                exit(-1);
            }
        }

        if (tfs_op_code == TFS_OP_CODE_CLOSE) {
            //printf("TFS_CLOSE on thread %d\n", session->s_id);//to remove
            int fd;

            memcpy(&fd, session->s_buffer + 5, sizeof(int));
            //printf("fd on thread %d: %d.\n", session->s_id, fd);//to remove

            int ret;
            ret = tfs_close(fd);
            //printf("close on thread %d: %d.\n", session->s_id, ret);//to remove
            
            if (send_msg(session->s_clientFd, &ret, sizeof(int), session) == -1) {
                exit(-1);
            }
        }

        if (tfs_op_code == TFS_OP_CODE_WRITE) {
            printf("TFS_WRITE on thread %d\n", session->s_id);//to remove
            int fhandle;
            size_t len;

            memcpy(&fhandle, session->s_buffer + 5, sizeof(int));
            memcpy(&len, session->s_buffer + 9, sizeof(size_t));
            printf("fhandle on thread %d: %d.\n", session->s_id, fhandle);//to remove
            printf("len on thread %d: %lu.\n", session->s_id, len);//to remove

            char buffer[len];
            memcpy(buffer, session->s_buffer + 17, sizeof(buffer));
            printf("buffer on thread %d: %s.\n", session->s_id, buffer);//to remove

            ssize_t r;
            r = tfs_write(fhandle, buffer, len);
            printf("r on thread %d: %lu.\n", session->s_id, r);//to remove

            if (send_msg(session->s_clientFd, &r, sizeof(ssize_t), session) == -1) {
                exit(-1);
            }
        }

        if (tfs_op_code == TFS_OP_CODE_READ) {
            //printf("TFS_READ on thread %d\n", session->s_id);//to remove
            int fhandle;
            size_t len;

            memcpy(&fhandle, session->s_buffer + 5, sizeof(int));
            memcpy(&len, session->s_buffer + 9, sizeof(size_t));
            //printf("fhandle on thread %d: %d.\n", session->s_id, fhandle);//to remove
            //printf("len on thread %d: %lu.\n", session->s_id, len);//to remove

            char buffer[len];
            ssize_t r;
            r = tfs_read(fhandle, buffer, len);
            //printf("r on thread %d: %lu.\n", session->s_id, r);//to remove
            //printf("buffer on thread %d: %s.\n", session->s_id, buffer);//to remove

            if (send_msg(session->s_clientFd, &r, sizeof(ssize_t), session) == -1) {
                exit(-1);
            }

            if (r != -1) {
                if (send_msg(session->s_clientFd, buffer, sizeof(char) * (size_t)r, session) == -1) {
                    exit(-1);
                }
            }
        }

        session->s_buffer_count--;
        if (pthread_mutex_unlock(&session->s_mutex) != 0) {
            perror("Erro no unlock");
            exit(-1);
        }
    }
    
}




int main(int argc, char **argv) {

    int serverFd;
    pthread_t tid[S];

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */
    if (tfs_init() == -1) {
        return -1;
    }
    if (server_init() == -1) {
        return -1;
    }

    for (int i = 0; i < S; i++) {
        if (pthread_create(&tid[i], NULL, fnThread, (void*)&session_table[i]) == -1) {
            perror("Erro no pthread_create");//to remove return -1;
        }
    }

    unlink(pipename);
    
    if (mkfifo (pipename, 0644) == -1) {
        perror("Erro no mkfifo");//to remove
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    if ((serverFd = open_pipe(pipename, O_RDONLY)) == -1) {
        perror("Erro ao abrir o pipe");
        return -1;
    }

    
    ssize_t r;
    char tfs_op_code;
    int session_id;
    for(;;) {

        if ((r = receive_msg(serverFd, &tfs_op_code, sizeof(tfs_op_code))) == -1) {
            return -1;
        }

        if (r == 0) {
            printf("client disconnected\n");//to remove
            if ((serverFd = open_pipe(pipename, O_RDONLY)) == -1) {
                perror("Erro ao abrir o pipe");
                return -1;
            }
            continue;
        }

        //printf("tfs_op_code on server: %d.\n", tfs_op_code);//to remove

        if (tfs_op_code == TFS_OP_CODE_MOUNT) {
            //printf("TFS_MOUNT on server\n");//to remove
            int clientFd;
            char client_pipe_path[40];
            if (receive_msg(serverFd, client_pipe_path, sizeof(client_pipe_path)) == -1) {
                return -1;
            }
            //printf("client_pipe_path on server: %s.\n", client_pipe_path);//to remove

            if ((clientFd = open_pipe(client_pipe_path, O_WRONLY)) == -1) {
                perror("Erro ao abrir o pipe");
                return -1;
            }

            if (pthread_mutex_lock(&free_session_mutex) != 0) {
                perror("Erro no lock");
                return -1;
            }
            
            session_id = -1;
            for (int id = 0; id < S; id++) { 
                if (free_session[id] == AVAILABLE) {
                    free_session[id] = UNAVAILABLE;
                    session_id = id;

                    if (pthread_mutex_lock(&session_table[session_id].s_mutex) != 0) {
                        perror("Erro no lock");
                        return -1;
                    }

                    session_table[session_id].s_clientFd = clientFd;

                    if (pthread_mutex_unlock(&session_table[session_id].s_mutex) != 0) {
                        perror("Erro no unlock");
                        return -1;
                    }
                    break;
                }
            }

            if (pthread_mutex_unlock(&free_session_mutex) != 0) {
                perror("Erro no lock");
                return -1;
            }

            //printf("session_id on server: %d.\n", session_id);//to remove
            if (send_msg(clientFd, &session_id, sizeof(int), &session_table[session_id]) == -1) {
                exit(-1);
            }
            continue;
        } 

        if (receive_msg(serverFd, &session_id, sizeof(int)) == -1) {
            return -1;
        }
        
        session_t *session = session_get(session_id);
        if (session == NULL) {
            return -1;
        }

        if (pthread_mutex_lock(&session->s_mutex) != 0) {
            perror("Erro no lock");
            return -1;
        }

        if (tfs_op_code == TFS_OP_CODE_UNMOUNT) {
            //printf("TFS_UNMOUNT on server\n");//to remove
            char request[5];
            memcpy(request ,&tfs_op_code, sizeof(char));
            memcpy(request + 1 ,&session_id, sizeof(int));
            
            session->s_buffer = (char *) malloc(sizeof(request));
            memcpy(session->s_buffer, request, sizeof(request));
        }
        
        if (tfs_op_code == TFS_OP_CODE_OPEN) {
            //printf("TFS_OPEN on server\n");//to remove
            char request[49];
            memcpy(request, &tfs_op_code, sizeof(char));
            memcpy(request + 1 ,&session_id, sizeof(int));
            if (receive_msg(serverFd, request + 5, sizeof(char) * 44) == -1) {
                return -1;
            }

            session->s_buffer = (char *) malloc(sizeof(request));
            memcpy(session->s_buffer, request, sizeof(request));
        }

        if (tfs_op_code == TFS_OP_CODE_CLOSE) {
            //printf("TFS_CLOSE on server\n");//to remove
            char request[9];
            memcpy(request, &tfs_op_code, sizeof(char));
            memcpy(request + 1 ,&session_id, sizeof(int));
            if (receive_msg(serverFd, request + 5, sizeof(char) * 4) == -1) {
                return -1;
            }

            session->s_buffer = (char *) malloc(sizeof(request));
            memcpy(session->s_buffer, request, sizeof(request));
        }

        if (tfs_op_code == TFS_OP_CODE_WRITE) {
            //printf("TFS_WRITE on server\n");//to remove
            int fhandle;
            size_t len;

            if (receive_msg(serverFd, &fhandle, sizeof(int)) == -1) {
                return -1;
            }
            if (receive_msg(serverFd, &len, sizeof(size_t)) == -1) {
                return -1;
            }

            char request[17 + len];
            memcpy(request, &tfs_op_code, sizeof(char));
            memcpy(request +  1, &session_id, sizeof(int));
            memcpy(request +  5, &fhandle, sizeof(int));
            memcpy(request +  9, &len, sizeof(size_t));
            if (receive_msg(serverFd, request + 17, sizeof(char) * len) == -1) {
                return -1;
            }

            session->s_buffer = (char *) malloc(sizeof(request));
            memcpy(session->s_buffer, request, sizeof(request));
        }

        if (tfs_op_code == TFS_OP_CODE_READ) {
            //printf("TFS_READ on server\n");//to remove
            char request[17];
            memcpy(request, &tfs_op_code, sizeof(char));
            memcpy(request + 1 ,&session_id, sizeof(int));
            if (receive_msg(serverFd, request + 5, sizeof(char) * 12) == -1) {
                return -1;
            }

            session->s_buffer = (char *) malloc(sizeof(request));
            memcpy(session->s_buffer, request, sizeof(request));
        }

        if (tfs_op_code == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED) {
            //printf("TFS_SHUTDOWN_AFTER_ALL_CLOSED on server\n");//to remove
            int ret;
            ret = tfs_destroy_after_all_closed();
            //printf("response on server: %d.\n", ret);//to remove

            if (send_msg(session->s_clientFd, &ret, sizeof(int), session) == -1) {
                exit(-1);
            }

            if (ret != -1) {
                break;
            }
        }
        
        session->s_buffer_count++;
        if (pthread_cond_signal(&session->s_cond) != 0) {
            pthread_mutex_unlock(&session->s_mutex);
            perror("Erro no signal");
            return -1;
        }

        if (pthread_mutex_unlock(&session->s_mutex) != 0) {
            perror("Erro no unlock");
            return -1;
        }

    }


    if (close_pipe(serverFd) == -1) {
        perror("Erro ao fechar o pipe");
        return -1;
    }
    if (unlink(pipename) != 0) {
        perror("Erro no unlink");
        return -1;
    }
    if (server_destroy() == -1) {
        return -1;
    }

    printf("Server ended sucessfully!!!!");//to remove

    return 0;
}