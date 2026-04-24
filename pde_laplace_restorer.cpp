#include "pde_laplace_restorer.h"
#include "Eigen/Sparse"
#include "Eigen/SparseLU"
#include "Eigen/Dense"
#include <QFileDialog>
#include <QMessageBox>

pde_laplace_restorer::pde_laplace_restorer(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    graphicsScene = new QGraphicsScene(this);
    ui.graphicsView->setScene(graphicsScene);
    pixmapItem = graphicsScene->addPixmap(QPixmap()); //na scenu pridame prazdny obrazok, ktory budem neskor prepisovat podla aktualneho stavu obrazku

}

pde_laplace_restorer::~pde_laplace_restorer()
{}

void pde_laplace_restorer::on_action_load_triggered() {
    QString fileName = QFileDialog::getOpenFileName(this, "Load image", "", "Images (*.png *.jpg *.pgm)");
    if (fileName.isEmpty()) {
        QMessageBox::information(this, "Action Canceled", "No image was selected.");
        return;
    }

    originalImage.load(fileName);
    if (!originalImage.load(fileName)) {
        QMessageBox::critical(this, "Error", "Failed to load the image. The file might be corrupted or unsupported.");
        return;
    }
    originalImage = originalImage.convertToFormat(QImage::Format_Grayscale8);

    img_width = originalImage.width();
    img_height = originalImage.height();

    int total_pixels = img_width * img_height;
    u.resize(total_pixels);
    Mask.resize(total_pixels);

    for (int j = 0; j < img_height; j++) { // j = os y (riadky)
        for (int i = 0; i < img_width; i++) {  // i = os x (stlpce)

            int k = j * img_width + i; //index 1D pola na zaklade 2D indexov

            int qt_y = (img_height - 1) - j; //preklopenie. y suradnicu prevedme s matematickej predstavy ((0,0) vlavo dole) na Qt (zaciatok vlavo hore)
            int qt_x = i;
            int pixelValue = qGray(originalImage.pixel(qt_x, qt_y)); //vytiahneme intenzitu aktualneho pixela orig obrazka - odtien sedej farby - cislo (0,255)
        
            u[k] = pixelValue; //ulozime do 1D vektora, podla ktoreho riesime Laplaceovu rovnicu, na vypocitany index tu intenzitu
        }
    }

    pixmapItem->setPixmap(QPixmap::fromImage(originalImage));
    ui.graphicsView->fitInView(pixmapItem, Qt::KeepAspectRatio);

    QMessageBox::information(this, "Success", "Image successfully loaded and processed into the mathematical model.");
}

