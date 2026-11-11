#pragma once

// What Copy to Clipboard puts on the clipboard for a file (the replays dialog).
//
// A URL list alone pastes as a file in Qt/KDE apps and GNOME Files, but the GTK file managers
// (Nemo, Caja, Thunar) paste files from "x-special/gnome-copied-files"; the plain path covers a
// chat window or a terminal. QtCore only, so tests/test_clipboardfile.cpp can check it.

#include <QByteArray>
#include <QList>
#include <QMimeData>
#include <QString>
#include <QUrl>

namespace ClipboardFile {

// The caller owns the result (QClipboard::setMimeData takes it over).
inline QMimeData *mimeData(const QString &absolutePath)
{
	const QUrl url = QUrl::fromLocalFile(absolutePath);
	QMimeData *data = new QMimeData();
	data->setUrls(QList<QUrl>() << url);
	data->setData("x-special/gnome-copied-files", QByteArray("copy\n") + url.toEncoded());
	data->setText(absolutePath);
	return data;
}

} // namespace ClipboardFile
