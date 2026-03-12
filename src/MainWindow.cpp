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
#include <QFileInfo>
#include <QThread>
#include <QTimer>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QtMath>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

#include <taglib/fileref.h>
#include <taglib/tag.h>

// --- Costruttore MainWindow ---
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

// --- Slots ---
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

// --- Normalizzazione ---
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
    setProperty("dispatchedFiles", 0);
    setProperty("activeJobs", 0);
    setProperty("maxParallelJobs", qMax(1, QThread::idealThreadCount()));
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    progressBar->setValue(0);

    QTimer::singleShot(0, [this]() { processNextFile(); });
}

void MainWindow::processNextFile() {
    int dispatchedFiles = property("dispatchedFiles").toInt();
    int activeJobs = property("activeJobs").toInt();
    int maxParallelJobs = property("maxParallelJobs").toInt();

    if ((stopFlag || processedFiles >= mp3Files.size()) && activeJobs == 0) {
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

    while (!stopFlag && activeJobs < maxParallelJobs && dispatchedFiles < mp3Files.size()) {
        const QString inputFile = mp3Files[dispatchedFiles];
        const QString relPath = QDir(inputDirPath).relativeFilePath(inputFile);
        const QString outputFile = QDir(outputDirPath).filePath(relPath);
        QDir().mkpath(QFileInfo(outputFile).path());

        dispatchedFiles++;

        if(QFile::exists(outputFile) && !overwrite) {
            logMessage("Salta (esistente): " + relPath, Qt::blue);
            processedFiles++;
            updateProgress();
            continue;
        }

        QStringList ffmpegParams;
        if (quality.startsWith("VBR 0")) ffmpegParams = {"-q:a", "0"};
        else if (quality.startsWith("VBR 5")) ffmpegParams = {"-q:a", "5"};
        else if (quality.startsWith("CBR 320")) ffmpegParams = {"-b:a", "320k"};
        else if (quality.startsWith("CBR 256")) ffmpegParams = {"-b:a", "256k"};
        else if (quality.startsWith("CBR 192")) ffmpegParams = {"-b:a", "192k"};

        auto *watcher = new QFutureWatcher<bool>(this);
        activeJobs++;
        setProperty("activeJobs", activeJobs);
        setProperty("dispatchedFiles", dispatchedFiles);

        connect(watcher, &QFutureWatcher<bool>::finished, this,
                [this, watcher, relPath]() {
                    const bool success = watcher->result();
                    int jobs = property("activeJobs").toInt();
                    setProperty("activeJobs", qMax(0, jobs - 1));

                    if (success) logMessage("OK: " + relPath, Qt::green);
                    else {
                        logMessage("ERRORE: " + relPath, Qt::red);
                        failedFiles << relPath;
                    }

                    processedFiles++;
                    updateProgress();
                    watcher->deleteLater();
                    QTimer::singleShot(0, this, [this]() { processNextFile(); });
                });

        watcher->setFuture(QtConcurrent::run([this, inputFile, outputFile, ffmpegParams]() {
            return normalizeSingleFile(inputFile, outputFile, targetPeak, ffmpegParams);
        }));
    }

    setProperty("dispatchedFiles", dispatchedFiles);
    setProperty("activeJobs", activeJobs);

    if ((stopFlag || processedFiles >= mp3Files.size()) && activeJobs == 0) {
        QTimer::singleShot(0, this, [this]() { processNextFile(); });
    }
}

// --- Funzioni core ---
bool MainWindow::normalizeSingleFile(const QString &inputFile, const QString &outputFile, double targetPeak, const QStringList &ffmpegAudioParams) {
    QString tempFile = outputFile + ".tmp.mp3";
    QString ffmpegProgram;

    const QString appDirFfmpeg = QDir(QCoreApplication::applicationDirPath()).filePath("thirdparty/ffmpeg/ffmpeg");
    const QString cwdFfmpeg = QDir::current().filePath("thirdparty/ffmpeg/ffmpeg");
    if (QFileInfo::exists(appDirFfmpeg) && QFileInfo(appDirFfmpeg).isExecutable()) {
        ffmpegProgram = appDirFfmpeg;
    } else if (QFileInfo::exists(cwdFfmpeg) && QFileInfo(cwdFfmpeg).isExecutable()) {
        ffmpegProgram = cwdFfmpeg;
    } else {
        // fallback: ffmpeg nel PATH di sistema
        ffmpegProgram = "ffmpeg";
    }

    // --- Normalizzazione (single-pass, più veloce del flow a 2 passaggi) ---
    // Converte target in dBFS (es. -1.0 dB) nel valore lineare richiesto da dynaudnorm (0..1).
    const double peakLinear = qPow(10.0, targetPeak / 20.0);
    const double boundedPeakLinear = qBound(0.1, peakLinear, 1.0);

    QStringList cmdNorm = {
        // In modalità multi-file concorrente è più efficiente usare 1 thread per processo
        // e lasciare alla concorrenza il saturare tutti i core.
        "-threads", "1",
        "-filter_threads", "1",
        "-i", inputFile,
        // dynaudnorm evita il pass di analisi volumedetect e riduce il tempo totale per file.
        "-af", QString("dynaudnorm=p=%1:m=50:s=12").arg(boundedPeakLinear, 0, 'f', 4),
        "-map_metadata", "0",
        "-vn", "-sn", "-dn",
        "-c:a", "libmp3lame"
    };
    cmdNorm.append(ffmpegAudioParams);
    cmdNorm << "-y" << tempFile;

    QProcess normProc;
    normProc.start(ffmpegProgram, cmdNorm);
    if (!normProc.waitForFinished(-1) || normProc.error() == QProcess::FailedToStart) {
        return false;
    }

    if(normProc.exitStatus() != QProcess::NormalExit || normProc.exitCode() != 0) {
        return false;
    }

    QFile::remove(outputFile);
    if(!QFile::rename(tempFile, outputFile)) {
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