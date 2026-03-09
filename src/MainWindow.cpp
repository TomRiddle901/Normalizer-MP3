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
    }
};