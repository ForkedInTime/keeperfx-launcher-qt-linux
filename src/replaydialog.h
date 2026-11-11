#pragma once

#include <QDialog>

namespace Ui { class ReplayDialog; }

class ReplayDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReplayDialog(QWidget *parent = nullptr);
    ~ReplayDialog();

    QString getReplayFileName();

private slots:
    void on_cancelButton_clicked();
    void on_startButton_clicked();
    void on_copyButton_clicked();

    void updateButtons();

    void showContextMenu(const QPoint &pos);

private:
    Ui::ReplayDialog *ui;

    QString replayFileName;
};
