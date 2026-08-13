#pragma once

#include <QDateTime>
#include <QVariant>
#include <QString>
#include <QObject>
#include <QProcess>

class Game : public QObject
{
    Q_OBJECT

public:
    enum StartType {
        NORMAL,
        HVLOG,
        DIRECT_CONNECT,
        MAP,
        CAMPAIGN,
        LOAD_SAVE,
        LOAD_PACKETSAVE,
        START_WITHOUT_MODS,
    };

    explicit Game(QWidget *parent = nullptr);

    static QString getStringFromStartType(StartType startType);

    bool start(StartType startType,
               QVariant data1 = QVariant(),
               QVariant data2 = QVariant(),
               QVariant data3 = QVariant());

    QString getErrorString();

signals:
    void gameEnded(int exitCode, QProcess::ExitStatus exitStatus);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QWidget *parentWidget;
    QProcess *process;
    QString errorString;
    // Stamped immediately before the process is launched, so a keeperfx.log
    // left over from an earlier session can be told apart from this run's.
    QDateTime processStartedAt;
};
