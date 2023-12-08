#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#define WHITE 0
#define BLACK 10
#define NOPIECE 0
#define PAWN 1
#define ROOK 2
#define KNIGHT 3
#define BISHOP 4
#define QUEEN 5
#define KING 6

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
