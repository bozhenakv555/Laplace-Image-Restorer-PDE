#include "pde_laplace_restorer.h"
#include "Eigen/Sparse"
#include "Eigen/SparseLU"
#include "Eigen/Dense"

pde_laplace_restorer::pde_laplace_restorer(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
}

pde_laplace_restorer::~pde_laplace_restorer()
{}

