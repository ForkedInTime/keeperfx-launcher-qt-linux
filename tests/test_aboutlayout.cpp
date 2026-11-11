// Standalone test for the Settings window's About tab: every line on it is shown whole at the
// window's fixed size, at the desktop's usual font sizes, and this edition's own credits line
// names its author and no one else. The window is built from the real
// ui/settingsdialog.ui (uic) and filled the way SettingsDialog fills it, with the real
// contributor and translator lists.
//
// Build and run:
//   tests/run.sh
#include "ui_settingsdialog.h"
#include "../src/aboutcredits.h"

#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QFont>
#include <QLabel>
#include <QPixmap>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <cstdio>

static int g_failures = 0;

static void expect(bool ok, const QString &what)
{
	std::printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
	if (!ok) {
		++g_failures;
	}
}

static QStringList readLines(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
		std::printf("  FAIL cannot read %s\n", qPrintable(path));
		++g_failures;
		return {};
	}
	return QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
}

// The About text in English, as SettingsDialog builds it.
static QString aboutText()
{
	QString s;
	s += "KeeperFX v1.4.0.6206 alpha<br/>";
	s += "<a href=\"https://keeperfx.net\">https://keeperfx.net</a><br/><br/>";
	s += "KeeperFX Launcher v1.11.0<br/>";
	s += "Launcher Libraries:<br/>";
	s += "<a href=\"https://www.qt.io/product/qt6\">Qt6 (6.12.0)</a>, <a href=\"https://github.com/lief-project/LIEF\">libLIEF</a>, "
	     "<a href=\"https://github.com/rikyoz/bit7z\">bit7z</a>, <a href=\"https://www.7-zip.org/\">7z</a>, "
	     "<a href=\"https://www.zlib.net/\">zlib</a><br/><br/>";
	s += "Community Discord of the official KeeperFX (Windows): <a href=\"https://discord.gg/WxgE8WZBku\">Keeper Klan Discord</a><br/>";
	s += "Problems with KeeperFX: Tux Edition: <a href=\"https://github.com/ForkedInTime/keeperfx-tux-edition/issues\">report them on our GitHub page</a>";
	return s;
}

// Is the whole of this label's text there to read? The label has the room its text needs, and
// every widget around it, up to the About tab, holds the whole label (a label partly outside its
// parent is cut at the edge). Inside a scroll area the label may lie below the visible part, as
// long as the scroll bar reaches the end of the page.
static bool shownWhole(QLabel *label, QWidget *tab)
{
	if (!label->isVisible()) {
		return false;
	}
	bool fits;
	if (label->wordWrap()) {
		fits = label->heightForWidth(label->width()) <= label->height();
	} else {
		fits = label->sizeHint().width() <= label->width() && label->sizeHint().height() <= label->height();
	}
	for (QWidget *w = label->parentWidget(); w && fits; w = w->parentWidget()) {
		auto *scroll = qobject_cast<QScrollArea *>(w->parentWidget());
		if (scroll && w == scroll->viewport()) {
			const int hidden = scroll->widget()->height() - scroll->viewport()->height();
			return hidden <= 0 || scroll->verticalScrollBar()->maximum() >= hidden;
		}
		fits = w->rect().contains(QRect(label->mapTo(w, QPoint(0, 0)), label->size()));
		if (w == tab) {
			break;
		}
	}
	return fits;
}

static void checkAt(int pointSize)
{
	QFont font = QApplication::font();
	font.setPointSize(pointSize);
	QApplication::setFont(font);

	QDialog dialog;
	Ui::SettingsDialog ui;
	ui.setupUi(&dialog);
	dialog.setFixedSize(dialog.size());   // as SettingsDialog does
	ui.labelAboutImage->setPixmap(QPixmap(480, 341));   // launchermn.jpg's size (no resources here)

	ui.labelAbout->setTextFormat(Qt::RichText);
	ui.labelAbout->setText(aboutText());

	QStringList contributors = readLines(SOURCE_ROOT "/res/contributors.txt");
	for (QString &c : contributors) {
		c = QString("<a href=\"https://github.com/%1\">%1</a>").arg(c.trimmed());
	}
	ui.labelContributors->setTextFormat(Qt::RichText);
	ui.labelContributors->setText("KeeperFX Contributors: " + contributors.join(", "));

	QStringList translators = readLines(SOURCE_ROOT "/res/launcher-translators.txt");
	for (QString &t : translators) {
		const QStringList parts = t.trimmed().split(',');
		t = parts.size() == 2 ? QString("<a href=\"%1\">%2</a>").arg(parts[1], parts[0]) : parts[0];
	}
	ui.labelLauncherTranslators->setTextFormat(Qt::RichText);
	ui.labelLauncherTranslators->setText("KeeperFX Launcher Translators: " + translators.join(", "));

	ui.labelTuxContributors->setTextFormat(Qt::RichText);
	ui.labelTuxContributors->setText(AboutCredits::tuxEditionContributors());

	ui.tabWidget->setCurrentWidget(ui.tab8_about);
	dialog.show();
	QApplication::processEvents();
	QApplication::processEvents();

	const QString at = QString(" (%1 pt)").arg(pointSize);
	expect(shownWhole(ui.labelAbout, ui.tab8_about), "the versions, links and support lines are shown whole" + at);
	expect(shownWhole(ui.labelContributors, ui.tab8_about), "the KeeperFX contributors are shown whole" + at);
	expect(shownWhole(ui.labelLauncherTranslators, ui.tab8_about), "the launcher translators are shown whole" + at);
	expect(shownWhole(ui.labelTuxContributors, ui.tab8_about), "the Tux Edition contributors are shown whole" + at);
	expect(ui.labelTuxContributors->y() > ui.labelLauncherTranslators->y(), "the Tux Edition line comes after the translators" + at);
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	// The desktop fonts players use: 9 pt is Qt's own default, 10 and 11 pt are GNOME's and KDE's,
	// and large-text settings take them to 14 pt and beyond.
	for (int pt : {9, 10, 11, 12, 13, 14, 16, 18}) {
		checkAt(pt);
	}

	// One name, one link: the edition's author.
	const QString credits = AboutCredits::tuxEditionContributors();
	expect(credits == "KeeperFX: Tux Edition Contributors: <a href=\"https://github.com/ForkedInTime\">ForkedInTime</a>",
	       "the Tux Edition line credits ForkedInTime alone: " + credits);
	expect(credits.count("<a ") == 1, "the Tux Edition line holds a single link");
	if (g_failures) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
