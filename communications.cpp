#include <QMessageBox>
#include <QCoreApplication>
#include <QThread>

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

void communications::readOrder()
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
    begun = 1;
    emit startGame();

    if (!first) {
        receive();
    }
}

void communications::readMove()
{
    int buffer[4];
    int piece = 0;
    if(read(sd, &buffer, sizeof(int)*4) < 0)
    {
        QMessageBox::critical((QWidget*)parent(),
                              tr("Conenction Error"),
                              tr("Couldn't receive my number"),
                              QMessageBox::Ok);
        QCoreApplication::exit();
    }
    piece = board[buffer[0]*8+buffer[1]];
    board[buffer[0]*8+buffer[1]] = 0;
    board[buffer[2]*8+buffer[3]] = piece;

    emit boardUpdate();

}

void communications::readState()
{
    int state;
    if (read(sd, &state, sizeof(int)) < 0)
    {
        QMessageBox::critical((QWidget*)parent(),
                              tr("Conenction Error"),
                              tr("Couldn't receive my number"),
                              QMessageBox::Ok);
        QCoreApplication::exit();
    }
    if (state == WHITEW)
    {
        QMessageBox::information((QWidget*)parent(),
                              tr("Conenction Error"),
                              tr("White wins. Black disconnected"),
                              QMessageBox::Retry);
        QCoreApplication::exit();

    }
    if (state == BLACKW)
    {
        QMessageBox::information((QWidget*)parent(),
                              tr("Conenction Error"),
                              tr("Black wins. White disconnected"),
                              QMessageBox::Retry);
        QCoreApplication::exit();
    }
    if (state == DRAW)
    {
        QMessageBox::information((QWidget*)parent(),
                              tr("Conenction Error"),
                              tr("draw"),
                                 QMessageBox::Retry);
        QCoreApplication::exit();
    }
    return;
}
void communications::run()
{
    if(allow)
    receive();

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
void communications::allowed(bool x)
{
    allow = x;
}
bool communications::receive()
{
    int buffer;
    //while(allow)
    //{
        if (read(sd, &buffer, sizeof(int)) < 0)
        {
            return false;
        }
        allow = 0;
        if(!begun && buffer == CLIENT_ID)
        {
            readOrder();
        }
        if(buffer == STATE)
        {
            readState();
        }
        if(buffer == MOVE)
        {
            readMove();
        }
    //}
    return true;
}
void communications::close()
{
    /* inchidem conexiunea, am terminat */
    ::close(sd);
}
