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
    //ak je RGB, vytvorime farebny format, inak ciernobiely
    QImage::Format format = isRGB ? QImage::Format_RGB32 : QImage::Format_Grayscale8;
    QImage img(img_width, img_height, format);

    for (int j = 0; j < img_height; j++) { // j = os y (riadky, idu zdola nahor)
        for (int i = 0; i < img_width; i++) { // i = os x (stlpce, idu zlava doprava)
            int k = j * img_width + i; 
            int qt_x = i;
            int qt_y = (img_height - 1) - j; //preklopenie y

            //qBound rovno osetri, aby cislo neuslo pod 0 alebo nad 255
            int r = qBound(0, qRound(u_r[k]), 255);
            //ak je grayscale, skopiruje sa r ->namiesame odtien sedej (vsetky rgb zlozky su rovnake) a vyfarbime dany pixel
            int g = isRGB ? qBound(0, qRound(u_g[k]), 255) : r; 
            int b = isRGB ? qBound(0, qRound(u_b[k]), 255) : r;

            img.setPixel(qt_x, qt_y, qRgb(r, g, b));
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

    isRGB = (ui.cb_colorMode->currentIndex() == 1);

    if (!isRGB) {
        originalImage = originalImage.convertToFormat(QImage::Format_Grayscale8);
    }
    else {
        originalImage = originalImage.convertToFormat(QImage::Format_RGB32);
    }
    img_width = originalImage.width();
    img_height = originalImage.height();

    total_pixels = img_width * img_height;

    u_g.assign(total_pixels, 0.0);
    //uk nie je RGB, vektor sa ostane prazdny
    u_r.assign(isRGB ? total_pixels : 0, 0.0); 
    u_b.assign(isRGB ? total_pixels : 0, 0.0);

    Mask.resize(total_pixels);
    Mask.assign(total_pixels, 1); //nainicalizujeme vsetky pixely na jednu - su zachovane, obrazik je na zaciatku uplne neposkodeny

    for (int j = 0; j < img_height; j++) { // j = os y (riadky)
        for (int i = 0; i < img_width; i++) {  // i = os x (stlpce)

            int k = j * img_width + i; //index 1D pola na zaklade 2D indexov

            int qt_y = (img_height - 1) - j; //preklopenie. y suradnicu prevedme s matematickej predstavy ((0,0) vlavo dole) na Qt (zaciatok vlavo hore)
            int qt_x = i;

            QColor color = originalImage.pixelColor(qt_x, qt_y);

            if (isRGB) {
                u_r[k] = color.red();
                u_g[k] = color.green();
                u_b[k] = color.blue();
            }
            else {
                u_g[k] = qGray(color.rgb()); //ak je gray, staci len gcko
            }   //vytiahneme intenzitu aktualneho pixela orig obrazka - odtien sedej farby - cislo (0,255)
                //ulozime do 1D vektora, podla ktoreho riesime Laplaceovu rovnicu, na vypocitany index tu intenzitu
        }
    }

    //vymazeme stare ulozene obrazky
    damagedImg = QImage();
    restoredImg = QImage();
    smoothedImg = QImage();

    updateView(originalImage);

    QMessageBox::information(this, "Success", "Image successfully loaded and processed into the mathematical model.");
}

void pde_laplace_restorer::on_tb_damage_clicked()
{
    if (u_g.empty()) return;

    if (!damagedImg.isNull()) {
        updateView(damagedImg);
        return;
    }

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
            u_g[k] = 0; //a zmazeme vlastne - bude vynulovany, cierny (damage)
            if (isRGB) {
                u_r[k] = 0;
                u_b[k] = 0;
            }
            damaged_count++;
        }
    }

    //vymazeme staru pamat:
    restoredImg = QImage();
    smoothedImg = QImage();
    damagedImg = createImageFromU(); //ulozime novy poskodeny
    updateView(damagedImg);
    QMessageBox::information(this, "Damage Applied",
        QString("Successfully removed %1 pixels (%2%).").arg(n_to_damage).arg(p));
}

void pde_laplace_restorer::on_tb_restore_clicked()
{
    if (u_g.empty()) return;

    if (!restoredImg.isNull()) {
        updateView(restoredImg);
        return;
    }

    int N = img_width * img_height; //celkovy pocet rovnic(riadkov matice)

    Eigen::SparseMatrix<double, Eigen::RowMajor> M(N, N); //pri RowMajor matica sa do pamate uklada po riadkoch
    //VectorXd - dynamicky vector doublov, naplnime ho nulami
    Eigen::VectorXd b_r = Eigen::VectorXd::Zero(N);
    Eigen::VectorXd b_g = Eigen::VectorXd::Zero(N);
    Eigen::VectorXd b_b = Eigen::VectorXd::Zero(N);

    M.reserve(Eigen::VectorXi::Constant(N, 5)); //rezervujeme v maticu miesto pre maximum 5 nenulovych prvkov v kazdom z N riadkov

    for (int k = 0; k < N; k++) {
        int j = k / img_width;
        int i = k % img_width;

        if (Mask[k] == 1) { //pixel nie je vymazany, hodnota intenzity je znama
            M.insert(k, k) = 1.; //na ho miesto zapiseme jednotku
            b_g(k) = u_g[k]; //a pravu stranu nastavime na tu znamu intenzitu
            if (isRGB) {
                b_r(k) = u_r[k];
                b_b(k) = u_b[k];
            }
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

    //ten isty solver aj pre rgb, aj pre grayscale
    //vyriesime rovnicu a vysledne intenzity pixelov ulozime do vektora result
    Eigen::VectorXd res_g = solver.solve(b_g);
    Eigen::VectorXd res_r, res_b;
    if (isRGB) {
        res_r = solver.solve(b_r);
        res_b = solver.solve(b_b);
    }

    //prekopirujeme vyratane hodnoty zo solvera spat do nasho pola u
    for (int k = 0; k < N; k++) {
        u_g[k] = res_g(k);
        if (isRGB) {
            u_r[k] = res_r(k);
            u_b[k] = res_b(k);
        }
    }

    restoredImg=createImageFromU();
    updateView(restoredImg);
    QMessageBox::information(this, "Success", "Image was reconstructed!");
}

void pde_laplace_restorer::on_tb_smooth_clicked()
{
    if (u_g.empty()) return;

    if (!smoothedImg.isNull()) {
        updateView(smoothedImg);
        QMessageBox::information(this, "Meow", "Showing already smoothed image!");
        return;
    }

    int N = img_width * img_height;

    double lambda = ui.dsb_lambda->value(); //cim mensia lambda, tym viac to rozmaze. cim vacsia, tym viac to ostane ako predtym

    Eigen::SparseMatrix<double, Eigen::RowMajor> M(N, N);
    Eigen::VectorXd b_r = Eigen::VectorXd::Zero(N);
    Eigen::VectorXd b_g = Eigen::VectorXd::Zero(N);
    Eigen::VectorXd b_b = Eigen::VectorXd::Zero(N);
    M.reserve(Eigen::VectorXi::Constant(N, 5));

    for (int k = 0; k < N; k++) {
        int j = k / img_width;
        int i = k % img_width;

        //-diagonala a prava strana(fidality term):
        M.insert(k, k) = 4.0 + lambda; //na diagonalu povodnej matice pripocitame lambda
        b_g(k) = lambda * u_g[k]; //u[k] je teraz nase u_0 (zrekonstruovany obrazok)
        if (isRGB) {
            b_r(k) = lambda * u_r[k];
            b_b(k) = lambda * u_b[k];
        }

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

    Eigen::VectorXd res_g = solver.solve(b_g);
    Eigen::VectorXd res_r, res_b;
    if (isRGB) {
        res_r = solver.solve(b_r);
        res_b = solver.solve(b_b);
    }

    for (int k = 0; k < N; k++) {
        u_g[k] = res_g(k);
        if (isRGB) {
            u_r[k] = res_r(k);
            u_b[k] = res_b(k);
        }
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
