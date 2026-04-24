#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_pde_laplace_restorer.h"

class pde_laplace_restorer : public QMainWindow
{
    Q_OBJECT

public:
    pde_laplace_restorer(QWidget *parent = nullptr);
    ~pde_laplace_restorer();

private:
    Ui::pde_laplace_restorerClass ui;
};

