#include <QMessageBox>
#include <QCoreApplication>
#include <QThread>
#include <QProcess>
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
    struct sockaddr_in server;	// structura folosita pentru conectare
    /* cream socketul */
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        QMessageBox::critical(parent,
                              tr("Socket error"),
                              tr("Couldn't create socket"),
                              QMessageBox::Ok);
        QCoreApplication::exit();
    }

    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    /* familia socket-ului */
    server.sin_family = AF_INET;
    /* adresa IP a serverului */
    server.sin_addr.s_addr = inet_addr(ip);
    /* portul de conectare */
    server.sin_port = htons (port);

    /* ne conectam la server */
    if (::connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
        QMessageBox::critical(parent,
                              tr("Conenction Error"),
                              tr("Couldn't connect to %1 on port %2").arg(ip).arg(port),
                              QMessageBox::Ok);
        QCoreApplication::exit();
    }

    start();
}

void communications::readSide()
{
    char blackOrWhite;
    if (read(sd, &blackOrWhite, sizeof(char)) <= 0)
    {
        QMessageBox::critical((QWidget*)parent(),
                              tr("Conenction Error"),
                              tr("Couldn't receive my number"),
                              QMessageBox::Ok);
        QCoreApplication::exit();
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
    int piece = 0;
    if(read(sd, &move, sizeof(int)*4) <= 0)
    {
        QMessageBox::critical((QWidget*)parent(),
                              tr("Conenction Error"),
                              tr("Couldn't receive my number"),
                              QMessageBox::Ok);
        QCoreApplication::exit();
    }
    piece = board[move.x1*8+move.y1];
    board[move.x1*8+move.y1] = 0;
    board[move.x2*8+move.y2] = piece;
    qDebug("MOVE read: %d %c%c %c%c", piece, move.y1+'A', (7- move.x1) + '1',  move.y2+'A', (7-move.x2) + '1');

    emit boardUpdate();

}
void communications::readState()
{
    int state;
    if (read(sd, &state, sizeof(int)) <= 0)
    {
        QMessageBox::critical((QWidget*)parent(),
                              tr("Conenction Error"),
                              tr("Couldn't receive my number"),
                              QMessageBox::Ok);
        QCoreApplication::exit();
    }
    qDebug("State read: %d", state );
    if (state == P_DISCONNECTED)
    {
        QMessageBox::information((QWidget*)parent(),
                                 tr("Game finished"),
                                 tr("Opponent disconected :("),
                                 QMessageBox::Retry);

        QCoreApplication::exit();

    }
    if (state == WHITEW)
    {
        QMessageBox::information((QWidget*)parent(),
                              tr("Game finished"),
                              tr("White wins!"),
                              QMessageBox::Retry);
        /*QString program = "/home/dam/Desktop/Retele/SAH/build-client_sah-Desktop_Qt_6_6_1_GCC_64bit-Debug/client_sah";
        QProcess process;
        process.start(program);*/
        QCoreApplication::exit();

    }
    if (state == BLACKW)
    {
        QMessageBox::information((QWidget*)parent(),
                              tr("Game finished"),
                              tr("Black wins!"),
                              QMessageBox::Retry);
        QCoreApplication::exit();
    }
    if (state == DRAW)
    {
        QMessageBox::information((QWidget*)parent(),
                              tr("Draw"),
                              tr("It's a draw"),
                              QMessageBox::Retry);
    }
    return;
}
void communications::run()
{
    while (1 /*game not finished*/)
    {
        // (apel blocant pina cind serverul raspunde); Atentie si la cum se face read- vezi cursul!
        if (!receive())
        {
            QMessageBox::critical((QWidget*)parent(),
                                  tr("Transmission Error"),
                                  tr("Couldn't receive board configuration"),
                                  QMessageBox::Ok);
            QCoreApplication::exit();
        }
    }
}

void communications::send(int messageId, int x1, int y1, int x2, int y2, QString msg)
{
    int command[4];		// mesajul trimis
    command[0] = x1;
    command[1] = y1;
    command[2] = x2;
    command[3] = y2;
    /*if (write(sd, messageId, sizeof(int)*2) <= 0)
    {
        QMessageBox::critical((QWidget*)parent(),
                              tr("Transmission Error"),
                              tr("Couldn't send message %1").arg(messageId),
                              QMessageBox::Ok);
        QCoreApplication::exit();
    }
    /*printf("message? %s", msg);
    if(msg!=null)
    {
        if (write(sd, command, sizeof(msg)) <= 0)
        {
            QMessageBox::critical((QWidget*)parent(),
                                  tr("Transmission Error"),
                                  tr("Couldn't send message %1").arg(messageId),
                                  QMessageBox::Ok);
    }*/
    if (write(sd, command, sizeof(int)*4) <= 0)
    {
        QMessageBox::critical((QWidget*)parent(),
                              tr("Transmission Error"),
                              tr("Couldn't send message %1").arg(messageId),
                              QMessageBox::Ok);
        QCoreApplication::exit();
    }
}
bool communications::receive()
{
    int message_id;
    if (read(sd, &message_id, sizeof(int)) <= 0)
    {
        return false;
    }
    if (!begun && message_id == CLIENT_ID)
    {
        readSide();
    }
    if (message_id == STATE)
    {
        readState();
    }
    if (message_id == MOVE)
    {
        readMove();
    }
    return true;
}
void communications::close()
{
    /* inchidem conexiunea, am terminat */
    ::close(sd);
}
