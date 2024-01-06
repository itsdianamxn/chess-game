
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
#include <poll.h>
/* portul folosit */

#define PORT 27181
#define MAX_USERS 100000
#define MATRIX_SIZE 8
#define CLIENT_ID 10
#define GAME_STATE 20
#define MOVE 30
#define WRONG_MOVE 40
#define WRONG_MOVE_CHECK 50
#define CORRECT_MOVE 60

#define WHITE 1
#define BLACK -1

#define WHITE_CHECK 1
#define BLACK_CHECK -1
#define DOUBLE_CHECK 2

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
#define GAME_GOING 4

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

int is_fd_inactive(int fd)
{
    struct pollfd fds[1];
    fds[0].fd = fd;
    fds[0].events = POLLIN;

    int ret = poll(fds, 1, 0);
    if (ret == -1)
    {
        return 0;
    }

    return ret > 0;
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
    bcopy((char *)&actfds, (char *)&readfds, sizeof(readfds));
    /* servim in mod concurent (!?) clientii... */
    while (1)
    {
        /* ajustam multimea descriptorilor activi (efectiv utilizati) */

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
            if (t->p1 != -1 && t->p2 == -1 && FD_ISSET(t->p1, &readfds))
            {
                if (is_fd_inactive(t->p1))
                {
                    FD_CLR(t->p1, &actfds);
                    FD_CLR(t->p2, &actfds);
                    delete_room(t->order_number);
                }
            }
            else if (t->p1 == -1 && t->p2 != -1 && FD_ISSET(t->p2, &readfds))
            {
                if (is_fd_inactive(t->p2))
                {
                    FD_CLR(t->p1, &actfds);
                    FD_CLR(t->p2, &actfds);
                    delete_room(t->order_number);
                }
            }
            else if (t->p1 != -1 && t->p2 != -1 && FD_ISSET(t->p1, &readfds) && FD_ISSET(t->p2, &readfds))
            {
                pid_t child = fork();
                if (child < 0)
                {
                    perror("[server] fork error.\n");
                    continue;
                }
                else if (child == 0)
                {
                    printf("Now playing game number %d.\n", t->order_number);
                    int winner = game(t->p1, t->p2);
                    switch (winner)
                    {
                    case WHITEW:
                        printf("Game number %d ended with player %d as a winner!\n", t->order_number, t->p1);
                        delete_room(t->order_number);
                        exit(0);
                    case DRAW:
                        printf("Game number %d ended in a draw!\n", t->order_number);
                        delete_room(t->order_number);
                        exit(0);
                    case BLACKW:
                        printf("Game number %d ended with player %d as a winner!\n", t->order_number, t->p2);
                        delete_room(t->order_number);
                        exit(0);
                    default:
                        printf("Error in game number %d. \n", t->order_number);
                        delete_room(t->order_number);
                        exit(-1);
                    }
                }
                else
                {
                    FD_CLR(t->p1, &actfds);
                    FD_CLR(t->p2, &actfds);
                }
            }
            t = t->next;
        }

    } /* while */
} /* main */

int sendMessage(int fd, int msg_id, void *msg, size_t msg_size)
{

    printf("I'm gonna send msg_id: %d to client %d. \n", msg_id, fd);
    int bytes = write(fd, &msg_id, sizeof(int));

    if (bytes <= 0)
    {
        perror("[client] Error sending msg_id to client. \n");
        return errno;
    }
    bytes = write(fd, msg, msg_size);

    if (bytes <= 0)
    {
        perror("[client] Error sending message to client. \n");
        return errno;
    }
    return 0;
}

void update_check_value(int i, int j, int check_board[8][8], int side)
{
    if (!(i < 0 || i > 7 || j < 0 || j > 7))
    {
        if (!check_board[i][j])
            check_board[i][j] = side;
        else if (check_board[i][j] != side)
            check_board[i][j] = DOUBLE_CHECK;
    }
}

void update_check_rook(int i, int j, int check_board[8][8], int game_board[8][8], int side)
{
    if (i != 7)
    {
        for (int k = i + 1; k < 8; k++)
        {
            update_check_value(k, j, check_board, side);
            if (game_board[k][j])
                break;
        }
    }
    if (i != 0)
    {
        for (int k = i - 1; k >= 0; k--)
        {
            update_check_value(k, j, check_board, side);
            if (game_board[k][j])
                break;
        }
    }
    if (j != 7)
    {
        for (int k = j + 1; k < 8; k++)
        {
            update_check_value(i, k, check_board, side);
            if (game_board[i][k])
                break;
        }
    }
    if (j != 0)
    {
        for (int k = j - 1; k >= 0; k--)
        {
            update_check_value(i, k, check_board, side);
            if (game_board[i][k])
                break;
        }
    }
}

void update_check_knight(int i, int j, int check_board[8][8], int game_board[8][8], int side)
{
    update_check_value(i + 2, j + 1, check_board, side);
    update_check_value(i + 2, j - 1, check_board, side);
    update_check_value(i - 2, j + 1, check_board, side);
    update_check_value(i - 2, j - 1, check_board, side);
    update_check_value(i + 1, j + 2, check_board, side);
    update_check_value(i + 1, j - 2, check_board, side);
    update_check_value(i - 1, j + 2, check_board, side);
    update_check_value(i - 1, j - 2, check_board, side);
}

void update_check_bishop(int i, int j, int check_board[8][8], int game_board[8][8], int side)
{
    for (int x = i + 1, y = j + 1; x < 8, y < 8; x++, y++)
    {
        update_check_value(x, y, check_board, side);
        if (game_board[x][y])
            break;
    }
    for (int x = i + 1, y = j - 1; x < 8, y >= 0; x++, y--)
    {
        update_check_value(x, y, check_board, side);
        if (game_board[x][y])
            break;
    }
    for (int x = i - 1, y = j + 1; x >= 0, y < 8; x--, y++)
    {
        update_check_value(x, y, check_board, side);
        if (game_board[x][y])
            break;
    }
    for (int x = i - 1, y = j - 1; x >= 0, y >= 0; x--, y--)
    {
        update_check_value(x, y, check_board, side);
        if (game_board[x][y])
            break;
    }
}

void update_check_king(int i, int j, int check_board[8][8], int game_board[8][8], int side)
{
    update_check_value(i + 1, j, check_board, side);
    update_check_value(i + 1, j - 1, check_board, side);
    update_check_value(i + 1, j + 1, check_board, side);
    update_check_value(i - 1, j, check_board, side);
    update_check_value(i - 1, j + 1, check_board, side);
    update_check_value(i - 1, j - 1, check_board, side);
    update_check_value(i, j + 1, check_board, side);
    update_check_value(i, j - 1, check_board, side);
}

void update_check_pawn(int i, int j, int check_board[8][8], int game_board[8][8], int side)
{
    if (side == BLACK)
    {
        update_check_value(i + 1, j - 1, check_board, side);
        update_check_value(i + 1, j + 1, check_board, side);
    }
    else
    {
        update_check_value(i - 1, j - 1, check_board, side);
        update_check_value(i - 1, j + 1, check_board, side);
    }
}

void update_check_board(int game_board[8][8], int check_board[8][8])
{
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (!game_board[i][j])
                continue;
            int side = game_board[i][j] < 0 ? -1 : 1;
            switch (abs(game_board[i][j]))
            {
            case ROOK:
                update_check_rook(i, j, check_board, game_board, side);
                break;
            case KNIGHT:
                update_check_knight(i, j, check_board, game_board, side);
                break;
            case BISHOP:
                update_check_bishop(i, j, check_board, game_board, side);
                break;
            case QUEEN:
                update_check_bishop(i, j, check_board, game_board, side);
                update_check_rook(i, j, check_board, game_board, side);
                break;
            case KING:
                update_check_king(i, j, check_board, game_board, side);
                break;
            case PAWN:
                update_check_pawn(i, j, check_board, game_board, side);
                break;
            default:
                break;
            }
        }
    }
}

bool is_rook_move_legal(int game_board[8][8], int moves[4], int side)
{
    if (moves[0] == moves[2])
    {
        if (moves[3] < moves[1])
        {
            for (int i = moves[1] + 1; i < moves[3]; i++)
                if (game_board[moves[0]][i] != 0)
                    return false;
        }
        else
        {
            for (int i = moves[3] + 1; i < moves[1]; i++)
                if (game_board[moves[0]][i] != 0)
                    return false;
        }
    }
    else if (moves[1] == moves[3])
    {
        if (moves[2] < moves[0])
        {
            for (int i = moves[2] + 1; i < moves[0]; i++)
                if (game_board[i][moves[1]] != 0)
                    return false;
        }
        else
        {
            for (int i = moves[0] + 1; i < moves[2]; i++)
                if (game_board[i][moves[1]] != 0)
                    return false;
        }
    }
    else
        return false;
    return true;
}

bool is_knight_move_legal(int game_board[8][8], int moves[4], int side)
{
    if (moves[2] == moves[0] - 2 && moves[3] == moves[1] - 1)
        return true;
    if (moves[2] == moves[0] + 2 && moves[3] == moves[1] - 1)
        return true;
    if (moves[2] == moves[0] - 2 && moves[3] == moves[1] + 1)
        return true;
    if (moves[2] == moves[0] + 2 && moves[3] == moves[1] + 1)
        return true;
    if (moves[3] == moves[1] - 2 && moves[2] == moves[0] - 1)
        return true;
    if (moves[3] == moves[1] + 2 && moves[2] == moves[0] - 1)
        return true;
    if (moves[3] == moves[1] - 2 && moves[2] == moves[0] + 1)
        return true;
    if (moves[3] == moves[1] + 2 && moves[2] == moves[0] + 1)
        return true;
    return false;
}

bool is_bishop_move_legal(int game_board[8][8], int moves[4], int side)
{
    if (abs(moves[2] - moves[0]) != abs(moves[3] - moves[1])) // checks if it's a diagonal move
        return false;
    if (moves[0] < moves[2])
    {
        if (moves[1] < moves[3])
        {
            for (int i = moves[2] - 1, j = moves[3] - 1; i > moves[0], j > moves[1]; i--, j--)
                if (game_board[i][j] != 0)
                    return false;
        }
        else
        {
            for (int i = moves[2] - 1, j = moves[3] + 1; i > moves[0], j < moves[1]; i--, j++)
                if (game_board[i][j] != 0)
                    return false;
        }
    }
    else
    {
        if (moves[1] < moves[3])
        {
            for (int i = moves[2] + 1, j = moves[3] - 1; i<moves[0], j> moves[1]; i++, j--)
                if (game_board[i][j] != 0)
                    return false;
        }
        else
        {
            for (int i = moves[2] + 1, j = moves[3] + 1; i < moves[0], j < moves[1]; i++, j++)
                if (game_board[i][j] != 0)
                    return false;
        }
    }
    return true;
}

bool is_queen_move_legal(int game_board[8][8], int moves[4], int side)
{
    return is_rook_move_legal(game_board, moves, side) || is_bishop_move_legal(game_board, moves, side);
}

bool is_king_move_legal(int game_board[8][8], int moves[4], int side)
{
    if (abs(moves[0] - moves[2]) > 1 || abs(moves[1] - moves[3]) > 1)
        return false;
    return true;
}

bool is_pawn_move_legal(int game_board[8][8], int moves[4], int side)
{
    int startRow = moves[0];
    int startCol = moves[1];
    int endRow = moves[2];
    int endCol = moves[3];

    int direction = (side == WHITE) ? 1 : -1;

    // Check for valid array indices
    if (startRow < 0 || startRow >= 8 || startCol < 0 || startCol >= 8 ||
        endRow < 0 || endRow >= 8 || endCol < 0 || endCol >= 8)
    {
        return false;
    }

    // No capture
    if (startCol == endCol)
    {
        if (startRow == 6 && endRow == 5 && game_board[5][endCol] == 0)
            return true;
        if (startRow == 1 && endRow == 2 && game_board[2][endCol] == 0)
            return true;
        if (startRow - endRow == direction && game_board[endRow][endCol] == 0)
            return true;
        if (startRow == 6 && endRow == 4 && game_board[5][endCol] == 0 && game_board[4][endCol] == 0)
            return true;
        if (startRow == 1 && endRow == 3 && game_board[2][endCol] == 0 && game_board[3][endCol] == 0)
            return true;
    }
    // Capture
    else if (abs(endCol - startCol) == 1 && startRow - endRow == direction)
    {
        if (game_board[endRow][endCol] * side < 0)
            return true;
    }
    return false;
}

bool check_if_there_is_check(int game_board[8][8], int check_board[8][8], int moves[4], int side)
{
    int dummy_board[8][8];
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
            dummy_board[i][j] = game_board[i][j];
    }
    int piece = dummy_board[moves[0]][moves[1]];
    dummy_board[moves[0]][moves[1]] = 0;
    dummy_board[moves[2]][moves[3]] = piece;
    int dummy_check[8][8] = {0};
    update_check_board(dummy_board, dummy_check);
    // printf("Move of %d\n", side);
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
            if (dummy_board[i][j] == KING * side)
            {
                // printf("King on position %d %d\n", i, j);
                // printf("Dummy_check: %d\n", dummy_check[i][j]);
                if (dummy_check[i][j] == side * (-1) || dummy_check[i][j] == 2)
                    return false;
                else
                    return true;
            }
    }
}

bool is_castling_move(int game_board[8][8], int moves[4], int side)
{
    if (side == WHITE)
    {
        if (moves[0] == 7 && moves[1] == 4 && moves[2] == 7)
        {
            if (moves[3] == 6 || moves[3] == 2)
                return true;
        }
    }
    else
    {
        if (moves[0] == 0 && moves[1] == 4 && moves[2] == 0)
        {
            if (moves[3] == 6 || moves[3] == 2)
                return true;
        }
    }
    return false;
}

bool is_castling_legal(int game_board[8][8], int check_board[8][8], int moves[4], int side)
{
    if (side == WHITE)
    {
        if (moves[3] == 6 && (check_board[7][5] == 0 || check_board[7][5] == WHITE_CHECK) && game_board[7][7] == ROOK * WHITE)
            return true;
        if (moves[3] == 2 && (check_board[7][3] == 0 || check_board[7][3] == WHITE_CHECK) && game_board[7][0] == ROOK * WHITE)
            return true;
    }
    else
    {
        if (moves[3] == 6 && (check_board[0][5] == 0 || check_board[0][5] == BLACK_CHECK) && game_board[0][7] == ROOK * BLACK)
            return true;
        if (moves[3] == 2 && (check_board[0][3] == 0 || check_board[0][3] == BLACK_CHECK) && game_board[0][0] == ROOK * BLACK)
            return true;
    }
    return false;
}

bool is_move_legal(int game_board[8][8], int check_board[8][8], int moves[4], int side) /// 0 - x_start, 1 - y_start , 2-x_fin, 3-y_fin
{
    if (moves[0] == moves[2] && moves[1] == moves[3]) // check if the piece has not been moved
        return false;
    if (game_board[moves[2]][moves[3]] * side > 0) // check if the piece is trying to eat it's own kind
        return false;
    if (moves[2] < 0 || moves[2] > 7 || moves[3] < 0 || moves[3] > 7)
        return false;

    if (!check_if_there_is_check(game_board, check_board, moves, side))
        return false;

    switch (abs(game_board[moves[0]][moves[1]]))
    {
    case ROOK:
        return is_rook_move_legal(game_board, moves, side);

    case KNIGHT:
        return is_knight_move_legal(game_board, moves, side);

    case BISHOP:
        return is_bishop_move_legal(game_board, moves, side);

    case QUEEN:
        return is_queen_move_legal(game_board, moves, side);

    case KING:
        if (is_castling_move(game_board, moves, side))
            return is_castling_legal(game_board, check_board, moves, side);
        else
            return is_king_move_legal(game_board, moves, side);

    case PAWN:
        return is_pawn_move_legal(game_board, moves, side);
    default:
        break;
    }
    return false;
}

bool check_rook_moves(int game_board[8][8], int check_board[8][8], int i, int j, int side)
{
    int moves[4];
    moves[0] = i;
    moves[1] = j;
    moves[3] = j;
    for (int x = i + 1; x < 8; x++)
    {
        moves[2] = x;
        if (is_move_legal(game_board, check_board, moves, side))
            return true;
        if (game_board[x][j])
            break;
    }
    for (int x = i - 1; x >= 0; x--)
    {
        moves[2] = x;
        if (is_move_legal(game_board, check_board, moves, side))
            return true;
        if (game_board[x][j])
            break;
    }
    moves[2] = i;
    for (int y = j + 1; j < 8; j++)
    {
        moves[3] = y;
        if (is_move_legal(game_board, check_board, moves, side))
            return true;
        if (game_board[i][y])
            break;
    }
    for (int y = j - 1; j >= 0; j--)
    {
        moves[3] = y;
        if (is_move_legal(game_board, check_board, moves, side))
            return true;
        if (game_board[i][y])
            break;
    }
    return false;
}

bool check_knight_moves(int game_board[8][8], int check_board[8][8], int i, int j, int side)
{
    int moves[4];
    moves[0] = i;
    moves[1] = j;
    moves[2] = i + 2;
    moves[3] = j + 1;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i + 2;
    moves[3] = j - 1;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i - 2;
    moves[3] = j + 1;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i - 2;
    moves[3] = j - 1;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i + 1;
    moves[3] = j + 2;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i + 1;
    moves[3] = j - 2;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i - 1;
    moves[3] = j + 2;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i - 1;
    moves[3] = j - 2;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    return false;
}

bool check_bishop_moves(int game_board[8][8], int check_board[8][8], int i, int j, int side)
{
    int moves[4];
    moves[0] = i;
    moves[1] = j;
    for (int x = i - 1, y = j - 1; x >= 0, y >= 0; x--, y--)
    {
        moves[2] = x;
        moves[3] = y;
        if (is_move_legal(game_board, check_board, moves, side))
            return true;
        if (game_board[x][y])
            break;
    }
    for (int x = i - 1, y = j + 1; x >= 0, y < 8; x--, y++)
    {
        moves[2] = x;
        moves[3] = y;
        if (is_move_legal(game_board, check_board, moves, side))
            return true;
        if (game_board[x][y])
            break;
    }
    for (int x = i + 1, y = j - 1; x < 8, y >= 0; x++, y--)
    {
        moves[2] = x;
        moves[3] = y;
        if (is_move_legal(game_board, check_board, moves, side))
            return true;
        if (game_board[x][y])
            break;
    }
    for (int x = i + 1, y = j + 1; x < 8, y < 8; x++, y++)
    {
        moves[2] = x;
        moves[3] = y;
        if (is_move_legal(game_board, check_board, moves, side))
            return true;
        if (game_board[x][y])
            break;
    }
    return false;
}

bool check_king_moves(int game_board[8][8], int check_board[8][8], int i, int j, int side)
{
    int moves[4];
    moves[0] = i;
    moves[1] = j;

    moves[2] = i + 1;
    moves[3] = j + 1;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i + 1;
    moves[3] = j - 1;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i + 1;
    moves[3] = j;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i - 1;
    moves[3] = j + 1;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i - 1;
    moves[3] = j;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i - 1;
    moves[3] = j - 1;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i;
    moves[3] = j + 1;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    moves[2] = i;
    moves[3] = j - 1;
    if (is_move_legal(game_board, check_board, moves, side))
        return true;
    if(side == WHITE)
    {
        if(i == 7 && j == 4)
        {
            moves[2] = i;
            moves[3] = j + 2;
            if(is_castling_legal(game_board,check_board,moves,side))
                return true;
            moves[3] = j - 2;
            if(is_castling_legal(game_board,check_board,moves,side))
                return true;
        }
    }
    else
    {
        if(i == 0 && j == 4)
        {
            moves[2] = i;
            moves[3] = j + 2;
            if(is_castling_legal(game_board,check_board,moves,side))
                return true;
            moves[3] = j - 2;
            if(is_castling_legal(game_board,check_board,moves,side))
                return true;
        }
    }
    return false;
}

bool check_pawn_moves(int game_board[8][8], int check_board[8][8], int i, int j, int side)
{
    int moves[4];
    moves[0] = i;
    moves[1] = j;
    if (side == WHITE)
    {
        moves[2] = i - 1;
        moves[3] = j;
        if (is_move_legal(game_board, check_board, moves, side))
            return true;

        if (i == 6)
        {
            moves[2] = i - 2;
            moves[3] = j;
            if (is_move_legal(game_board, check_board, moves, side))
                return true;
        }
        if (game_board[i - 1][j - 1] * side < 0)
        {
            moves[2] = i - 1;
            moves[3] = j - 1;
            if (is_move_legal(game_board, check_board, moves, side))
                return true;
        }
        if (game_board[i - 1][j + 1] * side < 0)
        {
            moves[2] = i - 1;
            moves[3] = j + 1;
            if (is_move_legal(game_board, check_board, moves, side))
                return true;
        }
    }
    else
    {
        moves[2] = i + 1;
        moves[3] = j;
        if (is_move_legal(game_board, check_board, moves, side))
            return true;

        if (i == 1)
        {
            moves[2] = i + 2;
            moves[3] = j;
            if (is_move_legal(game_board, check_board, moves, side))
                return true;
        }
        if (game_board[i + 1][j - 1] * side < 0)
        {
            moves[2] = i + 1;
            moves[3] = j - 1;
            if (is_move_legal(game_board, check_board, moves, side))
                return true;
        }
        if (game_board[i + 1][j + 1] * side < 0)
        {
            moves[2] = i + 1;
            moves[3] = j + 1;
            if (is_move_legal(game_board, check_board, moves, side))
                return true;
        }
    }
    return false;
}

int is_game_finished(int game_board[8][8], int check_board[8][8], int side)
{
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
        {
            if (game_board[i][j] * side <= 0)
                continue;
            if (game_board[i][j] * side > 0)
            {
                int moves[4];
                moves[0] = i;
                moves[1] = j;
                switch (abs(game_board[i][j]))
                {
                case ROOK:
                    if (check_rook_moves(game_board, check_board, i, j, side))
                        return true;
                    break;
                case KNIGHT:
                    if (check_knight_moves(game_board, check_board, i, j, side))
                        return true;
                    break;
                case BISHOP:
                    if (check_bishop_moves(game_board, check_board, i, j, side))
                        return true;
                    break;
                case QUEEN:
                    if (check_bishop_moves(game_board, check_board, i, j, side))
                        return true;
                    if (check_rook_moves(game_board, check_board, i, j, side))
                        return true;
                    break;
                case KING:
                    if (check_king_moves(game_board, check_board, i, j, side))
                        return true;
                    break;
                case PAWN:
                    if (check_pawn_moves(game_board, check_board, i, j, side))
                        return true;
                    break;
                default:
                    break;
                }
            }
        }
    return false;
}

int get_game_state(int game_board[8][8], int check_board[8][8], int side)
{
    int state = is_game_finished(game_board, check_board, side);
    if (state == true)
        return GAME_GOING;
    else
    {
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                if (game_board[i][j] == KING * side)
                {
                    if (check_board[i][j] == side * -1 || check_board[i][j] == 2)
                    {
                        if (side == WHITE)
                            return BLACKW;
                        else
                            return WHITEW;
                    }
                    else
                        return DRAW;
                }
            }
        }
    }
}

void endGame(int loser_fd, int winner_fd, int state)
{
    printf("[server] Client disconnected with descriptor %d. Now it will disconnect with descriptor %d as well. \n", loser_fd, winner_fd);
    fflush(stdout);

    sendMessage(winner_fd, GAME_STATE, &state, sizeof(state));
    if (state != DISCONNECTED)
        sendMessage(loser_fd, GAME_STATE, &state, sizeof(state));
    close(loser_fd);
    close(winner_fd);
}

int game(int fd1, int fd2)
{
    int buffer[4];
    int bytes;
    char side;

    side = '1';

    if (sendMessage(fd1, CLIENT_ID, &side, sizeof(side)))
    {
        close(fd1);
        close(fd2);
        return -1;
    }
    printf("[server] Side WHITE succesfully sent to %d. \n", fd1);

    side = '2';
    if (sendMessage(fd2, CLIENT_ID, &side, sizeof(side)))
    {
        close(fd1);
        close(fd2);
        return -1;
    }
    printf("[server] Side BLACK succesfully sent to %d. \n", fd2);

    int check_board[8][8] = {0};
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

    update_check_board(game_board, check_board);

    int state;
    while (1)
    {
        printf("Waiting on a move from client1: \n");
        /// read move from side 1
        bytes = read(fd1, buffer, sizeof(int) * 4);

        /// check for error from
        if (bytes <= 0)
        {
            endGame(fd1, fd2, DISCONNECTED);
            return BLACKW;
        }

        // printf("Move legal?: %d \n", is_move_legal(game_board, buffer, WHITE));
        bool legal = is_move_legal(game_board, check_board, buffer, WHITE);
        // printf("is_move_legal: %d and piece: %d\n", legal, game_board[buffer[0]][buffer[1]]);
        while (!legal)
        {
            if (check_if_there_is_check(game_board, check_board, buffer, WHITE))
            {
                if (sendMessage(fd1, WRONG_MOVE, buffer, sizeof(buffer)))
                {
                    endGame(fd1, fd2, DISCONNECTED);
                    return BLACKW;
                }
            }
            else if (sendMessage(fd1, WRONG_MOVE_CHECK, buffer, sizeof(buffer)))
            {
                endGame(fd1, fd2, DISCONNECTED);
                return BLACKW;
            }
            printf("Waiting on a move from client1: \n");
            /// read move from side 1
            bytes = read(fd1, buffer, sizeof(int) * 4);

            /// check for error from
            if (bytes <= 0)
            {
                endGame(fd1, fd2, DISCONNECTED);
                return BLACKW;
            }
            legal = is_move_legal(game_board, check_board, buffer, WHITE);
            printf("is_move_legal: %d\n", legal);
        }
        if(sendMessage(fd1, CORRECT_MOVE, buffer, sizeof(buffer)))
        {
            endGame(fd1, fd2, DISCONNECTED);
            return BLACKW;
        }

        int piece_moved = game_board[buffer[0]][buffer[1]];
        game_board[buffer[0]][buffer[1]] = 0;
        game_board[buffer[2]][buffer[3]] = piece_moved;
        if (abs(piece_moved) == PAWN)
        {
            if (piece_moved * WHITE > 0 && buffer[2] == 0)
                game_board[buffer[2]][buffer[3]] = QUEEN * WHITE;
        }
        else if(abs(piece_moved) == KING)
        {
            if(is_castling_move(game_board,buffer,WHITE))
            {
                if(buffer[3] == 6)
                {
                    game_board[7][5] = ROOK * WHITE;
                    game_board[7][7] = 0;
                }
                else
                {
                    game_board[7][3] = ROOK * WHITE;
                    game_board[7][0] = 0;
                }
                for(int i = 0; i < 8; i++)
                {
                    for(int j = 0; j < 8; j++)
                    {
                        printf("%d ",game_board[i][j]);
                    }
                    printf("\n");
                }
            }
        }
        update_check_board(game_board, check_board);
        state = get_game_state(game_board, check_board, BLACK);

        
        if (sendMessage(fd2, MOVE, buffer, sizeof(buffer)))
        {
            endGame(fd2, fd1, DISCONNECTED);
            return WHITEW;
        }

        if (state != GAME_GOING)
        {
            if (sendMessage(fd1, GAME_STATE, &state, sizeof(state)))
            {
                endGame(fd1, fd2, DISCONNECTED);
            }
            if (sendMessage(fd2, GAME_STATE, &state, sizeof(state)))
            {
                endGame(fd2, fd1, DISCONNECTED);
            }
            endGame(fd2, fd1, state);
            return state;
        }

        printf("Waiting on a move from client2: \n");
        bytes = read(fd2, buffer, sizeof(int) * 4);
        if (bytes <= 0)
        {
            endGame(fd2, fd1, DISCONNECTED);
            return WHITEW;
        }
        legal = is_move_legal(game_board, check_board, buffer, BLACK);
        printf("is_move_legal: %d\n", legal);
        while (!legal)
        {
            if (check_if_there_is_check(game_board, check_board, buffer, BLACK))
            {
                if (sendMessage(fd2, WRONG_MOVE, buffer, sizeof(buffer)))
                {
                    endGame(fd2, fd1, DISCONNECTED);
                    return WHITEW;
                }
            }
            else if (sendMessage(fd2, WRONG_MOVE_CHECK, buffer, sizeof(buffer)))
            {
                endGame(fd2, fd1, DISCONNECTED);
                return WHITEW;
            }
            printf("Waiting on a move from client2: \n");
            bytes = read(fd2, buffer, sizeof(int) * 4);
            if (bytes <= 0)
            {
                endGame(fd2, fd1, DISCONNECTED);
                return WHITEW;
            }
            legal = is_move_legal(game_board, check_board, buffer, BLACK);
            printf("is_move_legal: %d\n", legal);
        }
        if(sendMessage(fd2, CORRECT_MOVE, buffer, sizeof(buffer)))
        {
            endGame(fd2, fd1, DISCONNECTED);
            return WHITEW;
        }
        piece_moved = game_board[buffer[0]][buffer[1]];
        game_board[buffer[0]][buffer[1]] = 0;
        game_board[buffer[2]][buffer[3]] = piece_moved;
        if (abs(piece_moved) == PAWN)
        {
            if (piece_moved * BLACK > 0 && buffer[2] == 7)
                game_board[buffer[2]][buffer[3]] = QUEEN * BLACK;
        }
        else if(abs(piece_moved) == KING)
        {
            if(is_castling_move(game_board,buffer,BLACK))
            {
                if(buffer[3] == 6)
                {
                    game_board[0][5] = ROOK * BLACK;
                    game_board[0][7] = 0;
                }
                else
                {
                    game_board[0][3] = ROOK * BLACK;
                    game_board[0][0] = 0;
                }
                for(int i = 0; i < 8; i++)
                {
                    for(int j = 0; j < 8; j++)
                    {
                        printf("%d ",game_board[i][j]);
                    }
                    printf("\n");
                }
            }
        }
        update_check_board(game_board, check_board);
        state = get_game_state(game_board, check_board, WHITE);

        if (sendMessage(fd1, MOVE, buffer, sizeof(buffer)))
        {
            endGame(fd1, fd2, DISCONNECTED);
            return BLACKW;
        }
        if (state != GAME_GOING)
        {
            if (sendMessage(fd1, GAME_STATE, &state, sizeof(state)))
            {
                endGame(fd1, fd2, DISCONNECTED);
            }
            if (sendMessage(fd2, GAME_STATE, &state, sizeof(state)))
            {
                endGame(fd2, fd1, DISCONNECTED);
            }
            endGame(fd1, fd2, state);
            return state;
        }
    }
}
