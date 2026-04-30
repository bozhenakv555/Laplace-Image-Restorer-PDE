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
    graphicsScene->setSceneRect(pixmapItem->boundingRect()); //nastavi hranice sceny presne na velkost obrazka, aby to fitInView spravne vycentrovalo a neskakalo to
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

            if (val < 0) val = 0;
            if (val > 255) val = 255;
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

void pde_laplace_restorer::on_tb_restore_clicked()
{
    if (u.empty()) return;

    int N = img_width * img_height; //celkovy pocet rovnic(riadkov matice)

    Eigen::SparseMatrix<double, Eigen::RowMajor> M(N, N); //pri RowMajor matica sa do pamate uklada po riadkoch
    Eigen::VectorXd b = Eigen::VectorXd::Zero(N); //VectorXd - dynamicky vector doublov, naplnime ho nulami
   
    M.reserve(Eigen::VectorXi::Constant(N, 5)); //rezervujeme v maticu miesto pre maximum 5 nenulovych prvkov v kazdom z N riadkov

    for (int k = 0; k < N; k++) {
        int j = k / img_width;
        int i = k % img_width;

        if (Mask[k] == 1) { //pixel nie je vymazany, hodnota intenzity je znama
            M.insert(k, k) = 1.; //na ho miesto zapiseme jednotku
            b(k) = u[k]; //a pravu stranu nastavime na tu znamu intenzitu
        }
        else {
            M.insert(k, k) = 4.; //na diagonale su vzdy 4 (ak nemame Dirichletovu OP^)

            //pozerame na horizontalnych susedi
            if (i == 0) {
                //sme v prvom stlpci, lavy sused chyba -> robime trik so zrkadlenim - pravy sused (k+1) dostane vahu -2 cim kompenzuje ten lavy
                M.insert(k, k + 1) = -2.0;
            }
            else if (i == img_width - 1) {
                //sme v poslednom stlpci, pravy sused chyba -> lavy sused (k-1) dostane vahu -2
                M.insert(k, k - 1) = -2.0;
            }
            else {
                //sme vnutri riadku, obaja susedia existuju, kazdy ma klasickych -1
                M.insert(k, k - 1) = -1.0;
                M.insert(k, k + 1) = -1.0;
            }
            //pozerame na vertikalnych susedi
            if (j == 0) {
                //sme v prvom riadku, dolny sused chyba -> horny sused (k + img_width) dostane -2
                M.insert(k, k + img_width) = -2.0;
            }
            else if (j == img_height - 1) {
                //sme v poslednom riadku, horny sused chyba -> dolny sused (k - img_width) dostane -2
                M.insert(k, k - img_width) = -2.0;
            }
            else {
                //sme vnutri stlpca, obaja susedia existuju
                M.insert(k, k - img_width) = -1.0;
                M.insert(k, k + img_width) = -1.0;
            }
        }
    }
    M.makeCompressed();
    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver; //pripravime si solver, ktory vyriesi nasu sustavu Mu=b pomocou LU(lower/upper triangle-nenulove prsvky iba pod/nad diagonalou) rozkladu
    solver.analyzePattern(M); //solver si najprv preskuma strukturu nul v matici, aby vedel ako na nu
    solver.factorize(M); //tu sa deje samotny LU rozklad

    if (solver.info() != Eigen::Success) {
        QMessageBox::critical(this, "Error", "Couldn't solve the system!");
        return;
    }

    Eigen::VectorXd result = solver.solve(b); //vyriesime rovnicu a vysledne intenzity pixelov ulozime do vektora result

    for (int k = 0; k < N; k++) {
        u[k] = result(k); //prekopirujeme vyratane hodnoty zo solvera spat do nasho pola u
    }

    QImage restoredImg=createImageFromU();
    updateView(restoredImg);
    QMessageBox::information(this, "Success", "Image was reconstructed!");
}

void pde_laplace_restorer::on_tb_smooth_clicked()
{
    if (u.empty()) return;

    int N = img_width * img_height;

    double lambda = ui.dsb_lambda->value(); //cim mensia lambda, tym viac to rozmaze. cim vacsia, tym viac to ostane ako predtym

    Eigen::SparseMatrix<double, Eigen::RowMajor> M(N, N);
    Eigen::VectorXd b = Eigen::VectorXd::Zero(N);
    M.reserve(Eigen::VectorXi::Constant(N, 5));

    for (int k = 0; k < N; k++) {
        int j = k / img_width;
        int i = k % img_width;

        //-diagonala a prava strana(fidality term):
        M.insert(k, k) = 4.0 + lambda; //na diagonalu povodnej matice pripocitame lambda
        b(k) = lambda * u[k]; //u[k] je teraz nase u_0 (zrekonstruovany obrazok)

        //-susedia a okraje(smoothing term + zrkadlenie - presne ako v restore)
        // pozerame na horizontalnych susedov
        if (i == 0) {
            M.insert(k, k + 1) = -2.0;
        }
        else if (i == img_width - 1) {
            M.insert(k, k - 1) = -2.0;
        }
        else {
            M.insert(k, k - 1) = -1.0;
            M.insert(k, k + 1) = -1.0;
        }

        // pozerame na vertikalnych susedov
        if (j == 0) {
            M.insert(k, k + img_width) = -2.0;
        }
        else if (j == img_height - 1) {
            M.insert(k, k - img_width) = -2.0;
        }
        else {
            M.insert(k, k - img_width) = -1.0;
            M.insert(k, k + img_width) = -1.0;
        }
    }

    M.makeCompressed();
    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
    solver.analyzePattern(M);
    solver.factorize(M);

    if (solver.info() != Eigen::Success) {
        QMessageBox::critical(this, "Error", "Couldn't solve the system!");
        return;
    }

    Eigen::VectorXd result = solver.solve(b);

    for (int k = 0; k < N; k++) {
        u[k] = result(k);
    }

    QImage smoothedImg = createImageFromU();
    updateView(smoothedImg);
    QMessageBox::information(this, "Success", "The image was smoothed!");
}

void pde_laplace_restorer::on_tb_showOriginal_clicked()
{
    updateView(originalImage);
    QMessageBox::information(this, "Meow", "Here's the original");
}
