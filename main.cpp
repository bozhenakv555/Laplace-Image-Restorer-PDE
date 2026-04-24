#include "pde_laplace_restorer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    pde_laplace_restorer window;
    window.show();
    return app.exec();
}
