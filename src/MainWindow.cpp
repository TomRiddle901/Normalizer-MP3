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
#include <QDirIterator>
#include <QProcess>
#include <QFile>
#include <QThread>
#include <QTimer>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <regex>
#include <functional>

//  MainWindow 

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    auto central = new QWidget(this);
    auto layout = new QVBoxLayout(central);

    // Input
    auto inLayout = new QHBoxLayout();
    inputEntry = new QLineEdit();
    inputButton = new QPushButton("Sfoglia");
    inLayout->addWidget(new QLabel("Cartella input:"));
    inLayout->addWidget(inputEntry);
    inLayout->addWidget(inputButton);
    layout->addLayout(inLayout);
    connect(inputButton, &QPushButton::clicked, this, &MainWindow::browseInput);

    // Output
    auto outLayout = new QHBoxLayout();
    outputEntry = new QLineEdit();
    outputButton = new QPushButton("Sfoglia");
    outLayout->addWidget(new QLabel("Cartella output:"));
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
    logText = new QTextEdit();
    logText->setReadOnly(true);
    layout->addWidget(logText);

    setCentralWidget(central);
    setWindowTitle("Normalizzatore MP3");
    resize(700, 600);
}

// Slots

void MainWindow::browseInput() {
    inputDirPath = QFileDialog::getExistingDirectory(this, "Seleziona cartella di input");
    inputEntry->setText(inputDirPath);
}

void MainWindow::browseOutput() {
    outputDirPath = QFileDialog::getExistingDirectory(this, "Seleziona cartella di output");
    outputEntry->setText(outputDirPath);
}

void MainWindow::stopNormalization() {
    stopFlag = true;
    logMessage("Arresto richiesto...", Qt::red);
}

void MainWindow::logMessage(const QString &msg, const QColor &color) {
    logText->setTextColor(color);
    logText->append(msg);
    logText->ensureCursorVisible();
}

void MainWindow::updateProgress() {
    if (mp3Files.isEmpty()) return;
    int val = processedFiles*100/mp3Files.size();
    progressBar->setValue(val);
}

// Normalizzazione

void MainWindow::startNormalization() {
    if (isRunning) return;

    inputDirPath = inputEntry->text().trimmed();
    outputDirPath = outputEntry->text().trimmed();
    targetPeak = peakEntry->text().toDouble();
    quality = qualityCombo->currentText();
    overwrite = overwriteCheck->isChecked();

    if (inputDirPath.isEmpty() || outputDirPath.isEmpty()) {
        logMessage("Seleziona la cartella di input/output", Qt::red);
        return;
    }

    // Scan ricorsivo MP3
    mp3Files.clear();
    QDirIterator it(inputDirPath, QStringList() << "*.mp3", QDir::Files, QDirIterator::Subdirectories);
    while(it.hasNext()) {
        mp3Files << it.next();
    }

    if(mp3Files.isEmpty()) {
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

    // Parametri ffmpeg qualità
    QStringList ffmpegParams;
    if (quality.startsWith("VBR 0")) ffmpegParams = {"-q:a", "0"};
    else if (quality.startsWith("VBR 5")) ffmpegParams = {"-q:a", "5"};
    else if (quality.startsWith("CBR 320")) ffmpegParams = {"-b:a", "320k"};
    else if (quality.startsWith("CBR 256")) ffmpegParams = {"-b:a", "256k"};
    else if (quality.startsWith("CBR 192")) ffmpegParams = {"-b:a", "192k"};

    // Start normalizzazione 1 file alla volta
    QTimer::singleShot(0, [this, ffmpegParams]() { processNextFile(); });
}

void MainWindow::processNextFile() {
    if (stopFlag || processedFiles >= mp3Files.size()) {
        isRunning = false;
        startButton->setEnabled(true);
        stopButton->setEnabled(false);
        logMessage("Elaborazione completata", Qt::darkMagenta);
        if (!failedFiles.isEmpty()) {
            logMessage(QString("File con errori (%1):").arg(failedFiles.size()), Qt::red);
            for (const QString &f : failedFiles) logMessage("  " + f, Qt::red);
        }
        return;
    }

    QString inputFile = mp3Files[processedFiles];
    QString relPath = QDir(inputDirPath).relativeFilePath(inputFile);
    QString outputFile = QDir(outputDirPath).filePath(relPath);
    QDir().mkpath(QFileInfo(outputFile).path());

    if(QFile::exists(outputFile) && !overwrite) {
        logMessage("Salta (esistente): " + relPath, Qt::blue);
        processedFiles++;
        updateProgress();
        QTimer::singleShot(0, [this]() { processNextFile(); });
        return;
    }

    // Parametri ffmpeg qualità
    QStringList ffmpegParams;
    if (quality.startsWith("VBR 0")) ffmpegParams = {"-q:a", "0"};
    else if (quality.startsWith("VBR 5")) ffmpegParams = {"-q:a", "5"};
    else if (quality.startsWith("CBR 320")) ffmpegParams = {"-b:a", "320k"};
    else if (quality.startsWith("CBR 256")) ffmpegParams = {"-b:a", "256k"};
    else if (quality.startsWith("CBR 192")) ffmpegParams = {"-b:a", "192k"};

    bool success = normalizeSingleFile(inputFile, outputFile, targetPeak, ffmpegParams);

    if (success) logMessage("OK: " + relPath, Qt::green);
    else {
        logMessage("ERRORE: " + relPath, Qt::red);
        failedFiles << relPath;
    }

    processedFiles++;
    updateProgress();

    // Chiamata ricorsiva al file successivo
    QTimer::singleShot(0, [this]() { processNextFile(); });
}

// ------------------------- Funzioni core -------------------------

bool MainWindow::normalizeSingleFile(const QString &inputFile, const QString &outputFile, double targetPeak, const QStringList &ffmpegAudioParams) {
    QString tempFile = outputFile + ".tmp.mp3";
    QString ffmpegProgram = "./thirdparty/ffmpeg/ffmpeg";

    // Rilevazione picco
    QProcess peakProc;
    QStringList cmdPeak = {"-i", inputFile, "-af", "volumedetect", "-f", "null", "-"};
    peakProc.start(ffmpegProgram, cmdPeak);
    peakProc.waitForFinished(-1);
    QString stderrPeak = peakProc.readAllStandardError();

    std::regex rx("max_volume:\\s*([-\\d.]+)");
    std::smatch match;
    double currentPeak = 0.0;
    std::string s = stderrPeak.toStdString();
    if(std::regex_search(s, match, rx)) {
        currentPeak = std::stod(match[1]);
    } else {
        logMessage("Impossibile rilevare max_volume per " + inputFile, Qt::red);
        return false;
    }

    double gain = targetPeak - currentPeak;

    // Applica gain
    QStringList cmdNorm = {"-i", inputFile, "-af", QString("volume=%1dB").arg(gain),
                           "-c:a", "libmp3lame"};
    cmdNorm.append(ffmpegAudioParams);
    cmdNorm << "-y" << tempFile;

    QProcess normProc;
    normProc.start(ffmpegProgram, cmdNorm);
    normProc.waitForFinished(-1);

    if(normProc.exitCode() != 0) {
        logMessage("ffmpeg normalizzazione fallita per " + inputFile, Qt::red);
        return false;
    }

    // Copia tag ID3
    copyID3Tags(inputFile, tempFile);

    // Sostituisci file
    QFile::remove(outputFile);
    if(!QFile::rename(tempFile, outputFile)) {
        logMessage("Errore nel rinominare " + tempFile + " → " + outputFile, Qt::red);
        return false;
    }

    return true;
}

void MainWindow::copyID3Tags(const QString &src, const QString &dst) {
    TagLib::FileRef srcFile(src.toUtf8().constData());
    TagLib::FileRef dstFile(dst.toUtf8().constData());

    if (!srcFile.isNull() && !dstFile.isNull() && srcFile.tag() && dstFile.tag()) {
        TagLib::Tag *s = srcFile.tag();
        TagLib::Tag *d = dstFile.tag();

        d->setTitle(s->title());
        d->setArtist(s->artist());
        d->setAlbum(s->album());
        d->setComment(s->comment());
        d->setGenre(s->genre());
        d->setYear(s->year());
        d->setTrack(s->track());
        dstFile.save();
    } else {
        logMessage("Avviso: copia tag fallita per " + dst, Qt::darkYellow);
    }
}