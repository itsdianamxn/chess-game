#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "common.h"

#define LOCAL_IP "127.0.0.1"

class communications;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void paintEvent(QPaintEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void mousePressEvent(QMouseEvent *event);

    QString pieceName(int piece);
    int getColor(int piece);

public slots:
    void startPlay();
    void yourTurn();
    void showWinner();
    void showServerNotification(int type, const QString& title, const QString& msg, int buttons);

private:
    communications* comm;
    int board[8][8];
    QPixmap pixes[20];
    bool started = false;
    bool myTurn = false;
    int dragPiece = NOPIECE;
    int startDragX = 0;
    int startDragY = 0;

    void drawPiece(int x, int y, int w, QPainter* p);
    void init_board();
    bool isMyPiece(int piece);

    int is_valid(int x1, int y1, int x2, int y2);
    int is_valid_pawn(int x1, int y1, int x2, int y2);
    int is_valid_rook(int x1, int y1, int x2, int y2);
    int is_valid_knight(int x1, int y1, int x2, int y2);
    int is_valid_bishop(int x1, int y1, int x2, int y2);
    int is_valid_queen(int x1, int y1, int x2, int y2);
    int is_valid_king(int x1, int y1, int x2, int y2);
};

#endif // MAINWINDOW_H
