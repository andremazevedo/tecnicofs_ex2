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

#define S (20)

/*
 * Session
 */
typedef struct {
    int s_id;
    char s_buffer[BLOCK_SIZE]; //qual devia ser o tamanho do buffer?
    pthread_mutex_t s_mutex;
    pthread_cond_t s_cond;
    int s_buffer_count;
    int s_clientFd;
} session_t;

typedef enum { AVAILABLE = 0, UNAVAILABLE = 1 } allocation_session_t;

static session_t session_table[S];
static char free_session[S];

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
 */
void server_init() {

    for (int i = 0; i < S; i++) {
        free_session[i] = AVAILABLE;
    }

    for (int i = 0; i < S; i++) {
        session_table[i].s_id = i;

        if (pthread_mutex_init(&session_table[i].s_mutex, NULL) != 0) {
            perror("Erro no pthread_mutex_init");
        }

        if (pthread_cond_init(&session_table[i].s_cond, NULL) != 0) {
            perror("Erro no pthread_cond_init");
        }

        session_table[i].s_buffer_count = 0;
    }

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

        if (tfs_op_code == TFS_OP_CODE_UNMOUNT) {
            printf("TFS_UNMOUNT on thread %d\n", session->s_id);//to remove
            int session_id;

            memcpy(&session_id, session->s_buffer + 1, sizeof(int));
            printf("session_id on thread %d: %d.\n", session->s_id, session_id);//to remove

            if (!valid_session(session_id) || free_session[session_id] == AVAILABLE) {
                int response = -1;
                if (write(session->s_clientFd, &response, sizeof(int)) == -1) {
                    pthread_mutex_unlock(&session->s_mutex);
                    perror("Erro ao escrever no pipe");
                    //exit(-1);
                }
            }

            int response = 0;
            printf("response on thread %d: %d.\n", session->s_id, response);//to remove
            if (write(session->s_clientFd, &response, sizeof(int)) == -1) {
                pthread_mutex_unlock(&session->s_mutex);
                perror("Erro ao escrever no pipe");
                //exit(-1);
            }

            close(session->s_clientFd);
            free_session[session->s_id] = AVAILABLE;
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
                pthread_mutex_unlock(&session->s_mutex);
                perror("Erro ao escrever no pipe");
                //exit(-1);
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
                pthread_mutex_unlock(&session->s_mutex);
                perror("Erro ao escrever no pipe");
                //exit(-1);
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
                pthread_mutex_unlock(&session->s_mutex);
                perror("Erro ao escrever no pipe");
                //exit(-1);
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
                    pthread_mutex_unlock(&session->s_mutex);
                    perror("Erro ao escrever no pipe");
                    //exit(-1);
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
    pthread_t tid[S];

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */
    tfs_init();
    server_init();

    for (int i = 0; i < S; i++) {
        if (pthread_create(&tid[i], NULL, fnThread, (void*)&session_table[i]) == -1) {
            perror("Erro no pthread_create");
        }
    }

    unlink(pipename);
    
    if (mkfifo (pipename, 0644) == -1) {
        perror("Erro no mkfifo");
        return -1;
    }

    if ((serverFd = open(pipename, O_RDONLY)) == -1) {
        perror("Erro ao abrir o pipe");
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);
    
    ssize_t r;
    char tfs_op_code;
    int session_id;
    for(;;) {

        if ((r = read(serverFd, &tfs_op_code, sizeof(tfs_op_code))) == -1) {
            perror("Erro na leitura");
            return -1;
        }

        if (r == 0) {
            printf("client disconnected\n");//to remove
            serverFd = open(pipename, O_RDONLY);
            continue;
        }

        printf("tfs_op_code on server: %d.\n", tfs_op_code);//to remove

        if (tfs_op_code == TFS_OP_CODE_MOUNT) {
            printf("TFS_MOUNT on server\n");//to remove
            int clientFd;
            char client_pipe_path[40];
            if (read(serverFd, client_pipe_path, sizeof(client_pipe_path)) == -1) {
                perror("Erro na leitura");
                return -1;
            }
            printf("client_pipe_path on server: %s.\n", client_pipe_path);//to remove

            if ((clientFd = open(client_pipe_path, O_WRONLY)) == -1) {
                perror("Erro ao abrir o pipe");
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
            printf("session_id on server: %d.\n", session_id);//to remove
            if (write (clientFd, &session_id, sizeof(int)) == -1) {
                perror("Erro ao escrever no pipe");
                return -1;
            }
            continue;
            //se nao houver sessoes disponiveis???????????????????
            //nao e necessario mutexes??????????
        } 
            
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
        
        if (tfs_op_code == TFS_OP_CODE_UNMOUNT) {
            printf("TFS_UNMOUNT on server\n");//to remove
        }
        
        if (tfs_op_code == TFS_OP_CODE_OPEN) {
            printf("TFS_OPEN on server\n");//to remove

            if (read(serverFd, session->s_buffer + 5, sizeof(char) * 44) == -1) {
                perror("Erro na leitura");//unlock
                return -1;
            }
        }

        if (tfs_op_code == TFS_OP_CODE_CLOSE) {
            printf("TFS_CLOSE on server\n");

            if (read(serverFd, session->s_buffer + 5, sizeof(char) * 4) == -1) {
                perror("Erro na leitura");//unlock
                return -1;
            }
        }

        if (tfs_op_code == TFS_OP_CODE_WRITE) {
            printf("TFS_WRITE on server\n");//to remove
            size_t len;

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
        }

        if (tfs_op_code == TFS_OP_CODE_READ) {
            printf("TFS_READ on server\n");//to remove

            if (read(serverFd, session->s_buffer + 5, sizeof(char) * 12) == -1) {
                perror("Erro na leitura");//unlock
                return -1;
            }
        }

        if (tfs_op_code == TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED) {
            printf("TFS_SHUTDOWN_AFTER_ALL_CLOSED on server\n");//to remove
            int response;
            response = tfs_destroy_after_all_closed();
            printf("response on server: %d.\n", response);//to remove
            if (write(session->s_clientFd, &response, sizeof(int)) == -1) {
                pthread_mutex_unlock(&session->s_mutex);
                perror("Erro ao escrever no pipe");
            }
            if (response != -1) {
                break;
            }
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

    close(serverFd);
    if (unlink(pipename) != 0) {
        perror("Erro no unlink");
        return -1;
    }
    printf("Server ended sucessfully!!!!");
    return 0;
}