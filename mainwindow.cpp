#include <QPaintEvent>
#include <QPainter>
#include <QDataStream>
#include <QMimeData>
#include <QLabel>
#include <QDrag>

#include "mainwindow.h"
#include "communications.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    init_board();
    comm = new communications("127.0.0.1", 27182, (int*)board, parent);
    connect(comm, SIGNAL(startGame()), this, SLOT(startPlay()));
    connect(comm, SIGNAL(boardUpdate()), this, SLOT(yourTurn()));
    connect(comm, SIGNAL(closeGame()), this, SLOT(showWinner()));
}

MainWindow::~MainWindow() {}

void MainWindow::startPlay()
{
    myTurn = comm->isFirst();
    started = true;
    update();
    setWindowTitle(comm->isFirst() ?
        "Chess - You are playing white" :
        "Chess - You are playing black"
    );
}

void MainWindow::yourTurn()
{
    myTurn = true;
    update();
}
void MainWindow::showWinner()
{
    return;
}
void MainWindow::init_board()
{
    for (int i = 0; i<8; i++)
    {
        for (int j = 0; j<8; j++)
            board[i][j] = NOPIECE;
    }
    for (int i = 0; i<8; i++)
    {
        board[1][i] = PAWN + BLACK;
        board[6][i] = PAWN + WHITE;
    }
    board[0][0] = ROOK + BLACK;
    board[0][1] = KNIGHT + BLACK;
    board[0][2] = BISHOP + BLACK;
    board[0][3] = QUEEN + BLACK;
    board[0][4] = KING + BLACK;
    board[0][5] = BISHOP + BLACK;
    board[0][6] = KNIGHT + BLACK;
    board[0][7] = ROOK + BLACK;

    board[7][0] = ROOK + WHITE;
    board[7][1] = KNIGHT + WHITE;
    board[7][2] = BISHOP + WHITE;
    board[7][3] = QUEEN + WHITE;
    board[7][4] = KING + WHITE;
    board[7][5] = BISHOP + WHITE;
    board[7][6] = KNIGHT + WHITE;
    board[7][7] = ROOK + WHITE;

    pixes[PAWN + WHITE] = QPixmap("Images/lightPawn.png");
    pixes[ROOK + WHITE] = QPixmap("Images/lightRook.png");
    pixes[KNIGHT + WHITE] = QPixmap("Images/lightKnight.png");
    pixes[BISHOP + WHITE] = QPixmap("Images/lightBishop.png");
    pixes[QUEEN + WHITE] = QPixmap("Images/lightQueen.png");
    pixes[KING + WHITE] = QPixmap("Images/lightKing.png");
    pixes[PAWN + BLACK] = QPixmap("Images/darkPawn.png");
    pixes[ROOK + BLACK] = QPixmap("Images/darkRook.png");
    pixes[KNIGHT + BLACK] = QPixmap("Images/darkKnight.png");
    pixes[BISHOP + BLACK] = QPixmap("Images/darkBishop.png");
    pixes[QUEEN + BLACK] = QPixmap("Images/darkQueen.png");
    pixes[KING + BLACK] = QPixmap("Images/darkKing.png");
}

bool MainWindow::isMyPiece(int piece)
{
    if (piece >= BLACK) //if the piece is black
    {
        return !comm->isFirst();
    }
    else
    {
        return comm->isFirst();
    }
}

void MainWindow::drawPiece(int i, int j, int w, QPainter *p)
{
    if (board[i][j]<=NOPIECE)
        return;
    int margin = w/2;
    int x = margin+j*w;
    int y = margin+i*w;

    int piece = board[i][j];
    if (piece < 20)
        p->drawPixmap(x, y, w, w, pixes[piece]);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    QPainter p(this);

    if (!started)
    {
        p.setPen(Qt::white);
        p.drawText(0, 0, width(), height(), Qt::AlignHCenter|Qt::AlignVCenter, "Waiting for your opponent.");
        return;
    }

    int w = qMin(width(), height())/9; //size of a square
    int margin = w/2;
    p.setBackground(Qt::red);
    p.fillRect(margin, margin, 8*w, 8*w, QBrush(Qt::white, Qt::SolidPattern));
    p.setPen(Qt::white);

    for (int i = 0; i<8; i++)
    {
        p.drawText(i*w + margin, 0, w, margin, Qt::AlignHCenter|Qt::AlignVCenter, QString("%1").arg((char)(i+'A')));
        p.drawText(i*w + margin, 8*w+margin, w, margin, Qt::AlignHCenter|Qt::AlignVCenter, QString("%1").arg((char)(i+'A')));
        p.drawText(0, i*w + margin, margin, w, Qt::AlignHCenter|Qt::AlignVCenter, QString("%1").arg(8-i));
        p.drawText(8*w+margin, i*w + margin, margin, w, Qt::AlignHCenter|Qt::AlignVCenter, QString("%1").arg(8-i));

    }

    for (int i = 0; i<8; i++)
    {
        for (int j = 0; j<8; j++)
        {
            if ((i+j) % 2)
                p.fillRect(margin+i*w,margin+j*w,w,w, QBrush(Qt::darkGray, Qt::SolidPattern));
        }
    }
    // drawing pieces
    for (int i = 0; i<8; i++)
    {
        for (int j = 0; j<8; j++)
        {
            drawPiece(i, j, w, &p);
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-dndchessmove")) {
        if (event->source() == this) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        } else {
            event->acceptProposedAction();
        }
    } else {
        event->ignore();
    }
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-dndchessmove")) {
        if (event->source() == this) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        } else {
            event->acceptProposedAction();
        }
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasFormat("application/x-dndchessmove") ||
        event->source() != this)
    {
        event->ignore();
        return;
    }

    QByteArray itemData = event->mimeData()->data("application/x-dndchessmove");
    QDataStream dataStream(&itemData, QIODevice::ReadOnly);

    QPoint pos = event->position().toPoint();
    int w = qMin(width(), height())/9; //size of a square
    int margin = w/2;
    int x2 = (pos.x()-margin)/w;
    int y2 = (pos.y()-margin)/w;
    int piece, x1, y1;
    dataStream >> piece >> x1 >> y1;
    if (x1==x2 && y1==y2)
    {
        event->ignore();
        return; // put the piece back to its origin
    }

    qDebug("DROP %d from (%d,%d) to (%d,%d)", piece, y1, x1, y2, x2);
    board[y2][x2] = piece;
    update();
    event->setDropAction(Qt::MoveAction);
    event->accept();
    comm->allowed(0);
    comm->send(0,y1, x1, y2, x2, " ");
    comm->allowed(1);
    myTurn = false;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (!myTurn)
    {
        return;
    }

    QPoint pos = event->position().toPoint();
    int w = qMin(width(), height())/9; //size of a square
    int margin = w/2;
    int x = (pos.x()-margin)/w;
    int y = (pos.y()-margin)/w;

    if (x<0 || y<0 || x>=8 || y>=8)
        return;

    int piece = board[y][x];
    if (piece <= NOPIECE || !isMyPiece(piece))
    {
        return;
    }

    qDebug("Start DRAGging %d from (%d,%d)", piece, y, x);
    QPixmap pixmap = pixes[piece].scaled(w, w, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    QByteArray itemData;
    QDataStream dataStream(&itemData, QIODevice::WriteOnly);
    dataStream << piece << x << y;

    QMimeData *mimeData = new QMimeData;
    mimeData->setData("application/x-dndchessmove", itemData);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(pixmap);
    drag->setHotSpot(event->position().toPoint() - QPoint(x*w+margin, y*w+margin));

    board[y][x] = -board[y][x];
    update();

    if (drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction) == Qt::MoveAction) {
        board[y][x] = NOPIECE;
    } else {
        board[y][x] = -board[y][x];
    }
    update();
}
