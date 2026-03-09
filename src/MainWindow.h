#pragma once

#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE;
class QPushButton;
class QLabel;
class QTextEdit;
QT_END_NAMESPACE;

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        MainWindow(QWidget *parent = nullptr);

    private slots:
        void onNormalizeClicked();

    private:
        QPushButton *normalizeButton;
        QLabel *statusLabel;
        QTextEdit *logText;
};