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

#include <functional>
#include <regex>
#include <iostream>
#include <atomic>

// --- Task per normalizzazione ---
class NormalizeTask : public QRunnable {
public:
    NormalizeTask(const QString &in, const QString &out, double peak, const QStringList &params,
                  std::function<void(bool,const QString&)> callback)
        : inputFile(in), outputFile(out), targetPeak(peak), ffmpegParams(params), cb(callback) {}

    void run() override{
        bool success = false;
        QString errorMsg;
        QString tempFile = outputFile + ".tmp.mp3";

        QString program = "./thirdparty/ffmpeg/ffmpeg";

        // Rilevazione picco
        QStringList cmdPeak = {"-i", inputFile, "-map", "0:a:0?", "-af", "volumedetect", "-f", "null", "-"};
        QProcess peakProc;
        peakProc.start(program, cmdPeak);
        peakProc.waitForFinished(-1);
        QString stderr = peakProc.readAllStandardError();

        double currentPeak = 0.0;
        std::regex rx("max_volume:\\s*([-\\d.]+)");
        std::smatch match;
        std::string s = stderr.toStdString();
        if (std::regex_search(s, match, rx)){
            currentPeak = std::stod(match[1]);
        }else{
            errorMsg = "Impossibile trovare max_volume";
        }

        double gain = targetPeak - currentPeak;

        // Applica gain
        QStringList cmdNorm = {"-i", inputFile, "-af", QString("volume=%1dB").arg(gain), "-map", "0:v?", "-map", "0:a:0?", "-map", "0:s?",
                                "-map", "0:d?", "-map", "0:t?", "-c:v", "copy", "c:a", "libmp3lame"};
        cmdNorm.append(ffmpegParams);
        cmdNorm << "-y" << tempFile;

        QProcess normProc;
        normProc.start(program, cmdNorm);
        normProc.waitForFinished(-1);
        if(normProc.exitCode() != 0){
            errorMsg = "ffmpeg normalizzazione fallito";
        }else{
            QFile::remove(outputFile);
            QFile::rename(tempFile, outputFile);
            success = true;
        }

        cb(success, errorMsg);
    }

    private:
        QString inputFile;
        QString outputFile;
        double targetPeak;
        QStringList ffmpegParams;
        std::function<void(bool,const QString&)> cb;
};

// Main Window
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent){
    auto central = new QWidget(this);
    auto layout = new QVBoxLayout(central);

    // Input
    auto inLayout = new QHBoxLayout();
    inputEntry = new QLineEdit();
    inputButton = new QPushButton();
    inLayout->addWidget(inputEntry);
    inLayout->addWidget(inputButton);
    layout->addLayout(inLayout);
    connect(inputButton, &QPushButton::clicked, this, &MainWindow::browseInput);

    // Output
    auto outLayout = new QHBoxLayout();
    outputEntry = new QLineEdit();
    outputButton = new QPushButton();
    outLayout->addWidget(outputEntry);
    outLayout->addWidget(outputButton);
    layout->addLayout(outLayout);
    connect(outputButton, &QPushButton::clicked, this, &MainWindow::browseOutput);

    // Picco target
    auto peakLayout = new QHBoxLayout();
    peakLayout->addWidget(new QLabel("Picco target (dB):"));
    peakEntry = new QLineEdit("0.0");
    peakLayout->addWidget(peakEntry);
    layout->addLayout(peakLayout);

    // Qualità
    auto qualLayout = new QHBoxLayout();
    qualLayout->addWidget(new QLabel("Qualità MP3:"));
    qualityCombo = new QComboBox();
    qualityCombo->addItems({"VBR 0 (massima)", "VBR 5 (media)", "CBR 320k", "CBR 256k", "CBR 192k"});
    qualLayout->addWidget(qualityCombo);
    layout->addLayout(qualLayout);

    // Sovrascrivi
    overwriteCheck = new QCheckBox("Sovrascrivi file esistenti");
    layout->addWidget(overwriteCheck);

    // Start/Stop
    auto ctrlLayout = new QHBoxLayout();
    startButton = new QPushButton("Avvia normalizzazione");
    stopButton = new QPushButton("Stop");
    stopButton->setEnabled(false);
    ctrlLayout->addWidget(startButton);
    ctrlLayout->addWidget(stopButton);
    layout->addLayout(ctrlLayout);
    connect(startButton, &QPushButton::clicked, this, &MainWindow::startNormalization);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::stopNormalization);
}