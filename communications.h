#ifndef COMMUNICATIONS_H
#define COMMUNICATIONS_H

#include <QThread>
#include "common.h"

#define CLIENT_ID       10
#define STATE           20
#define MOVE            30
#define WRONG_MOVE      40
#define IN_CHECK        50
#define P_DISCONNECTED  0
#define WHITE_WIN       1
#define DRAW            2
#define BLACK_WIN       3


class communications : public QThread
{
    Q_OBJECT

public:
    explicit communications(const char* ip, int port, int* board, QWidget *parent = nullptr);
    void send(int messageId, int x1, int y1, int x2, int y2, int piece);
    void close();
    inline bool isFirst() { return first; }
protected:
    void run() override;

private:
    int port;       // portul de conectare la server
    int sd = 0;		// descriptorul de socket
    char* serverIp = nullptr;
    int* board;
    bool first;
    bool begun = false;
    int ex_piece = NOPIECE;
    bool receive();
    void readMove();
    void readState();
    void readSide();
    void rollback();
    void inCheck();

signals:
    void startGame();
    void boardUpdate();
    void closeGame();
    void serverNotification(int type, const QString& title, const QString& msg, int buttons);
};

#endif // COMMUNICATIONS_H
