#include "MainWindow.h"
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QProcess>
#include <QDebug>

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