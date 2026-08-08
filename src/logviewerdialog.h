#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTimer>

// Shows the launcher's own log and the engine's log inside the launcher.
//
// Both files already existed; there was simply no way to read them without knowing
// where the game directory is. That matters most at the moment something fails --
// the failure dialogs can open this directly, so "it didn't work" becomes something
// the user can actually read and paste into a bug report.
//
// A dialog rather than a tab in the main window: the launcher is a front door, not a
// debugging workbench, and this only wants to exist when something has gone wrong.
class LogViewerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogViewerDialog(QWidget *parent = nullptr);

    // Opens the viewer with the launcher log selected.
    static void showLauncherLog(QWidget *parent);
    // Opens the viewer with the game log selected -- what "the game won't start"
    // actually needs.
    static void showGameLog(QWidget *parent);

private slots:
    void refresh();
    void copyCurrentToClipboard();
    void openContainingFolder();

private:
    struct LogTab
    {
        QString path;
        QPlainTextEdit *view = nullptr;
    };

    void addLogTab(const QString &title, const QString &path);
    LogTab *currentTab();

    QTabWidget *tabs = nullptr;
    QList<LogTab> logTabs;
    QTimer *tailTimer = nullptr;
};
