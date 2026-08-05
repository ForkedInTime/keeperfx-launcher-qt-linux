#pragma once
/******************************************************************************/
// Repairs the download URLs keeperfx.net's API hands out.
//
// Header-only and free of any launcher dependency beyond QtCore, so
// tests/test_workshopurl.cpp can exercise it without linking the launcher.
/******************************************************************************/
#include <QString>
#include <QUrl>

// The API reports a file as
//   filename : "pandaemonium v1.3.zip"
//   url      : "https://keeperfx.net/workshop/download/786/2174/pandaemonium+v1.3.zip"
//
// The space has been form-encoded to "+", which is only meaningful in a query
// string. In a URL path "+" is a literal plus, so the server does not recognise
// the name and answers 404 -- the API hands out a link its own download endpoint
// rejects. Verified against the live site: the "+" form returns 404 and the
// "%20" form returns 200. Every workshop file whose name contains a space is
// affected, which is a large share of the catalogue.
//
// The last path segment is rebuilt from the filename the API also reports,
// rather than rewriting "+" to "%20" in the raw URL: a literal "+" is legal in a
// path, and a file genuinely named "c++ tutorial.zip" must survive. Rebuilding
// from the filename is unambiguous; patching the encoded form is guesswork.
//
// Anything unexpected (no URL, no filename, no path separator) falls through to
// the raw URL unchanged, so this can only ever help.
inline QString repair_workshop_file_url(const QString & raw_url, const QString & filename)
{
	if (raw_url.isEmpty() || filename.isEmpty()) {
		return raw_url;
	}
	const int last_slash = raw_url.lastIndexOf('/');
	if (last_slash < 0) {
		return raw_url;
	}
	// toPercentEncoding leaves unreserved characters alone and encodes the rest,
	// so a space becomes %20 and a literal '+' becomes %2B -- both of which the
	// server decodes back to the filename it stored.
	const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(filename));
	return raw_url.left(last_slash + 1) + encoded;
}
