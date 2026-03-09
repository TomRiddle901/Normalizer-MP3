#pragma once

#include <QMainWindow>
#include <QString>
#include <QColor>
#include <QAtomicInt>
#include <QStringList>

QT_BEGIN_NAMESPACE;
class QPushButton;
class QLabel;
class QTextEdit;
class QProgressBar;
class QLineEdit;
class QComboBox;
class QCheckBox;
QT_END_NAMESPACE;

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        MainWindow(QWidget *parent = nullptr);

    private slots:
        void browseInput();
        void browseOutput();
        void startNormalization();
        void stopNormalization();

    private:
        void logMessage(const QString &msg, const QColor &color = Qt::black);
        void updateProgress();
        void processFiles();
        void normalizeSingleFile(const QString &inputFile, const QString &outputFile, double targetPeak, const QStringList &ffmpegAudioParams);
        void copyID3Tags(const QString &src, const QString &dst);

        // GUI
        QLineEdit *inputEntry;
        QLineEdit *outputEntry;
        QLineEdit *peakEntry;
        QComboBox *qualityCombo;
        QCheckBox *overwriteCheck;
        QPushButton *inputButton;
        QPushButton *outputButton;
        QPushButton *startButton;
        QPushButton *stopbutton;
        QLabel *statusLabel;
        QTextEdit *logText;
        QProgressBar *progressBar;

        // Stato
        QString inputDirPath;
        QString outputDirPath;
        double targetPeak = 0.0;
        QString quality;
        bool overwrite = false;
        bool isRunning = false;
        bool stopFlag = false;

        QStringList mp3Files;
        QAtomicInt processedfiles;
        QStringList failedFiles;
};