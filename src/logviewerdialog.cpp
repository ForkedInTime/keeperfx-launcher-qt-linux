#include "logviewerdialog.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPushButton>
#include <QScrollBar>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Both logs sit beside the launcher binary, in the game directory: the launcher
// writes "<its own basename>.log" (see logger.cpp) and the engine writes
// keeperfx.log next to itself.
QString gameDirectory()
{
    return QCoreApplication::applicationDirPath();
}

QString launcherLogPath()
{
    return gameDirectory() + QDir::separator()
           + QFileInfo(QCoreApplication::applicationFilePath()).baseName() + ".log";
}

QString gameLogPath()
{
    return gameDirectory() + QDir::separator() + "keeperfx.log";
}

// Reading the tail is deliberate. A heavylog session can reach tens of megabytes,
// and loading that into a QPlainTextEdit would freeze the launcher at the exact
// moment the user is already unhappy. The end of a log is the part that explains a
// failure anyway.
constexpr qint64 MAX_LOG_BYTES = 512 * 1024;

QString readLogTail(const QString &path)
{
    QFile file(path);
    if (file.exists() == false) {
        return QObject::tr("No log file yet at:\n%1").arg(path);
    }
    if (file.open(QIODevice::ReadOnly | QIODevice::Text) == false) {
        return QObject::tr("Could not open:\n%1\n\n%2").arg(path, file.errorString());
    }

    QString notice;
    if (file.size() > MAX_LOG_BYTES) {
        file.seek(file.size() - MAX_LOG_BYTES);
        notice = QObject::tr("[ showing the last %1 KB of %2 KB ]\n\n")
                     .arg(MAX_LOG_BYTES / 1024)
                     .arg(file.size() / 1024);
    }
    const QString contents = QString::fromUtf8(file.readAll());
    file.close();
    return notice + contents;
}

} // namespace

LogViewerDialog::LogViewerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Logs", "Log viewer window title"));
    resize(900, 560);

    tabs = new QTabWidget(this);
    addLogTab(tr("Launcher", "Log tab"), launcherLogPath());
    addLogTab(tr("Game", "Log tab"), gameLogPath());

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);

    // Copy comes first because it is what someone reporting a problem actually needs;
    // the next step is almost always pasting this into an issue or a chat.
    QPushButton *copyButton = buttons->addButton(tr("Copy", "Log viewer button"),
                                                 QDialogButtonBox::ActionRole);
    QPushButton *folderButton = buttons->addButton(tr("Open folder", "Log viewer button"),
                                                   QDialogButtonBox::ActionRole);
    QPushButton *refreshButton = buttons->addButton(tr("Refresh", "Log viewer button"),
                                                    QDialogButtonBox::ActionRole);

    connect(copyButton, &QPushButton::clicked, this, &LogViewerDialog::copyCurrentToClipboard);
    connect(folderButton, &QPushButton::clicked, this, &LogViewerDialog::openContainingFolder);
    connect(refreshButton, &QPushButton::clicked, this, &LogViewerDialog::refresh);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    refresh();

    // Keep following the file while the dialog is open, so a user can leave it up,
    // reproduce the problem, and watch what the game writes.
    tailTimer = new QTimer(this);
    tailTimer->setInterval(2000);
    connect(tailTimer, &QTimer::timeout, this, &LogViewerDialog::refresh);
    tailTimer->start();
}

void LogViewerDialog::addLogTab(const QString &title, const QString &path)
{
    QPlainTextEdit *view = new QPlainTextEdit(this);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    logTabs.append({path, view});
    tabs->addTab(view, title);
}

LogViewerDialog::LogTab *LogViewerDialog::currentTab()
{
    const int index = tabs->currentIndex();
    if (index < 0 || index >= logTabs.size()) {
        return nullptr;
    }
    return &logTabs[index];
}

void LogViewerDialog::refresh()
{
    for (LogTab &tab : logTabs) {
        // Only touch the widget when the text actually changed, or the periodic
        // refresh would fight the user's scrolling and text selection.
        const QString text = readLogTail(tab.path);
        if (tab.view->toPlainText() == text) {
            continue;
        }
        const bool wasAtBottom = tab.view->verticalScrollBar()->value()
                                 == tab.view->verticalScrollBar()->maximum();
        tab.view->setPlainText(text);
        if (wasAtBottom) {
            tab.view->verticalScrollBar()->setValue(tab.view->verticalScrollBar()->maximum());
        }
    }
}

void LogViewerDialog::copyCurrentToClipboard()
{
    if (LogTab *tab = currentTab()) {
        QApplication::clipboard()->setText(tab->view->toPlainText());
    }
}

void LogViewerDialog::openContainingFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(gameDirectory()));
}

void LogViewerDialog::showLauncherLog(QWidget *parent)
{
    LogViewerDialog dialog(parent);
    dialog.tabs->setCurrentIndex(0);
    dialog.exec();
}

void LogViewerDialog::showGameLog(QWidget *parent)
{
    LogViewerDialog dialog(parent);
    dialog.tabs->setCurrentIndex(1);
    dialog.exec();
}
