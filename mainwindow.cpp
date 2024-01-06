#include <QPaintEvent>
#include <QPainter>
#include <QDataStream>
#include <QMimeData>
#include <QLabel>
#include <QDrag>
#include <QToolTip>
#include <QMessageBox>
#include <QApplication>

#include "mainwindow.h"
#include "communications.h"
//10.20.0.1
#define CASTLING_RIGHT 90
#define CASTLING_LEFT  91
#define MOVE_SIMPLE 88
#define MOVE_EAT    89
#define MOVE_NOPE    0

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setAcceptDrops(true);
    comm = new communications(LOCAL_IP, 27181, (int*)board, parent);
    connect(comm, SIGNAL(startGame()), this, SLOT(startPlay()), Qt::QueuedConnection);
    connect(comm, SIGNAL(boardUpdate()), this, SLOT(yourTurn()), Qt::QueuedConnection);
    connect(comm, SIGNAL(closeGame()), this, SLOT(showWinner()), Qt::QueuedConnection);
    connect(comm, SIGNAL(serverNotification(int,QString,QString,int)),
            this, SLOT(showServerNotification(int,QString,QString,int)), Qt::QueuedConnection);

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
    setWindowTitle("Chess");
}

MainWindow::~MainWindow() {}

void MainWindow::startPlay()
{
    myTurn = comm->isFirst();
    started = true;

    init_board();
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

QString MainWindow::pieceName(int piece)
{
    if (piece == NOPIECE)
        return "";

    QString value;
    switch (piece % 10)
    {
        case PAWN:   value = "pawn";    break;
        case ROOK:   value = "rook";    break;
        case KNIGHT: value = "knight";  break;
        case BISHOP: value = "bishop";  break;
        case QUEEN:  value = "queen";   break;
        case KING:   value = "king";    break;
        default:     value = "unknown";
    }

    return QString(getColor(piece) == WHITE ? "white %1" : "black %1").arg(value);
}

void MainWindow::showServerNotification(int type, const QString& title, const QString& msg, int buttons)
{
    switch (type) {
        case QMessageBox::Warning:
            QMessageBox::warning(this, title, msg, (QMessageBox::StandardButton)buttons);
            return;
        case QMessageBox::Critical:
            QMessageBox::critical(this, title, msg, (QMessageBox::StandardButton)buttons);
            QApplication::quit();
            return;
        default: //use a tooltip
            QPoint offset(this->x() + this->width()/2, this->y() + this->height()/2);
            QToolTip::showText(offset, msg, this, rect(), 1000);
        }
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
    p.fillRect(0, 0, 9*w, 10*w, QBrush(Qt::black, Qt::SolidPattern));

    int margin = w/2;
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
            if (dragPiece != NOPIECE)
            {
                int moveType = is_valid(startDragY, startDragX, j, i);
                if (moveType != MOVE_NOPE)
                {
                    p.setOpacity(.33);
                    p.fillRect(margin+i*w, margin+j*w, w, w,
                               QBrush(moveType == MOVE_EAT ? Qt::red : Qt::green, Qt::SolidPattern));
                    if (moveType == MOVE_SIMPLE)
                        p.drawPixmap(margin+i*w, margin+j*w, w, w, pixes[dragPiece]);
                    p.setOpacity(1);
                }
            }
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

    if (!myTurn)
        p.fillRect(margin, margin, 8*w, 8*w, QBrush(QColor(127, 127, 127, 64), Qt::SolidPattern));
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
    dragPiece = NOPIECE;

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

    int valid_move = is_valid(y1, x1, y2, x2);
    if (x1==x2 && y1==y2 || valid_move == MOVE_NOPE)
    {
        qDebug("Drag '%s' from (%d %d) to (%d %d) aborted because the move is invlid!", qPrintable(pieceName(piece)), x1, y1, x2, y2);
        event->ignore();
        return; // put the piece back to its origin
    }

    //promotion (pawn to queen)
    if (piece == (BLACK + PAWN) && y2 == 7)
        piece = BLACK + QUEEN;
    if (piece == (WHITE + PAWN) && y2 == 0)
        piece = WHITE + QUEEN;

    qDebug("DROP '%s' from (%d,%d) to (%d,%d)", qPrintable(pieceName(piece)), y1, x1, y2, x2);
    int old_piece = board[y2][x2];
    board[y2][x2] = piece;
    board[y1][x1] = NOPIECE;

    //castling
    if (valid_move == CASTLING_RIGHT)
    {
        board[y1][5] = ROOK + getColor(piece);
        board[y1][7] = NOPIECE;
    }
    if (valid_move == CASTLING_LEFT)
    {
        board[y1][3] = ROOK + getColor(piece);
        board[y1][0] = NOPIECE;
    }
    update();

    event->setDropAction(Qt::MoveAction);
    event->accept();

    comm->send(0, y1, x1, y2, x2, old_piece);
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

    qDebug("Start DRAGging '%s' from (%d,%d)", qPrintable(pieceName(piece)),  y, x);
    QPixmap pixmap = pixes[piece].scaled(w, w, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    //QPixmap pixmap = pixes[piece].scaled(w,w);
    QByteArray itemData;
    QDataStream dataStream(&itemData, QIODevice::WriteOnly);
    dataStream << piece << x << y;

    QMimeData *mimeData = new QMimeData;
    mimeData->setData("application/x-dndchessmove", itemData);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(pixmap);
    drag->setHotSpot(event->position().toPoint() - QPoint(x*w+margin, y*w+margin));

    dragPiece = board[y][x];
    board[y][x] = -dragPiece;
    startDragX = x;
    startDragY = y;
    update();

    if (drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction) != Qt::MoveAction)
    {
        // event aborted, restoring piece
        board[y][x] = -board[y][x];
    }
    qDebug("drag execute");
    update();
}

int MainWindow::getColor(int piece)
{
    return (piece/10) * 10;
}

int MainWindow::is_valid(int x1, int y1, int x2, int y2)
{
    int srcPiece = abs(board[x1][y1]);
    qDebug("Validating move from (%d %d) '%s' to (%d %d) '%s'.",
           x1, y1, qPrintable(pieceName(srcPiece)),
           x2, y2, qPrintable(pieceName(board[x2][y2])));

    if (!isMyPiece(srcPiece) ||
        board[x2][y2] != NOPIECE && getColor(board[x2][y2]) == getColor(srcPiece) ||
        (x1==x2 && y1==y2))
        return MOVE_NOPE;

    int crtPiece = srcPiece % 10; // no color
    switch (crtPiece)
    {
        case PAWN:   return is_valid_pawn(x1, y1, x2, y2);
        case ROOK:   return is_valid_rook(x1, y1, x2, y2);
        case KNIGHT: return is_valid_knight(x1, y1, x2, y2);
        case BISHOP: return is_valid_bishop(x1, y1, x2, y2);
        case QUEEN:  return is_valid_queen(x1, y1, x2, y2);
        case KING:   return is_valid_king(x1, y1, x2, y2);
        default:     return MOVE_NOPE;
    }
}

int MainWindow::is_valid_pawn(int x1, int y1, int x2, int y2)
{
    int srcPiece = abs(board[x1][y1]);
    if (getColor(srcPiece) == WHITE)
    {
        if (y1 == y2 && x2 == x1-1 && board[x2][y2] == NOPIECE) // daca face pas normal
        {
            return MOVE_SIMPLE;
        }
        if (y1 == y2 && x1 == 6 && x2 == 4 &&
            board[5][y2] == NOPIECE && board[4][y2] == NOPIECE) //de pe pozitia de start poate sa mearga 2 pasi
        {
            return MOVE_SIMPLE;
        }
        if (getColor(board[x2][y2])==BLACK && x2 == x1-1 && abs(y1-y2)==1) //cand mananca
        {
            return MOVE_EAT;
        }
    }
    else
    {
        if (y1 == y2 && x2 == x1+1 && board[x2][y2] == NOPIECE) //daca face pas normal
        {
            return MOVE_SIMPLE;
        }
        if (y1 == y2 && x1 == 1 && x2 == 3 &&
            board[2][y2] == NOPIECE&& board[3][y2] == NOPIECE) //de pe pozitia de start poate sa mearga 2 pasi
        {
            return MOVE_SIMPLE;
        }
        if (board[x2][y2] != NOPIECE && getColor(board[x2][y2]) == WHITE && x2 == x1+1 && abs(y1-y2) == 1) //cand mananca
        {
            return MOVE_EAT;
        }
    }
    return MOVE_NOPE;
}

int MainWindow::is_valid_rook(int x1, int y1, int x2, int y2)
{
    int i;
    if (x1 == x2)
    {
        if (y1<y2)
        {
            for (i = y1 + 1; i < y2; i++)
                if (board[x1][i]!=NOPIECE)
                    return MOVE_NOPE;
        }
        else
            for (i = y1 - 1; i > y2; i--)
                if (board[x1][i]!=NOPIECE)
                    return MOVE_NOPE;
        return (board[x2][y2] == NOPIECE) ? MOVE_SIMPLE : MOVE_EAT;
    }
    else if (y1 == y2)
    {
        if (x1<x2)
        {
            for (i = x1 + 1; i < x2; i++)
                if (board[i][y1]!=NOPIECE)
                    return MOVE_NOPE;
        }
        else
            for (i = x1 - 1; i > x2; i--)
                if (board[i][y1]!=NOPIECE)
                    return MOVE_NOPE;
        return (board[x2][y2] == NOPIECE) ? MOVE_SIMPLE : MOVE_EAT;
    }
    return MOVE_NOPE;
}

int MainWindow::is_valid_knight(int x1, int y1, int x2, int y2)
{
    if (abs(x1-x2) * abs(y1-y2) == 2)
    {
        return (board[x2][y2] == NOPIECE) ? MOVE_SIMPLE : MOVE_EAT;
    }
    return MOVE_NOPE;
}

int MainWindow::is_valid_bishop(int x1, int y1, int x2, int y2)
{
    if (abs(x1-x2) == abs(y1-y2))
    {
        int stepX = (x1<x2) ? 1 : -1;
        int stepY = (y1<y2) ? 1 : -1;
        x1 += stepX;
        y1 += stepY;
        for (; x1 != x2; x1 += stepX, y1 += stepY)
        {
            if (board[x1][y1] != NOPIECE)
                return MOVE_NOPE;
        }
        return (board[x2][y2] == NOPIECE) ? MOVE_SIMPLE : MOVE_EAT;
    }
    return MOVE_NOPE;
}

int MainWindow::is_valid_queen(int x1, int y1, int x2, int y2)
{
    int moveType = is_valid_rook(x1, y1, x2, y2);
    if (moveType == MOVE_NOPE)
    {
        moveType = is_valid_bishop(x1, y1, x2, y2);
    }
    return moveType;
}

int MainWindow::is_valid_king(int x1, int y1, int x2, int y2)
{
    int i;
    if (abs(x1-x2)<=1 && abs(y1-y2) <= 1)
        return (board[x2][y2] == NOPIECE) ? MOVE_SIMPLE : MOVE_EAT;

    // castling:
    if (x1 == x2 && y1 == 4 && abs(y1-y2) == 2)
    {
        int myColor = abs(getColor(board[x1][y1]));
        if (y2 == 6)
        {
            for (i = y1 + 1; i < y2; i++)
                if (board[x1][i] != NOPIECE)
                    return MOVE_NOPE;
            if (board[x1][7] != ROOK + myColor)
                return MOVE_NOPE;
        }
        else if (y2 == 2)
        {
            for (i = y1 - 1; i >= y2-1; i--)
                if (board[x1][i] != NOPIECE)
                    return MOVE_NOPE;
            if (board[x1][0] != ROOK + myColor)
                return MOVE_NOPE;
        }

        if (myColor == BLACK && x1 == 0 ||
            myColor == WHITE && x1 == 7)
        {
            return (y2==6) ? CASTLING_RIGHT : CASTLING_LEFT;
        }
    }
    return MOVE_NOPE;
}

