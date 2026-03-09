#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QStringList>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void processFile(const QString &inputFile);
    bool normalizeSingleFile(const QString &inputFile, const QString &outputFile);
    void copyID3Tags(const QString &inputFile, const QString &outputFile);
    void logMessage(const QString &msg);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void selectFolder();

private:
    QStringList scanMp3Recursive(const QString &path);

    QTextEdit *logBox;
    QPushButton *selectButton;
    QProgressBar *progressBar;

    int totalFiles;
    int processedFiles;
};

#endif