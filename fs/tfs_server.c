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

#define S (1)

/*
 * Session
 */
typedef struct {
    int s_id;// remove? switch to thread?
    pthread_t s_tid;
    char s_buffer[BLOCK_SIZE]; //qual devia ser o tamanho do buffer?
    pthread_mutex_t s_mutex;
    pthread_cond_t s_cond;
    int s_buffer_count;
    int s_clientFd; // remove? switch to thread?
} session_t;

static session_t session_table[S];

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





void *fnThread(void *arg) {
    session_t *session = (session_t*)arg;
    char tfs_op_code;

    for(;;) {
        if (pthread_mutex_lock(&session->s_mutex) != 0) {
            perror("Erro no lock");
        }

        printf("\nsession_id on thread: %d\n\n", session->s_id);//to remove
        while (session->s_buffer_count == 0) {
            pthread_cond_wait(&session->s_cond, &session->s_mutex);
        }

        memcpy(&tfs_op_code, session->s_buffer,sizeof(char));
        printf("tfs_op_code on thread %d: %d.\n", session->s_id, tfs_op_code);//to remove

        if (tfs_op_code == TFS_OP_CODE_MOUNT) {
            printf("TFS_MOUNT on thread %d\n", session->s_id);//to remove
            char client_pipe_path[40];

            memcpy(client_pipe_path, session->s_buffer + 1, sizeof(client_pipe_path));
            printf("client_pipe_path on thread %d: %s.\n", session->s_id, client_pipe_path);//to remove

            if ((session->s_clientFd = open(client_pipe_path, O_WRONLY)) == -1) {
                perror("Erro ao abrir o pipe");
                //exit(-1);//unlock
            }

            printf("session_id on thread %d: %d.\n", session->s_id, session->s_id);//to remove
            if (write(session->s_clientFd, &session->s_id, sizeof(int)) == -1) {
                perror("Erro ao escrever no pipe");
                //exit(-1);//unlock
            }
        }

        if (tfs_op_code == TFS_OP_CODE_UNMOUNT) {
            printf("TFS_UNMOUNT on thread %d\n", session->s_id);//to remove
            close(session->s_clientFd);
        }

        if (tfs_op_code == TFS_OP_CODE_OPEN) {
            printf("TFS_OPEN on thread %d\n", session->s_id);//to remove
            char path[40];
            int flags;

            memcpy(path, session->s_buffer + 5, sizeof(path));
            memcpy(&flags, session->s_buffer + 45, sizeof(int));
            printf("path on thread %d: %s.\n", session->s_id, path);//to remove
            printf("flags on thread %d: %d.\n", session->s_id, flags);//to remove

            int fd;
            fd = tfs_open(path, flags);
            printf("fd on thread %d: %d.\n", session->s_id, fd);//to remove
            if (write(session->s_clientFd, &fd, sizeof(int)) == -1) {
                perror("Erro ao escrever no pipe");
                //exit(-1);//unlock
            }
        }

        if (tfs_op_code == TFS_OP_CODE_CLOSE) {
            printf("TFS_CLOSE on thread %d\n", session->s_id);//to remove
            int fd;

            memcpy(&fd, session->s_buffer + 5, sizeof(int));
            printf("fd on thread %d: %d.\n", session->s_id, fd);//to remove

            int response;
            response = tfs_close(fd);
            printf("close on thread %d: %d.\n", session->s_id, response);//to remove
            if (write(session->s_clientFd, &response, sizeof(int)) == -1) {
                perror("Erro ao escrever no pipe");
                //exit(-1);//unlock
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
            if (write(session->s_clientFd, &r, sizeof(ssize_t)) == -1) {
                perror("Erro ao escrever no pipe");
                //exit(-1);//unlock
            }
        }

        if (tfs_op_code == TFS_OP_CODE_READ) {
            printf("TFS_READ on thread %d\n", session->s_id);//to remove
            int fhandle;
            size_t len;

            memcpy(&fhandle, session->s_buffer + 5, sizeof(int));
            memcpy(&len, session->s_buffer + 9, sizeof(size_t));
            printf("fhandle on thread %d: %d.\n", session->s_id, fhandle);//to remove
            printf("len on thread %d: %lu.\n", session->s_id, len);//to remove

            char buffer[len];
            ssize_t r;
            r = tfs_read(fhandle, buffer, len);
            printf("r on thread %d: %lu.\n", session->s_id, r);//to remove
            printf("buffer on thread %d: %s.\n", session->s_id, buffer);//to remove
            if (write(session->s_clientFd, &r, sizeof(ssize_t)) == -1) {
                perror("Erro ao escrever no pipe");
                //exit(-1);//unlock
            }
            if (r != -1) {
                if (write(session->s_clientFd, buffer, sizeof(char) * (size_t)r) == -1) {
                    perror("Erro ao escrever no pipe");
                    //exit(-1);//unlock
                }
            }
        }

        session->s_buffer_count--;
        if (pthread_mutex_unlock(&session->s_mutex) != 0) {
            perror("Erro no unlock");
            //exit(-1);
        }
    }
    
}




int main(int argc, char **argv) {

    int serverFd;
    char tfs_op_code;

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */
    tfs_init();

    for (int i = 0; i < S; i++) {
        session_table[i].s_id = i;
        session_table[i].s_buffer_count = 0;

        if (pthread_mutex_init(&session_table[i].s_mutex, NULL) != 0) {
            perror("Erro no pthread_mutex_init");
        }

        if (pthread_cond_init(&session_table[i].s_cond, NULL) != 0) {
            perror("Erro no pthread_cond_init");
        }

        if (pthread_create(&session_table[i].s_tid, NULL, fnThread, (void*)&session_table[i]) == -1) {
            perror("Erro no pthread_create");
        }
    }

    unlink(pipename);
    
    if (mkfifo (pipename, 0644) == -1) {
        perror("Erro no mkfifo");
        return 1;
    }

    if ((serverFd = open(pipename, O_RDONLY)) == -1) {
        perror("Erro ao abrir o pipe");
        return -1;
    }
    
    ssize_t n;
    for(;;) {

        if ((n = read(serverFd, &tfs_op_code, sizeof(tfs_op_code))) == -1) {
            perror("Erro na leitura");
        }

        if (n == 0) {
            printf("client disconnected\n");//to remove
            serverFd = open(pipename, O_RDONLY);
            continue;
        }

        printf("tfs_op_code on server: %d.\n", tfs_op_code);//to remove
        
        if (tfs_op_code == TFS_OP_CODE_MOUNT) {
            printf("TFS_MOUNT on server\n");//to remove
            int session_id = 0;
            session_t *session = session_get(session_id);
            if (session == NULL) {
                return -1;
            }

            if (pthread_mutex_lock(&session->s_mutex) != 0) {
                perror("Erro no lock");
                return -1;
            }

            memcpy(session->s_buffer ,&tfs_op_code, sizeof(char));
            if (read(serverFd, session->s_buffer + 1, sizeof(char) * 40) == -1) {
                perror("Erro na leitura");//unlock
                return -1;
            }

            session->s_buffer_count++;
            if (pthread_cond_signal(&session->s_cond) != 0) {
                perror("Erro no signal");//unlock
                return -1;
            }

            if (pthread_mutex_unlock(&session->s_mutex) != 0) {
                perror("Erro no unlock");
                return -1;
            }
        }
        
        if (tfs_op_code == TFS_OP_CODE_UNMOUNT) {
            printf("TFS_MOUNT on server\n");//to remove
            int session_id;
            if (read(serverFd, &session_id, sizeof(int)) == -1) {
                perror("Erro na leitura");
                return -1;
            }

            session_t *session = session_get(session_id);
            if (session == NULL) {
                return -1;
            }

            memcpy(session->s_buffer ,&tfs_op_code, sizeof(char));
            memcpy(session->s_buffer + 1 ,&session_id, sizeof(int));

            session->s_buffer_count++;
            if (pthread_cond_signal(&session->s_cond) != 0) {
                perror("Erro no signal");//unlock
                return -1;
            }

            if (pthread_mutex_unlock(&session->s_mutex) != 0) {
                perror("Erro no unlock");
                return -1;
            }
        }
        
        if (tfs_op_code == TFS_OP_CODE_OPEN) {
            printf("TFS_OPEN on server\n");//to remove
            int session_id;
            if (read(serverFd, &session_id, sizeof(int)) == -1) {
                perror("Erro na leitura");
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

            memcpy(session->s_buffer ,&tfs_op_code, sizeof(char));
            memcpy(session->s_buffer + 1 ,&session_id, sizeof(int));
            if (read(serverFd, session->s_buffer + 5, sizeof(char) * 44) == -1) {
                perror("Erro na leitura");//unlock
                return -1;
            }

            session->s_buffer_count++;
            if (pthread_cond_signal(&session->s_cond) != 0) {
                perror("Erro no signal");//unlock
                return -1;
            }

            if (pthread_mutex_unlock(&session->s_mutex) != 0) {
                perror("Erro no unlock");
                return -1;
            }
        }

        if (tfs_op_code == TFS_OP_CODE_CLOSE) {
            printf("TFS_CLOSE on server\n");
            int session_id;
            if (read(serverFd, &session_id, sizeof(int)) == -1) {
                perror("Erro na leitura");
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

            memcpy(session->s_buffer ,&tfs_op_code, sizeof(char));
            memcpy(session->s_buffer + 1 ,&session_id, sizeof(int));
            if (read(serverFd, session->s_buffer + 5, sizeof(char) * 4) == -1) {
                perror("Erro na leitura");//unlock
                return -1;
            }

            session->s_buffer_count++;
            if (pthread_cond_signal(&session->s_cond) != 0) {
                perror("Erro no signal");//unlock
                return -1;
            }

            if (pthread_mutex_unlock(&session->s_mutex) != 0) {
                perror("Erro no unlock");
                return -1;
            }
        }

        if (tfs_op_code == TFS_OP_CODE_WRITE) {
            printf("TFS_WRITE on server\n");//to remove
            int session_id;
            size_t len;
            if (read(serverFd, &session_id, sizeof(int)) == -1) {
                perror("Erro na leitura");
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

            memcpy(session->s_buffer ,&tfs_op_code, sizeof(char));
            memcpy(session->s_buffer + 1,&session_id, sizeof(int));
            if (read(serverFd, session->s_buffer + 5, sizeof(int)) == -1) {
                perror("Erro na leitura");//unlock
                return -1;
            }

            if (read(serverFd, &len, sizeof(size_t)) == -1) {
                perror("Erro na leitura");
                return -1;
            }

            memcpy(session->s_buffer + 9, &len, sizeof(size_t));
            if (read(serverFd, session->s_buffer + 17, sizeof(char) * len) == -1) {
                perror("Erro na leitura");//unlock
                return -1;
            }

            session->s_buffer_count++;
            if (pthread_cond_signal(&session->s_cond) != 0) {
                perror("Erro no signal");//unlock
                return -1;
            }

            if (pthread_mutex_unlock(&session->s_mutex) != 0) {
                perror("Erro no unlock");
                return -1;
            }
        }

        if (tfs_op_code == TFS_OP_CODE_READ) {
            printf("TFS_READ on server\n");//to remove
            int session_id;
            if (read(serverFd, &session_id, sizeof(int)) == -1) {
                perror("Erro na leitura");
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

            memcpy(session->s_buffer ,&tfs_op_code, sizeof(char));
            memcpy(session->s_buffer + 1,&session_id, sizeof(int));
            if (read(serverFd, session->s_buffer + 5, sizeof(char) * 12) == -1) {
                perror("Erro na leitura");//unlock
                return -1;
            }

            session->s_buffer_count++;
            if (pthread_cond_signal(&session->s_cond) != 0) {
                perror("Erro no signal");//unlock
                return -1;
            }

            if (pthread_mutex_unlock(&session->s_mutex) != 0) {
                perror("Erro no unlock");
                return -1;
            }
        }



    }

    //close(serverFd);
    //unlink(pipename);

    return 0;
}