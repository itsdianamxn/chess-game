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

public slots:
    void startPlay();
    void yourTurn();
    void showWinner();

private:
    communications* comm;
    int board[8][8];
    QPixmap pixes[20];
    bool started = false;
    bool myTurn = false;

    void drawPiece(int x, int y, int w, QPainter* p);
    void init_board();
    bool isMyPiece(int piece);

};

#endif // MAINWINDOW_H
