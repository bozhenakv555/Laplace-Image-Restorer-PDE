#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_pde_laplace_restorer.h"
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

class pde_laplace_restorer : public QMainWindow
{
    Q_OBJECT

public:
    pde_laplace_restorer(QWidget* parent = nullptr);
    ~pde_laplace_restorer();
    void updateView(QImage image); //aktualizuje scenu v graphicsview a prisposobi zoom novemu obrazku
    QImage createImageFromU(); //prepocita pdr vektor u spat na obrazok so spravnym preklopenim y

private:
    Ui::pde_laplace_restorerClass ui;
    int img_width = 0;
    int img_height = 0;
    std::vector<double> u;
    std::vector<int> Mask;
    int total_pixels;
    QImage originalImage;

    QGraphicsScene* graphicsScene;
    QGraphicsPixmapItem* pixmapItem;

private slots:
    void on_action_load_triggered();
    void on_tb_damage_clicked();
    void on_tb_restore_clicked();
    void on_tb_smooth_clicked();
    void on_tb_showOriginal_clicked();
};

