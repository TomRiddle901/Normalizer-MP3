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

    // Progress bar
    progressBar = new QProgressBar();
    layout->addWidget(progressBar);

    // Log
    logText = new QTextEdit;
    logText->setReadOnly(true);
    layout->addWidget(logText);

    setCentralWidget(central);
    setWindowTitle("Normalizzatore MP3");
    resize(700, 600);
}

// Slots
void MainWindow::browseInput(){
    inputDirPath = QFileDialog::getExistingDirectory(this, "Seleziona cartella di input");
    inputEntry->setText(inputDirPath);
}
void MainWindow::browseOutput(){
    outputDirPath = QFileDialog::getExistingDirectory(this, "Seleziona cartella di output");
    outputEntry->setText(outputDirPath);
}
void MainWindow::stopNormalization(){
    stopFlag = true;
    logMessage("Arresto richiesto...",Qt::red);
}

void MainWindow::logMessage(const QString &msg, const QColor &color){
    logText->setTextColor(color);
    logText->append(msg);
    logText->ensureCursorVisible();
}

void MainWindow::updateProgress(){
    if (mp3Files.isEmpty()){return;}

    int val = processedFiles*100/mp3Files.size();
    progressBar->setValue(val);
}

void MainWindow::startNormalization(){
    if (isRunning){return;}

    inputDirPath = inputEntry->text().trimmed();
    outputDirPath = outputEntry->text().trimmed();
    targetPeak = peakEntry->text().toDouble();
    quality = qualityCombo->currentText();
    overwrite = overwriteCheck->isChecked();

    if (inputDirPath.isEmpty() || outputDirPath.isEmpty()){
        logMessage("Seleziona la cartella di input/output", Qt::red);
        return;
    }

    QDir inDir(inputDirPath);
    mp3Files = inDir.entryList(QStringList() << "*.mp3", QDir::Files|QDir::NoDotAndDotDot);
    if (mp3Files.isEmpty()){
        logMessage("Nessun MP3 trovato", Qt::red);
        return;
    }

    processedFiles = 0;
    failedFiles.clear();
    stopFlag = false;
    isRunning = true;
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    progressBar->setValue(0);

    QStringList ffmpegParams;
    if (quality.startsWith("VBR 0")){
        ffmpegParams = {"-q:a", "0"};
    }else if (quality.startsWith("VBR 5")){
        ffmpegParams = {"-q:a", "5"};
    }else if (quality.startsWith("CBR 320")){
        ffmpegParams = {"-b:a", "320k"};
    }else if (quality.startsWith("CBR 256")){
        ffmpegParams = {"-b:a", "256k"};
    }else if (quality.startsWith("CBR 192")){
        ffmpegParams = {"-b:a", "192k"};
    }

    for (const QString &f : mp3Files){
        if (stopFlag){break;}
        QString inputFile = inputDirPath + "/" + f;
        QString outputFile = outputDirPath + "/" + f;
        if (QFile::exists(outputFile) && !overwrite){
            logMessage("Salta (esistente): " + f, Qt::blue);
            processedFiles++;
            updateProgress();
            continue;
        }
        auto cb = [this,f](bool ok, const QString &err){
            if (ok) {
                logMessage("OK: " + f, Qt::green);
            }else{
                logMessage("ERRORE: " + f + err, Qt::red);
                failedFiles.append(f);
            }

            processedFiles++;
            updateProgress();
            if (processedFiles >= mp3Files.size()){
                isRunning = false;
                startButton->setEnabled(true);
                stopButton->setEnabled(false);
                logMessage("Elaborazione completata", Qt::darkMagenta);
            }
        };
        NormalizeTask *task = new NormalizeTask(inputFile, outputFile, targetPeak, ffmpegParams, cb);
        task->setAutoDelete(true);
        QThreadPool::globalInstance()->start(task);
    }   
}