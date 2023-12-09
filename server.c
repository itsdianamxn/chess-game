
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <signal.h>
/* portul folosit */

#define PORT 27182
#define MAX_USERS 100000
#define MATRIX_SIZE 8
#define CLIENT_ID 10
#define GAME_STATE 20
#define MOVE 30
#define WHITE 1
#define BLACK -1
#define ROOK 1
#define KNIGHT 2
#define BISHOP 3
#define QUEEN 4
#define KING 5
#define PAWN 6

#define DISCONNECTED 0
#define WHITEW 1
#define DRAW 2
#define BLACKW 3

extern int errno; /* eroarea returnata de unele apeluri */

struct game_rooms
{
    int p1;
    int p2;
    int order_number;
    struct game_rooms *next;
} *first;

void add_client(int client_fd)
{
    if (first == NULL)
    {
        struct game_rooms *new_client = (struct game_rooms *)malloc(sizeof(struct game_rooms));
        new_client->order_number = 0;
        new_client->p1 = client_fd;
        new_client->p2 = -1;

        new_client->next = NULL;
        first = new_client;
    }
    else
    {
        struct game_rooms *t = first;
        int ok = 0;
        while (t->next != NULL)
        {
            if (t->p1 == -1)
            {
                t->p1 = client_fd;
                ok = 1;
                break;
            }
            if (t->p2 == -1)
            {
                t->p2 = client_fd;
                ok = 1;
                break;
            }
            t = t->next;
        }
        if (t->next == NULL)
        {
            if (t->p1 == -1)
            {
                t->p1 = client_fd;
                ok = 1;
            }
            else if (t->p2 == -1)
            {
                t->p2 = client_fd;
                ok = 1;
            }
        }
        if (!ok)
        {
            struct game_rooms *new_client = (struct game_rooms *)malloc(sizeof(struct game_rooms));
            new_client->order_number = t->order_number + 1;
            new_client->p1 = client_fd;
            new_client->p2 = -1;
            new_client->next = NULL;

            t->next = new_client;
        }
    }
}

void delete_room(int room_number)
{
    if (room_number == 0)
    {
        struct game_rooms *t = first;
        first = first->next;
        free(t);
    }
    else
    {
        struct game_rooms *t = first;
        while (t->next->order_number != room_number)
            t = t->next;
        struct game_rooms *p = t->next;

        t->next = p->next;
        free(p);
    }
}

/* functie de convertire a adresei IP a clientului in sir de caractere */
char *conv_addr(struct sockaddr_in address)
{
    static char str[25];
    char port[7];

    /* adresa IP a clientului */
    strcpy(str, inet_ntoa(address.sin_addr));
    /* portul utilizat de client */
    bzero(port, 7);
    sprintf(port, ":%d", ntohs(address.sin_port));
    strcat(str, port);
    return (str);
}

int game(int, int);
int sendMessage(int, int, void *, size_t);
/* programul */
int main()
{
    signal(SIGPIPE, SIG_IGN);
    struct sockaddr_in server; /* structurile pentru server si clienti */
    struct sockaddr_in from;
    fd_set readfds;    /* multimea descriptorilor de citire */
    fd_set actfds;     /* multimea descriptorilor activi */
    struct timeval tv; /* structura de timp pentru select() */
    int sd, client;    /* descriptori de socket */
    int optval = 1;    /* optiune folosita pentru setsockopt()*/
    int fd;            /* descriptor folosit pentru
                      parcurgerea listelor de descriptori */
    int nfds;          /* numarul maxim de descriptori */
    socklen_t len;     /* lungimea structurii sockaddr_in */

    /* creare socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server] Eroare la socket().\n");
        return errno;
    }

    /*setam pentru socket optiunea SO_REUSEADDR */
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    /* pregatim structurile de date */
    bzero(&server, sizeof(server));

    /* umplem structura folosita de server */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    /* atasam socketul */
    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server] Eroare la bind().\n");
        return errno;
    }

    /* punem serverul sa asculte daca vin clienti sa se conecteze */
    if (listen(sd, MAX_USERS) == -1)
    {
        perror("[server] Eroare la listen().\n");
        return errno;
    }

    /* completam multimea de descriptori de citire */
    FD_ZERO(&actfds);    /* initial, multimea este vida */
    FD_SET(sd, &actfds); /* includem in multime socketul creat */

    tv.tv_sec = 1; /* se va astepta un timp de 1 sec. */
    tv.tv_usec = 0;

    /* valoarea maxima a descriptorilor folositi */
    nfds = sd;

    printf("[server] Asteptam la portul %d...\n", PORT);
    fflush(stdout);

    /* servim in mod concurent (!?) clientii... */
    while (1)
    {
        /* ajustam multimea descriptorilor activi (efectiv utilizati) */
        bcopy((char *)&actfds, (char *)&readfds, sizeof(readfds));

        /* apelul select() */
        if (select(nfds + 1, &readfds, NULL, NULL, &tv) < 0)
        {
            perror("[server] Eroare la select().\n");
            return errno;
        }
        /* vedem daca e pregatit socketul pentru a-i accepta pe clienti */
        if (FD_ISSET(sd, &readfds))
        {
            /* pregatirea structurii client */
            len = sizeof(from);
            bzero(&from, sizeof(from));

            /* a venit un client, acceptam conexiunea */
            client = accept(sd, (struct sockaddr *)&from, &len);

            /* eroare la acceptarea conexiunii de la un client */
            if (client < 0)
            {
                perror("[server] Eroare la accept().\n");
                continue;
            }

            if (nfds < client) /* ajusteaza valoarea maximului */
                nfds = client;

            /* includem in lista de descriptori activi si acest socket */
            FD_SET(client, &actfds);

            printf("[server] S-a conectat clientul cu descriptorul %d, de la adresa %s.\n", client, conv_addr(from));
            fflush(stdout);
            add_client(client);
        }
        bcopy((char *)&actfds, (char *)&readfds, sizeof(readfds));
        struct game_rooms *t = first;
        while (t != NULL)
        {
            if (t->p1 != -1 && t->p2 != -1 && FD_ISSET(t->p1, &readfds) && FD_ISSET(t->p2, &readfds))
            {
                printf("Now playing game number %d.\n", t->order_number);
                if (game(t->p1, t->p2))
                {
                    fflush(stdout);
                    FD_CLR(t->p1, &actfds);
                    FD_CLR(t->p2, &actfds);
                    delete_room(t->order_number);
                }
            }
            t = t->next;
        }

    } /* while */
} /* main */

/* realizeaza primirea si retrimiterea unui mesaj unui client */

// void sendMatrix(int sd, int matrix[MATRIX_SIZE][MATRIX_SIZE])
// {
//     // Serialize and send the matrix
//     for (int i = 0; i < MATRIX_SIZE; i++)
//     {
//         if (write(sd, matrix[i], sizeof(int) * MATRIX_SIZE) <= 0)
//         {
//             perror("Error sending matrix");
//             exit(EXIT_FAILURE);
//         }
//     }
// }

int sendMessage(int fd, int msg_id, void *msg, size_t msg_size)
{

    printf("I'm gonna send msg_id: %d to client %d. \n", msg_id, fd);
    int bytes = write(fd, &msg_id, sizeof(int));
    printf("%d\n",bytes);
    if (bytes <= 0)
    {
        perror("[client] Error sending msg_id to client. \n");
        return errno;
    }
    bytes = write(fd, msg, msg_size);
    printf("%d\n",bytes);
    if (bytes <= 0)
    {
        perror("[client] Error sending message to client. \n");
        return errno;
    }
    return 0;
}

int is_game_finished(int game_board[8][8], int moves[4], int side)
{
    if(game_board[moves[0]][moves[1]] == PAWN * side)
    {
        if(side == WHITE && moves[2] == 0)
            return WHITEW;
        if(side == BLACK && moves[2] == 7)
            return BLACKW;
    }
    return 0;
}

void endGame(int loser_fd, int winner_fd, int state)
{
    printf("[server] Client disconnected with descriptor %d. Now it will disconnect with descriptor %d as well. \n", loser_fd, winner_fd);
    fflush(stdout);
    int exit_code = 0;
    if (sendMessage(winner_fd, GAME_STATE, &state, sizeof(state)))
    {
        close(loser_fd);
        close(winner_fd);
        exit_code = 1;
    }
    close(loser_fd);
    close(winner_fd);
    exit(exit_code);
}

int game(int fd1, int fd2)
{
    int buffer[4];
    int bytes;
    char side;

    side = '1';
    printf("Hey! Game started!\n");

    if (sendMessage(fd1, CLIENT_ID, &side, sizeof(side)))
    {
        close(fd1);
        close(fd2);
        return 1;
    }
    printf("[server] Side WHITE succesfully sent to %d. \n", fd1);

    side = '2';
    if (sendMessage(fd2, CLIENT_ID, &side, sizeof(side)))
    {
        close(fd1);
        close(fd2);
        return 1;
    }
    printf("[server] Side BLACK succesfully sent to %d. \n", fd2);

    pid_t child_pid;
    child_pid = fork();
    if (child_pid < 0)
    {
        close(fd1);
        close(fd2);
        perror("[server] Eroare la fork. \n");
        return 1;
    }
    else if (child_pid == 0)
    {
        int game_board[8][8] = {
        -1,
        -2,
        -3,
        -4,
        -5,
        -3,
        -2,
        -1,
        -6,
        -6,
        -6,
        -6,
        -6,
        -6,
        -6,
        -6,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        6,
        6,
        6,
        6,
        6,
        6,
        6,
        6,
        1,
        2,
        3,
        4,
        5,
        3,
        2,
        1,
    };
    
        int state;
        while (1)
        {
            printf("Waiting on a move from client1: \n");
            /// read move from side 1
            bytes = read(fd1, buffer, sizeof(int) * 4);

            /// check for error from
            if (bytes <= 0)
            {
                endGame(fd1,fd2,DISCONNECTED);
            }
            for (int i = 0; i < 4; i++)
                printf("%d, ", buffer[i]);
            printf("\n");

            state = is_game_finished(game_board, buffer, WHITE);
            printf("State 1->2: %d\n", state);
            ////make move
            int piece_moved = game_board[buffer[0]][buffer[1]];
            game_board[buffer[0]][buffer[1]] = 0;
            game_board[buffer[2]][buffer[3]] = piece_moved;

            /// send the state of the game
            if (sendMessage(fd2, MOVE, buffer, sizeof(buffer)))
            {
                    endGame(fd2,fd1,DISCONNECTED);
            }
            
            
            if (state)
            {
                if (sendMessage(fd1, GAME_STATE, &state, sizeof(state)))
                {
                    endGame(fd1,fd2,BLACKW);
                }
                if (sendMessage(fd2, GAME_STATE, &state, sizeof(state)))
                {
                    endGame(fd2,fd1,WHITEW);
                }
                endGame(fd2,fd1,state);
            }

            printf("Waiting on a move from client2: \n");
            /// read move from client 2
            bytes = read(fd2, buffer, sizeof(int) * 4);
            if (bytes <= 0)
            {
                endGame(fd2,fd1,WHITEW);
            }
            for (int i = 0; i < 4; i++)
                printf("%d, ", buffer[i]);
            printf("\n");
            piece_moved = game_board[buffer[0]][buffer[1]];
            game_board[buffer[0]][buffer[1]] = 0;
            game_board[buffer[2]][buffer[3]] = piece_moved;

            /// send state of the game
             if (sendMessage(fd1, MOVE, buffer, sizeof(buffer)))
                {
                    endGame(fd1,fd2,DISCONNECTED);
                }
            state = is_game_finished(game_board, buffer, BLACK);
            printf("State 2->1: %d\n", state);
            if (state)
            {
                if (sendMessage(fd1, GAME_STATE, &state, sizeof(state)))
                {
                    endGame(fd1,fd2,DISCONNECTED);
                }
                if (sendMessage(fd2, GAME_STATE, &state, sizeof(state)))
                {
                    endGame(fd2,fd1,DISCONNECTED);
                }
                endGame(fd1,fd2,state);
            }
            
                /// send move of client 2 to client 1
               
        }
        exit(0);
    }
    else
        return 1;
}
