#include <QCoreApplication>
#include <QThread>
#include <QMessageBox>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include "communications.h"

communications::communications(const char* ip, int port, int* board, QWidget *parent)
    : QThread{parent}, board(board), port(port)
{
    serverIp = strdup(ip);
    start();
}

void communications::readSide()
{
    char blackOrWhite;
    if (read(sd, &blackOrWhite, sizeof(char)) <= 0)
    {
        emit serverNotification(QMessageBox::Critical, tr("Conenction Error"), tr("Couldn't receive my number"), QMessageBox::Ok);
        return;
    }
    first = blackOrWhite == '1';

    qDebug("Im %c, my server is: %d", blackOrWhite, sd);
    begun = true;
    emit startGame();
}

void communications::readMove()
{
    struct
    {
        int x1, y1, x2, y2;
    } move;
    int piece = NOPIECE;
    if (read(sd, &move, sizeof(int)*4) <= 0)
    {
        emit serverNotification(QMessageBox::Critical, tr("Conenction Error"), tr("Couldn't receive opponent move"), QMessageBox::Ok);
        return;
    }

    piece = board[move.x1*8+move.y1];
    // adjust for promotion (pawn to queen)
    if (piece == (BLACK + PAWN) && move.x2 == 7)
        piece = BLACK + QUEEN;
    if (piece == (WHITE + PAWN) && move.x2 == 0)
        piece = WHITE + QUEEN;
    board[move.x1*8+move.y1] = NOPIECE;
    board[move.x2*8+move.y2] = piece;

    if (piece == BLACK + KING || piece == WHITE + KING)
    {
        if (abs(move.y1 - move.y2) == 2)
        {
            if (move.y2 == 6) // right castling
            {
                board[move.x2*8+move.y2-1] = board[move.x1*8+7];
                board[move.x1*8+7] = NOPIECE;
            }
            else // y2 == 2, left castling
            {
                board[move.x2*8+move.y2+1] = board[move.x1*8+0];
                board[move.x1*8+0] = NOPIECE;

            }
        }
    }

    qDebug("MOVE read: %d %c%c %c%c", piece, move.y1+'A', (7- move.x1) + '1',  move.y2+'A', (7-move.x2) + '1');

    emit boardUpdate();
}
void communications::readState()
{
    int state;
    if (read(sd, &state, sizeof(int)) <= 0)
    {
        emit serverNotification(QMessageBox::Critical, tr("Conenction Error"), tr("Couldn't receive my number"), QMessageBox::Ok);
        return;
    }

    qDebug("State read: %d", state );
    if (state == P_DISCONNECTED)
    {
        emit serverNotification(QMessageBox::Critical, tr("Game finished"), tr("Opponent disconected :(<br>You win!"), QMessageBox::Ok);
        return;
    }
    if (state == WHITE_WIN)
    {
        emit serverNotification(QMessageBox::Critical, tr("Game finished"), tr("White wins!"), QMessageBox::Ok);
        return;
    }
    if (state == BLACK_WIN)
    {
        emit serverNotification(QMessageBox::Critical, tr("Game finished"), tr("Black wins!"), QMessageBox::Ok);
        return;
    }
    if (state == DRAW)
    {
        emit serverNotification(QMessageBox::Critical, tr("Draw"), tr("It's a draw"), QMessageBox::Ok);
        return;
    }
    return;
}

void communications::rollback()
{
    struct
    {
        int x1, y1, x2, y2;
    } move;
    if (read(sd, &move, sizeof(int)*4) <= 0)
    {
        emit serverNotification(QMessageBox::Critical, tr("Conenction Error"), tr("Couldn't receive rollbck message"), QMessageBox::Ok);
        return;
    }
    int piece = board[move.x2*8+move.y2];
    board[move.x1*8+move.y1] = piece;
    board[move.x2*8+move.y2] = ex_piece;
   // qDebug("ROLLBACK read: %s %c%c %c%c", piece, move.y1+'A', (7- move.x1) + '1',  move.y2+'A', (7-move.x2) + '1');

    emit serverNotification(QMessageBox::Warning, tr("Invalid move!"), tr("The server rejected this move."), QMessageBox::Ok);
    emit boardUpdate();
}

void communications::inCheck()
{
    struct
    {
        int x1, y1, x2, y2;
    } move;
    if (read(sd, &move, sizeof(int)*4) <= 0)
    {
        emit serverNotification(QMessageBox::Critical, tr("Conenction Error"), tr("Couldn't receive rollbck message"), QMessageBox::Ok);
        return;
    }
    if (move.x1<0 || move.x2<0 || move.y1<0 || move.y2<0 || move.x1>7 || move.x2>7 || move.y1>7 || move.y2>7)
    {
        qDebug("Invalid data from server! Check: (%d %d) (%d %d)", move.x1, move.y1,  move.x2, move.y2);
    }
    int piece = board[move.x2*8+move.y2];
//    if (piece <= NOPIECE)
        qDebug("Check! %d %c%c %c%c", piece, move.y1+'A', (7- move.x1) + '1',  move.y2+'A', (7-move.x2) + '1');
    board[move.x1*8+move.y1] = piece;
    board[move.x2*8+move.y2] = ex_piece;

    emit serverNotification(QMessageBox::Warning, tr("Invalid move!"), tr("Your king is in Check."), QMessageBox::Ok);
    emit boardUpdate();
}

void communications::run()
{
    struct sockaddr_in server;	// structura folosita pentru conectare
    /* cream socketul */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        emit serverNotification(QMessageBox::Critical, tr("Socket error"), tr("Couldn't create socket"), QMessageBox::Ok);
        return;
    }

    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    /* familia socket-ului */
    server.sin_family = AF_INET;
    /* adresa IP a serverului */
    server.sin_addr.s_addr = inet_addr(serverIp);
    /* portul de conectare */
    server.sin_port = htons(port);

    /* ne conectam la server */
    if (::connect (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
        emit serverNotification(QMessageBox::Critical, tr("Conenction Error"),
                                tr("Couldn't connect to %1 on port %2").arg(serverIp).arg(port), QMessageBox::Ok);
        return;
    }

    while (1 /*game not finished*/)
    {
        // (apel blocant pina cind serverul raspunde); Atentie si la cum se face read- vezi cursul!
        if (!receive())
        {
            emit serverNotification(QMessageBox::Critical, tr("Transmission Error"),
                                    tr("Couldn't receive board configuration"), QMessageBox::Ok);
            return;
        }
    }
}

void communications::send(int messageId, int x1, int y1, int x2, int y2, int piece)
{
    ex_piece = piece;
    int command[4];		// mesajul trimis
    command[0] = x1;
    command[1] = y1;
    command[2] = x2;
    command[3] = y2;
    if (write(sd, command, sizeof(int)*4) <= 0)
    {
        emit serverNotification(QMessageBox::Critical, tr("Transmission Error"),
                                tr("Couldn't send message %1").arg(messageId), QMessageBox::Ok);
        return;
    }
}
bool communications::receive()
{
    int message_id;
    if (read(sd, &message_id, sizeof(int)) <= 0)
    {
        return false;
    }

    switch(message_id)
    {
        case CLIENT_ID:
            if (!begun)
            {
                readSide();
            }
            break;
        case STATE:
            readState();
            break;
        case MOVE:
            readMove();
            break;
        case WRONG_MOVE:
            rollback();
        case IN_CHECK:
            inCheck();
            break;
        default:
            return false;
    }
    return true;
}

void communications::close()
{
    /* inchidem conexiunea, am terminat */
    ::close(sd);
}
