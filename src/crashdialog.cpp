#include "crashdialog.h"
#include "ui_crashdialog.h"

#include "archiver.h"
#include "crashcontext.h"
#include "kfxversion.h"
#include "savefile.h"
#include "settings.h"
#include "version.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLayout>
#include <QMessageBox>
#include <QScreen>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>

// Crash reports go to this edition's own issue tracker and nowhere else.
//
// The upstream dialog POSTed the report -- description, contact details,
// keeperfx.cfg, keeperfx.log and optionally a save -- to keeperfx.net. This one
// sends nothing anywhere. It saves the report as a zip on the player's computer
// and opens a new issue on our GitHub page with the details filled in; the
// player attaches the zip and submits the issue themselves.

// How much of the end of keeperfx.log is read for the issue text. The full log
// goes into the zip; the issue only quotes its end, where a crash is.
static const qint64 LOG_TAIL_BYTES = 64 * 1024;

// The issue text travels in the URL, so it is kept well inside what browsers
// and GitHub accept once percent-encoded.
static const int ISSUE_BODY_MAX_CHARS = 3000;

CrashDialog::CrashDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CrashDialog)
    , saveFileList(SaveFile::getAll())
{
    ui->setupUi(this);

    // Load save file list
    if (saveFileList.empty() == true) {
        ui->saveFileComboBox->setDisabled(true);
        ui->saveFileComboBox->setPlaceholderText(tr("No saves found", "Menu Placeholder"));
    } else {
        ui->saveFileComboBox->addItem(tr("None", "Default Savefile"));
        for (SaveFile *saveFile : std::as_const(saveFileList)) {
            ui->saveFileComboBox->addItem(saveFile->toString());
        }
    }

    // Handle information label
    ui->crashInfoLabel2->setTextFormat(Qt::RichText);
    ui->crashInfoLabel2->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->crashInfoLabel2->setOpenExternalLinks(true);
    ui->crashInfoLabel2->setText(ui->crashInfoLabel2->text().arg(
        QString("<a href=\"%1\">%1</a>").arg(CrashContext::ISSUE_TRACKER_URL)));

    // Not resizable, but as tall as the text needs at the width the dialog was
    // designed for. Fixing the size straight from the .ui file clipped the
    // wrapped first paragraph whenever the desktop scaled fonts up. Never
    // taller than the screen either, or the buttons end up below it: the
    // description box gives up the room first, the wrapped text keeps it.
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);
    const int dialogWidth = width();
    int dialogHeight = layout()->hasHeightForWidth() ? layout()->totalHeightForWidth(dialogWidth)
                                                     : sizeHint().height();
    if (const QScreen *dialogScreen = screen()) {
        dialogHeight = qMin(dialogHeight, dialogScreen->availableGeometry().height() * 9 / 10);
    }
    dialogHeight = qMax(dialogHeight, minimumSizeHint().height());
    setFixedSize(dialogWidth, dialogHeight);
}

CrashDialog::~CrashDialog()
{
    delete ui;
}

void CrashDialog::setStdErrorString(QString stdError)
{
    this->stdErrorString = stdError;
}

void CrashDialog::setGameStartTime(QDateTime startTime)
{
    this->gameStartTime = startTime;
}

void CrashDialog::on_closeButton_clicked()
{
    this->close();
}

// Distribution, kernel, session and how the game was installed: what a crash
// report needs to be reproduced on another machine.
static QString systemDescription()
{
    QStringList parts;
    parts << QSysInfo::prettyProductName();
    parts << QStringLiteral("kernel %1").arg(QSysInfo::kernelVersion());

    const QString session = qEnvironmentVariable("XDG_SESSION_TYPE");
    if (session.isEmpty() == false) {
        parts << session;
    }
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    if (desktop.isEmpty() == false) {
        parts << desktop;
    }

    if (qEnvironmentVariableIsSet("APPIMAGE")) {
        parts << QStringLiteral("AppImage");
    } else if (qEnvironmentVariableIsSet("FLATPAK_ID")) {
        parts << QStringLiteral("Flatpak");
    }

    parts << QStringLiteral("Qt %1").arg(QString::fromLatin1(qVersion()));
    return parts.join(QStringLiteral(", "));
}

// The first folder the report can be written to: the game folder, which is
// where the player looks for the game's files, then Documents, then /tmp.
static QString reportFolder()
{
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/crash-reports",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/KeeperFX crash reports",
        QDir::tempPath() + "/keeperfx-crash-reports",
    };

    for (const QString &folder : candidates) {
        if (QDir().mkpath(folder) && QFileInfo(folder).isWritable()) {
            return folder;
        }
    }
    return QString();
}

// Copy a file into the staging folder under the name it gets inside the zip.
static bool stageFile(const QTemporaryDir &staging, const QString &sourcePath, const QString &name, QStringList &staged)
{
    if (QFileInfo(sourcePath).isFile() == false) {
        return false;
    }

    const QString target = staging.filePath(name);
    if (QFile::copy(sourcePath, target) == false) {
        qWarning() << "Crash report: could not copy" << sourcePath;
        return false;
    }

    staged << target;
    return true;
}

static QString readTail(const QString &path, qint64 maxBytes)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) == false) {
        return QString();
    }
    if (file.size() > maxBytes) {
        file.seek(file.size() - maxBytes);
        file.readLine(); // start on a whole line
    }
    return QString::fromUtf8(file.readAll());
}

void CrashDialog::on_reportButton_clicked()
{
    this->ui->reportButton->setDisabled(true);

    const QString homeDir = QDir::homePath();
    const QDateTime now = QDateTime::currentDateTime();
    const QString archiveName = QStringLiteral("keeperfx-crash-%1.zip").arg(now.toString("yyyyMMdd-HHmmss"));

    CrashContext::ReportFacts facts;
    facts.gameVersion = KfxVersion::currentVersion.fullString;
    facts.launcherVersion = QString(LAUNCHER_VERSION);
    facts.system = systemDescription();
    facts.description = ui->infoTextEdit->toPlainText().trimmed();
    facts.gameOutput = CrashContext::hideHomeDir(this->stdErrorString, homeDir);

    // Everything for the zip is gathered in a scratch folder first, under the
    // names it will have inside the zip.
    QTemporaryDir staging;
    QStringList staged;
    QString logNote;

    // keeperfx.log -- but only if this run wrote it. The engine truncates the
    // log as it starts, so a log older than the launch is a leftover from an
    // earlier session: attaching it describes a run that did not fail and hides
    // the fact that the failing run logged nothing at all.
    const QString kfxLogPath = QCoreApplication::applicationDirPath() + "/keeperfx.log";
    const QDateTime kfxLogModified = QFileInfo(kfxLogPath).lastModified();

    if (QFileInfo::exists(kfxLogPath) == false) {
        logNote = QStringLiteral("There was no keeperfx.log in the game folder.");

    } else if (CrashContext::shouldAttachLog(kfxLogModified, this->gameStartTime) == false) {
        qWarning() << "Not adding keeperfx.log to the crash report: last modified"
                   << kfxLogModified.toString(Qt::ISODate)
                   << "but the game was started at"
                   << this->gameStartTime.toString(Qt::ISODate);

        // Say so in the report itself, so nobody reads a missing log as a log
        // that was empty.
        logNote = QStringLiteral("The game produced no log for this run; the keeperfx.log in the "
                                 "game folder predates it and was left out.");

    } else if (staging.isValid() && stageFile(staging, kfxLogPath, "keeperfx.log", staged)) {
        facts.logTail = CrashContext::hideHomeDir(readTail(kfxLogPath, LOG_TAIL_BYTES), homeDir);
        logNote = QStringLiteral("keeperfx.log is in this zip.");
    }

    if (staging.isValid()) {
        // The game's settings
        QFile kfxConfigFile = Settings::getKfxConfigFile();
        stageFile(staging, kfxConfigFile.fileName(), "keeperfx.cfg", staged);

        // The launcher's own log: the game's error output on a failed start is
        // only there.
        const QString launcherLogPath = QCoreApplication::applicationDirPath() + "/"
                                        + QFileInfo(QCoreApplication::applicationFilePath()).baseName() + ".log";
        stageFile(staging, launcherLogPath, QFileInfo(launcherLogPath).fileName(), staged);

        // The save the player picked
        const int saveFileIndex = ui->saveFileComboBox->currentIndex();
        if (saveFileIndex > 0) { // -1 = nothing selected, 0 = "None"
            SaveFile *saveFile = saveFileList.at(saveFileIndex - 1);
            if (saveFile) {
                const QString savePath = saveFile->file.fileName();
                stageFile(staging, savePath, QFileInfo(savePath).fileName(), staged);
            }
        }

        // The report itself, in full: the issue text is cut to fit a URL.
        QFile reportText(staging.filePath("crash-report.txt"));
        if (reportText.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QString text;
            text += QStringLiteral("KeeperFX crash report, %1\n\n").arg(now.toString(Qt::ISODate));
            text += QStringLiteral("Game: %1\nLauncher: %2\nSystem: %3\n%4\n\n")
                        .arg(facts.gameVersion, facts.launcherVersion, facts.system, logNote);
            text += QStringLiteral("What happened:\n%1\n\n")
                        .arg(facts.description.isEmpty() ? QStringLiteral("(not described)") : facts.description);
            text += QStringLiteral("Game output:\n%1\n")
                        .arg(facts.gameOutput.isEmpty() ? QStringLiteral("(none)") : facts.gameOutput);
            reportText.write(text.toUtf8());
            reportText.close();
            staged << reportText.fileName();
        }
    }

    // Pack it
    QString archivePath;
    const QString folder = reportFolder();
    if (staging.isValid() && folder.isEmpty() == false && staged.isEmpty() == false) {
        archivePath = QDir(folder).filePath(archiveName);
        if (Archiver::compressFiles(staged, archivePath.toStdString(), bit7z::BitFormat::Zip) == false
            || QFileInfo::exists(archivePath) == false) {
            qWarning() << "Crash report: could not write" << archivePath;
            archivePath.clear();
        }
    }
    facts.archiveName = archivePath.isEmpty() ? QString() : archiveName;

    // The issue
    QString title = facts.description.section(QLatin1Char('\n'), 0, 0).trimmed();
    if (title.size() > 80) {
        title = title.left(77) + QStringLiteral("...");
    }
    title = title.isEmpty() ? QStringLiteral("Crash in %1").arg(facts.gameVersion)
                            : QStringLiteral("Crash: %1").arg(title);
    const QUrl issueUrl = CrashContext::newIssueUrl(title, CrashContext::issueBody(facts, ISSUE_BODY_MAX_CHARS));

    if (archivePath.isEmpty() == false) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    }
    const bool browserOpened = QDesktopServices::openUrl(issueUrl);

    QString message;
    if (archivePath.isEmpty() == false) {
        message += tr("The crash report was saved as:\n%1\n\n", "MessageBox Text").arg(QDir::toNativeSeparators(archivePath));
    } else {
        message += tr("The crash report file could not be saved. "
                      "Please attach keeperfx.log from your game folder to the issue instead.\n\n",
                      "MessageBox Text");
    }
    if (browserOpened) {
        message += tr("A new issue on our GitHub page has opened in your browser with the details "
                      "filled in. Attach the report file to it and click Submit.",
                      "MessageBox Text");
    } else {
        message += tr("Please report it on our GitHub page:\n%1", "MessageBox Text")
                       .arg(QString(CrashContext::ISSUE_TRACKER_URL) + "/new");
    }
    message += tr("\n\nNothing has been sent anywhere. The report reaches us only when you submit the issue.",
                  "MessageBox Text");

    QMessageBox::information(this, tr("Crash Report", "MessageBox Title"), message);

    // Accept crash dialog (close it)
    this->accept();
}
