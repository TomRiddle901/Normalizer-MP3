#include "MainWindow.h"

#include <QVBoxLayout>
#include <QWidget>
#include <QFileDialog>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QFile>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>

#include <taglib/fileref.h>
#include <taglib/tag.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      totalFiles(0),
      processedFiles(0)
{
    setAcceptDrops(true);

    QWidget *central = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout;

    selectButton = new QPushButton("Seleziona cartella MP3");
    logBox = new QTextEdit;
    progressBar = new QProgressBar;

    logBox->setReadOnly(true);
    progressBar->setValue(0);

    layout->addWidget(selectButton);
    layout->addWidget(progressBar);
    layout->addWidget(logBox);

    central->setLayout(layout);
    setCentralWidget(central);

    connect(selectButton, &QPushButton::clicked,
            this, &MainWindow::selectFolder);

    setWindowTitle("MP3 Normalizer");
    resize(650,450);
}

void MainWindow::logMessage(const QString &msg)
{
    logBox->append(msg);
}

QStringList MainWindow::scanMp3Recursive(const QString &path)
{
    QStringList files;

    QDirIterator it(path,
                    QStringList() << "*.mp3",
                    QDir::Files,
                    QDirIterator::Subdirectories);

    while(it.hasNext())
        files << it.next();

    return files;
}

void MainWindow::selectFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this,"Seleziona cartella");

    if(dir.isEmpty())
        return;

    QStringList files = scanMp3Recursive(dir);

    totalFiles = files.size();
    processedFiles = 0;

    progressBar->setMaximum(totalFiles);

    for(const QString &file : files)
        processFile(file);

    logMessage("Completato.");
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();

    QStringList files;

    for(const QUrl &url : urls)
    {
        QFileInfo info(url.toLocalFile());

        if(info.isDir())
            files += scanMp3Recursive(info.absoluteFilePath());
        else if(info.suffix().toLower() == "mp3")
            files << info.absoluteFilePath();
    }

    totalFiles = files.size();
    processedFiles = 0;

    progressBar->setMaximum(totalFiles);

    for(const QString &file : files)
        processFile(file);
}

void MainWindow::processFile(const QString &inputFile)
{
    QFileInfo info(inputFile);

    QString outputFile =
        info.absolutePath() + "/" +
        info.completeBaseName() + "_normalized.mp3";

    logMessage("Processing: " + inputFile);

    if(normalizeSingleFile(inputFile, outputFile))
    {
        copyID3Tags(inputFile, outputFile);
        logMessage("OK");
    }
    else
    {
        logMessage("Errore");
    }

    processedFiles++;
    progressBar->setValue(processedFiles);
}

bool MainWindow::normalizeSingleFile(const QString &inputFile,
                                     const QString &outputFile)
{
    QString tempFile = outputFile + ".tmp";

    QProcess proc;

    QString program = "thirdparty/ffmpeg/ffmpeg";

    QStringList args;
    args << "-i"
         << inputFile
         << "-filter:a"
         << "loudnorm"
         << "-y"
         << tempFile;

    proc.start(program,args);
    proc.waitForFinished(-1);

    if(proc.exitCode() != 0)
    {
        logMessage("ffmpeg errore");
        return false;
    }

    QFile::remove(outputFile);
    QFile::rename(tempFile, outputFile);

    return true;
}

void MainWindow::copyID3Tags(const QString &inputFile,
                             const QString &outputFile)
{
    TagLib::FileRef src(inputFile.toStdString().c_str());
    TagLib::FileRef dst(outputFile.toStdString().c_str());

    if(src.isNull() || dst.isNull())
        return;

    TagLib::Tag *srcTag = src.tag();
    TagLib::Tag *dstTag = dst.tag();

    dstTag->setTitle(srcTag->title());
    dstTag->setArtist(srcTag->artist());
    dstTag->setAlbum(srcTag->album());
    dstTag->setGenre(srcTag->genre());
    dstTag->setYear(srcTag->year());
    dstTag->setTrack(srcTag->track());

    dst.file()->save();
}