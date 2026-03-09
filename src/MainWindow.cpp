#include "MainWindow.h"
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QThreadPool>
#include <QRunnable>
#include <QProcess>
#include <QFile>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    auto central = new QWidget(this);
    auto layout = new QVBoxLayout(central);

    statusLabel = new QLabel("Normalizzatore MP3 pronto!", this);
    logText = new QTextEdit(this);
    logText->setReadOnly(true);

    normalizeButton = new QPushButton("Seleziona e normalizza MP3", this);

    layout->addWidget(statusLabel);
    layout->addWidget(logText);
    layout->addWidget(normalizeButton);

    setCentralWidget(central);
    setWindowTitle("Normalizzatore MP3");
    resize(500, 300);

    connect(normalizeButton, &QPushButton::clicked, this, &MainWindow::onNormalizeClicked);
}

void MainWindow::onNormalizeClicked()
{
    QString file = QFileDialog::getOpenFileName(this, "Seleziona MP3", "", "MP3 Files (*.mp3)");
    if (file.isEmpty()){return;}

    logText->append("File selezionato: " + file);
    statusLabel->setText("Normalizzazione in corso...");

    // Chiamata ffmpeg per normalizzare il volume
    QString outputFile = file;
    outputFile.insert(file.lastIndexOf('.'), "_normalized");

    QString program = "./thirdparty/ffmpeg/ffmpeg"; // Persorso locale
    QStringList arguments;
    arguments << "-i" << file << "-filter:a" << "loudnorm" << outputFile;

    QProcess ffmpeg;
    ffmpeg.start(program, arguments);
    if (!ffmpeg.waitForFinished()){
        logText->append("Errore nella normalizzazione");
        statusLabel->setText("Errore!");
        return;
    }

    logText->append("File normalizzato salvato come: " + outputFile);
    statusLabel->setText("Normalizzazione completata!");
}