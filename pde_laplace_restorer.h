#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_pde_laplace_restorer.h"
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

class pde_laplace_restorer : public QMainWindow
{
    Q_OBJECT

public:
    pde_laplace_restorer(QWidget *parent = nullptr);
    ~pde_laplace_restorer();

private:
    Ui::pde_laplace_restorerClass ui;
    int img_width = 0;
    int img_height = 0;
    std::vector<double> u;
    std::vector<int> Mask;
    QImage originalImage;

    QGraphicsScene* graphicsScene;
    QGraphicsPixmapItem* pixmapItem;

private slots:
    void on_action_load_triggered();
};

