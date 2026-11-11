// Test for the Settings window: in every language the launcher ships, at the desktop fonts players
// use, every label, checkbox, button, dropdown entry and tab is shown whole at the size the window
// opens with. A long text may wrap and a tab may scroll down; nothing may be cut off.
//
// This runs the real SettingsDialog, so it is built from the whole launcher but main.cpp (CMake
// target test_settingsfit, not built by default):
//   cmake --build build --target test_settingsfit && build/test_settingsfit
// or LAUNCHER_BUILD_DIR=build tests/run.sh
//
// Settings are read from files next to the program, so the test first copies itself into a temporary
// folder with a version.txt and empty .cfg files, then runs that copy once per language and font, each
// in its own process (a fresh translator and font each time), offscreen.
#include "settingsdialog.h"
#include "kfxversion.h"
#include "launcheroptions.h"
#include "settings.h"
#include "translator.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QGroupBox>
#include <QScreen>
#include <QLabel>
#include <QProcess>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleFactory>
#include <QStyleOptionButton>
#include <QStyleOptionComboBox>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextDocument>
#include <cstdio>

namespace {

const char *const LANGUAGES[] = {"en", "cs", "de", "es", "fr", "it", "ja", "ko", "nl", "pl", "pt", "ru", "uk", "zh-hans"};

struct Font {
    const char *family;
    int pointSize;
    const char *who;
};
struct Run {
    const char *family;
    int pointSize;
    int screenWidth;
    int screenHeight;
    const char *who;
};
// The screen sizes are what the window sees: a 1080p monitor, and a 1366x768 laptop at 125%.
const Run RUNS[] = {
    {"Adwaita Sans", 11, 1920, 1080, "GNOME"},
    {"Noto Sans", 10, 1920, 1080, "KDE Plasma"},
    {"DejaVu Sans", 10, 1920, 1080, "Xfce"},
    {"Adwaita Sans", 14, 1920, 1080, "GNOME Large Text"},
    {"Adwaita Sans", 11, 1093, 614, "GNOME, 1366x768 laptop at 125%"},
};

int g_cuts = 0;
QString g_where; // language and font of this run, for the report

QString plain(const QString &text)
{
    QTextDocument doc;
    doc.setHtml(text);
    return doc.toPlainText().replace('\n', " / ").left(80);
}

QString describe(QWidget *w, QWidget *tab)
{
    const QPoint p = w->mapTo(tab, QPoint(0, 0));
    return QString("%1 %2 at %3,%4 %5x%6").arg(w->metaObject()->className(), w->objectName())
        .arg(p.x()).arg(p.y()).arg(w->width()).arg(w->height());
}

void cut(const QString &tab, const QString &what, const QString &text, const QString &detail)
{
    ++g_cuts;
    std::printf("  FAIL %s | %s | %s | \"%s\" | %s\n", qPrintable(g_where), qPrintable(tab), qPrintable(what),
                qPrintable(plain(text)), qPrintable(detail));
}

QString withoutMnemonic(QString text)
{
    text.replace("&&", QChar(0x1));
    text.remove('&');
    text.replace(QChar(0x1), "&");
    return text;
}

// Is the rectangle (in w's coordinates) inside every widget around w, up to the tab? Inside a scroll
// area it may lie below the visible part, as long as the scroll bar reaches the end of the page.
bool insideParents(QWidget *w, const QRect &rect, QWidget *tab, QString *outside)
{
    for (QWidget *p = w->parentWidget(); p; p = p->parentWidget()) {
        auto *scroll = qobject_cast<QScrollArea *>(p->parentWidget());
        if (scroll && p == scroll->viewport()) {
            const QRect inPage(w->mapTo(scroll->widget(), rect.topLeft()), rect.size());
            const bool reachable = scroll->verticalScrollBar()->maximum() >= scroll->widget()->height() - p->height();
            if (!scroll->widget()->rect().adjusted(-1, -1, 1, 1).contains(inPage) || !reachable) {
                *outside = scroll->objectName() + " page";
                return false;
            }
            // Above the scroll area, what counts is the scroll area itself.
            return scroll == tab || insideParents(scroll, scroll->rect(), tab, outside);
        }
        const QRect inParent(w->mapTo(p, rect.topLeft()), rect.size());
        if (!p->rect().adjusted(-1, -1, 1, 1).contains(inParent)) {
            *outside = p->objectName();
            return false;
        }
        if (p == tab) {
            break;
        }
    }
    return true;
}

void checkTab(const QString &tabName, QWidget *tab)
{
    for (QScrollArea *scroll : tab->findChildren<QScrollArea *>()) {
        if (scroll->isVisibleTo(tab) && scroll->horizontalScrollBar()->isVisible()) {
            cut(tabName, describe(scroll, tab), "", "scrolls sideways");
        }
    }
    for (QWidget *w : tab->findChildren<QWidget *>()) {
        if (!w->isVisibleTo(tab) || w->width() <= 0) {
            continue;
        }
        QString text;
        QSize need;               // what the text needs
        QRect room = w->rect();   // where the widget draws it
        if (auto *label = qobject_cast<QLabel *>(w)) {
            if (label->text().isEmpty() || !label->pixmap().isNull()) {
                continue;
            }
            text = label->text();
            room = label->contentsRect();
            const int frame = label->height() - room.height();
            if (label->wordWrap()) {
                need = QSize(room.width(), label->heightForWidth(label->width()) - frame);
            } else {
                const QSize hint = label->sizeHint();
                need = QSize(hint.width() - (label->width() - room.width()), hint.height() - frame);
            }
        } else if (auto *button = qobject_cast<QAbstractButton *>(w)) {
            if (button->text().isEmpty()) {
                continue;
            }
            text = button->text();
            QStyleOptionButton opt;
            opt.initFrom(button);
            opt.text = text;
            const bool check = qobject_cast<QCheckBox *>(button) || qobject_cast<QRadioButton *>(button);
            room = button->style()->subElementRect(check ? QStyle::SE_CheckBoxContents : QStyle::SE_PushButtonContents, &opt, button);
            if (!check) {
                room.adjust(2, 0, -2, 0);
            }
            const int icon = button->icon().isNull() ? 0 : button->iconSize().width() + 4;
            need = QSize(button->fontMetrics().horizontalAdvance(withoutMnemonic(text)) + icon, button->fontMetrics().height());
        } else if (auto *combo = qobject_cast<QComboBox *>(w)) {
            QStyleOptionComboBox opt;
            opt.initFrom(combo);
            opt.editable = combo->isEditable();
            opt.frame = combo->hasFrame();
            const QRect field = combo->style()->subControlRect(QStyle::CC_ComboBox, &opt, QStyle::SC_ComboBoxEditField, combo);
            for (int i = 0; i < combo->count(); ++i) {
                const int n = combo->fontMetrics().horizontalAdvance(combo->itemText(i))
                    + (combo->itemIcon(i).isNull() ? 0 : combo->iconSize().width() + 4);
                if (n > field.width() + 1) {
                    cut(tabName, describe(w, tab), combo->itemText(i), QString("entry needs %1 px, has %2").arg(n).arg(field.width()));
                }
            }
            QString outside;
            if (!insideParents(w, w->rect(), tab, &outside)) {
                cut(tabName, describe(w, tab), combo->currentText(), "dropdown runs outside " + outside);
            }
            continue;
        } else if (auto *group = qobject_cast<QGroupBox *>(w)) {
            if (!group->title().isEmpty() && group->fontMetrics().horizontalAdvance(group->title()) + 16 > group->width()) {
                cut(tabName, describe(w, tab), group->title(), "group title too wide");
            }
            continue;
        } else {
            continue;
        }

        // Cut inside its own box: wider than its room (1 px of side bearing allowed), or more than 3 px
        // taller (a line's leading above and below the glyphs is not the glyphs).
        if (need.width() > room.width() + 1 || need.height() > room.height() + 3) {
            cut(tabName, describe(w, tab), text, QString("needs %1x%2, has %3x%4").arg(need.width()).arg(need.height())
                .arg(room.width()).arg(room.height()));
            continue;
        }

        // Cut by a widget around it: the part the text is drawn in must lie inside each of them.
        QRect drawn(room.topLeft(), QSize(qMin(room.width(), need.width()), qMin(room.height(), need.height())));
        if (auto *label = qobject_cast<QLabel *>(w)) {
            if (label->alignment() & Qt::AlignRight) {
                drawn.moveRight(room.right());
            } else if (label->alignment() & Qt::AlignHCenter) {
                drawn.moveCenter(QPoint(room.center().x(), drawn.center().y()));
            }
            if (label->alignment() & Qt::AlignVCenter) {
                drawn.moveCenter(QPoint(drawn.center().x(), room.center().y()));
            } else if (label->alignment() & Qt::AlignBottom) {
                drawn.moveBottom(room.bottom());
            }
        }
        QString outside;
        if (!insideParents(w, drawn, tab, &outside)) {
            cut(tabName, describe(w, tab), text, "text runs outside " + outside);
        }
    }
}

// One language at one font: open the window and check every tab. Returns the number of cuts.
int runOne(int argc, char **argv)
{
    const QString language = argv[1];
    QApplication app(argc, argv);
    LauncherOptions::processApp(app);
    app.setStyle(QStyleFactory::create("Fusion")); // as the launcher does
    QFont font(QString::fromUtf8(argv[2]));
    font.setPointSize(atoi(argv[3]));
    app.setFont(font);
    g_where = QString("%1, %2 %3 pt, %4x%5").arg(language, QString::fromUtf8(argv[2]), QString::fromUtf8(argv[3]),
                                                QString::fromUtf8(argv[4]), QString::fromUtf8(argv[5]));

    const QSize screen = QGuiApplication::primaryScreen()->size();
    if (screen != QSize(atoi(argv[4]), atoi(argv[5]))) {
        std::printf("  FAIL %s | the screen is %dx%d, not %sx%s\n", qPrintable(g_where), screen.width(), screen.height(), argv[4], argv[5]);
        return 1;
    }

    Settings::load();
    KfxVersion::loadCurrentVersion();
    if (language != "en") {
        auto *translator = new Translator;
        if (!translator->loadLanguage(language)) {
            std::printf("  FAIL %s | no translation\n", qPrintable(g_where));
            return 1;
        }
        app.installTranslator(translator);
    }

    SettingsDialog dialog;
    dialog.show();
    app.processEvents();
    app.processEvents();

    auto *tabs = dialog.findChild<QTabWidget *>("tabWidget");
    const int barNeeds = tabs->tabBar()->sizeHint().width();
    if (barNeeds > tabs->width()) {
        cut("(tabs)", "tab bar", "", QString("the tabs need %1 px, have %2").arg(barNeeds).arg(tabs->width()));
    }
    for (int i = 0; i < tabs->count(); ++i) {
        tabs->setCurrentIndex(i);
        app.processEvents();
        app.processEvents();
        checkTab(tabs->tabText(i), tabs->widget(i));
    }
    for (QAbstractButton *button : dialog.findChildren<QAbstractButton *>()) {
        if (button->isVisible() && !tabs->isAncestorOf(button) && !button->text().isEmpty()) {
            const QSize hint = button->sizeHint();
            if (hint.width() > button->width() || hint.height() > button->height()) {
                cut("(buttons)", describe(button, &dialog), button->text(),
                    QString("needs %1x%2").arg(hint.width()).arg(hint.height()));
            }
        }
    }
    return g_cuts;
}

} // namespace

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsSet("SETTINGSFIT_RUN")) {
        return runOne(argc, argv) == 0 ? 0 : 1;
    }

    // Copy this program into a folder of its own, with the files Settings and KfxVersion read.
    QCoreApplication core(argc, argv);
    QTemporaryDir dir;
    const QString copy = dir.filePath("test_settingsfit");
    if (!dir.isValid() || !QFile::copy(QCoreApplication::applicationFilePath(), copy)) {
        std::printf("FAIL cannot copy the test into a temporary folder\n");
        return 1;
    }
    QFile::setPermissions(copy, QFile::permissions(copy) | QFile::ExeOwner);
    // A current engine, so every setting gated on the engine version is shown.
    QFile version(dir.filePath("version.txt"));
    version.open(QIODevice::WriteOnly);
    version.write("1.4.0.6206 alpha\n");
    version.close();
    for (const char *cfg : {"keeperfx.cfg", "keeperfx-launcher-qt.cfg"}) {
        QFile f(dir.filePath(cfg));
        f.open(QIODevice::WriteOnly);
    }

    int failedRuns = 0;
    for (const Run &r : RUNS) {
        // Qt's offscreen platform takes its screen from a file
        const QString screenFile = dir.filePath(QString("screen-%1x%2.json").arg(r.screenWidth).arg(r.screenHeight));
        QFile json(screenFile);
        json.open(QIODevice::WriteOnly);
        json.write(QString("{\"screens\": [{\"name\": \"screen\", \"x\": 0, \"y\": 0, \"width\": %1, \"height\": %2, "
                           "\"logicalDpi\": 96, \"logicalBaseDpi\": 96, \"dpr\": 1}]}")
                       .arg(r.screenWidth).arg(r.screenHeight).toUtf8());
        json.close();
        for (const char *language : LANGUAGES) {
            QProcess run;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("SETTINGSFIT_RUN", "1");
            env.insert("QT_QPA_PLATFORM", "offscreen:configfile=" + screenFile);
            env.remove("QT_SCALE_FACTOR");
            run.setProcessEnvironment(env);
            run.setProcessChannelMode(QProcess::ForwardedOutputChannel);
            run.start(copy, {language, r.family, QString::number(r.pointSize), QString::number(r.screenWidth),
                             QString::number(r.screenHeight)});
            run.waitForFinished(120000);
            const bool ok = run.exitStatus() == QProcess::NormalExit && run.exitCode() == 0;
            std::printf("  %s %s, %s %d pt, %dx%d (%s)\n", ok ? "ok  " : "FAIL", language, r.family, r.pointSize,
                        r.screenWidth, r.screenHeight, r.who);
            std::fflush(stdout);
            if (!ok) {
                ++failedRuns;
            }
        }
    }
    if (failedRuns) {
        std::printf("%d of %d runs cut something off\n", failedRuns, int(std::size(RUNS) * std::size(LANGUAGES)));
        return 1;
    }
    std::printf("all settings-fit runs passed\n");
    return 0;
}
