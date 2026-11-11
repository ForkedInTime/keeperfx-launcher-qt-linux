// Standalone test for what "Copy to Clipboard" puts on the clipboard for a file: a URL list (Qt,
// KDE, GNOME Files), the GNOME copied-files format the GTK file managers paste from (Nemo, Caja,
// Thunar), and the plain path for a chat window or a terminal.
//
// Build and run:
//   tests/run.sh
#include "../src/clipboardfile.h"

#include <QCoreApplication>
#include <QScopedPointer>
#include <QUrl>
#include <cstdio>

static int g_failures = 0;

static void expect(bool ok, const char *what)
{
	std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) {
		++g_failures;
	}
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	const QString path = QString::fromUtf8("/home/player/.local/share/keeperfx-alpha/replays/campaign/a b \xc3\xb1.fxpkt");
	QScopedPointer<QMimeData> data(ClipboardFile::mimeData(path));

	expect(data->urls() == QList<QUrl>() << QUrl::fromLocalFile(path), "a URL list with the file");
	expect(data->hasFormat("x-special/gnome-copied-files"), "the GNOME copied-files format");
	expect(data->data("x-special/gnome-copied-files") == "copy\n" + QUrl::fromLocalFile(path).toEncoded(),
		"which says copy, then the file's encoded URL");
	expect(data->text() == path, "and the plain path as text");

	if (g_failures) {
		std::printf("%d clipboard test(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all clipboard tests passed\n");
	return 0;
}
