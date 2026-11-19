#pragma once

#include "savefile.h"

#include <QDateTime>
#include <QDialog>

#include <optional>

namespace Ui {
class CrashDialog;
}

class CrashDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CrashDialog(QWidget *parent = nullptr);
    ~CrashDialog();

    void setStdErrorString(QString stdErrorString);

    /**
     * The moment the game was launched. Without it, keeperfx.log cannot be
     * attributed to this run and so is left out of the report entirely.
     */
    void setGameStartTime(QDateTime gameStartTime);

    /** How the game process ended, for the report. */
    void setExitCode(int exitCode);

private slots:
    void on_closeButton_clicked();
    void on_reportButton_clicked();

private:
    Ui::CrashDialog *ui;
    QList<SaveFile *> saveFileList;
    QString stdErrorString;
    QDateTime gameStartTime;
    std::optional<int> exitCode;

    // The replay the game was recording or playing when it crashed, from this run's log
    QString replayPath;
    int replayTurn = -1;
    bool replayPlayback = false;
    void findReplay();
};
