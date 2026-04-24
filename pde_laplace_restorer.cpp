#include "pde_laplace_restorer.h"
#include "Eigen/Sparse"
#include "Eigen/SparseLU"
#include "Eigen/Dense"
#include <QFileDialog>
#include <QMessageBox>
#include <random>

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

void pde_laplace_restorer::updateView(QImage image) 
{
    pixmapItem->setPixmap(QPixmap::fromImage(image)); //premeni vyratany qimage na qpixmap a hodi ho na nas objekt v scene
    ui.graphicsView->fitInView(pixmapItem, Qt::KeepAspectRatio); //zazoomuje okno tak, aby do neho obrazok presne sadol a nenatiahol sa
}

QImage pde_laplace_restorer::createImageFromU()
{
    QImage img(img_width, img_height, QImage::Format_Grayscale8); //vytvorime uplne prazdny 8-bitovy ciernobiely obrazok (jeden kanal)

    for (int j = 0; j < img_height; j++) { // j = os y (riadky, idu zdola nahor)
        for (int i = 0; i < img_width; i++) { // i = os x (stlpce, idu zlava doprava)
            int k = j * img_width + i; 
            int qt_x = i;
            int qt_y = (img_height - 1) - j; //preklopenie y

            int val = qRound(u[k]); //vytiahneme intenzitu z vektora u a zaokruhlime na cele cislo

            img.setPixel(qt_x, qt_y, qRgb(val, val, val)); //namiesame odtien sedej (vsetky rgb zlozky su rovnake) a vyfarbime dany pixel
        }
    }
    return img; //vyvratime hotovy obrazok, ktory sa hned moze poslat do updateView
}

void pde_laplace_restorer::on_action_load_triggered() 
{
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

    total_pixels = img_width * img_height;
    u.resize(total_pixels);
    Mask.resize(total_pixels);
    Mask.assign(total_pixels, 1); //nainicalizujeme vsetky pixely na jednu - su zachovane, obrazik je na zaciatku uplne neposkodeny

    for (int j = 0; j < img_height; j++) { // j = os y (riadky)
        for (int i = 0; i < img_width; i++) {  // i = os x (stlpce)

            int k = j * img_width + i; //index 1D pola na zaklade 2D indexov

            int qt_y = (img_height - 1) - j; //preklopenie. y suradnicu prevedme s matematickej predstavy ((0,0) vlavo dole) na Qt (zaciatok vlavo hore)
            int qt_x = i;
            int pixelValue = qGray(originalImage.pixel(qt_x, qt_y)); //vytiahneme intenzitu aktualneho pixela orig obrazka - odtien sedej farby - cislo (0,255)
        
            u[k] = pixelValue; //ulozime do 1D vektora, podla ktoreho riesime Laplaceovu rovnicu, na vypocitany index tu intenzitu
        }
    }

    updateView(originalImage);

    QMessageBox::information(this, "Success", "Image successfully loaded and processed into the mathematical model.");
}

void pde_laplace_restorer::on_tb_damage_clicked()
{
    if (u.empty()) return;

    int p = ui.sb_damagePercent->value(); //percento pixelov, ktore chceme poskodit
    int total_pixels = img_width * img_height;
    int n_to_damage = (total_pixels * p) / 100; //pocet pixelov, ktore chceme poskodit

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, total_pixels - 1);

    int damaged_count = 0;
    while (damaged_count < n_to_damage) {
        int k = dis(gen); //vygenerujeme nahodny index od 0 po (total_pixels - 1)
        
        if (Mask[k] == 1) { //ak tento pixel stale nie je zmazany (oznaceny 1)
            Mask[k] = 0;  //tak oznacime ako zmazany
            u[k] = 0; //a zmazeme vlastne - bude vynulovany, cierny (damage)
            damaged_count++;
        }
    }

    QImage damagedImg = createImageFromU();
    updateView(damagedImg);
    QMessageBox::information(this, "Damage Applied",
        QString("Successfully removed %1 pixels (%2%).").arg(n_to_damage).arg(p));
}