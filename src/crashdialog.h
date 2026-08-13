#pragma once

#include "savefile.h"

#include <QDateTime>
#include <QDialog>

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

private slots:
    void on_cancelButton_clicked();
    void on_sendButton_clicked();

private:
    Ui::CrashDialog *ui;
    QList<SaveFile *> saveFileList;
    QString stdErrorString;
    QDateTime gameStartTime;
};
