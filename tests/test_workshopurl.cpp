// Standalone tests for the workshop download-URL repair.
//
// Build and run:
//   tests/run.sh
#include "../src/workshopurl.h"

#include <QCoreApplication>
#include <cstdio>

static int g_failures = 0;

static void expect(const QString & got, const char * wanted, const char * what)
{
	if (got == QString(wanted)) {
		std::printf("  ok   %s\n", what);
	} else {
		std::printf("  FAIL %s:\n         got    '%s'\n         wanted '%s'\n",
			what, qUtf8Printable(got), wanted);
		++g_failures;
	}
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);

	// The real case, from item 786. The "+" form 404s against the live site and
	// the "%20" form returns 200; both were checked by hand before this was written.
	expect(repair_workshop_file_url(
			"https://keeperfx.net/workshop/download/786/2174/pandaemonium+v1.3.zip",
			"pandaemonium v1.3.zip"),
		"https://keeperfx.net/workshop/download/786/2174/pandaemonium%20v1.3.zip",
		"a space becomes %20, not +");

	// Several spaces, and the rest of the path must be left exactly as it was.
	expect(repair_workshop_file_url(
			"https://keeperfx.net/workshop/download/12/34/a+b+c.zip", "a b c.zip"),
		"https://keeperfx.net/workshop/download/12/34/a%20b%20c.zip",
		"every space is encoded and the path prefix is untouched");

	// A filename with no space is already fine and must come back byte-identical.
	expect(repair_workshop_file_url(
			"https://keeperfx.net/workshop/download/1/2/plain.zip", "plain.zip"),
		"https://keeperfx.net/workshop/download/1/2/plain.zip",
		"a plain filename is unchanged");

	// A literal '+' in the name is why this rebuilds from the filename instead of
	// rewriting '+' to '%20': blind rewriting would corrupt this into a space.
	expect(repair_workshop_file_url(
			"https://keeperfx.net/workshop/download/1/2/c%2B%2B+guide.zip", "c++ guide.zip"),
		"https://keeperfx.net/workshop/download/1/2/c%2B%2B%20guide.zip",
		"a literal + survives as %2B while the space becomes %20");

	// Other characters that need encoding in a path.
	expect(repair_workshop_file_url(
			"https://keeperfx.net/workshop/download/1/2/x.zip", "50% more & better.zip"),
		"https://keeperfx.net/workshop/download/1/2/50%25%20more%20%26%20better.zip",
		"percent and ampersand are encoded too");

	// Degenerate inputs fall through untouched rather than inventing a URL.
	expect(repair_workshop_file_url("https://x/y/z.zip", ""), "https://x/y/z.zip",
		"no filename -> raw URL unchanged");
	expect(repair_workshop_file_url("", "a b.zip"), "",
		"no URL -> empty stays empty");
	expect(repair_workshop_file_url("noslashes", "a b.zip"), "noslashes",
		"no path separator -> raw URL unchanged");

	if (g_failures > 0) {
		std::printf("\n%d workshop URL test(s) FAILED.\n", g_failures);
		return 1;
	}
	std::printf("\nAll workshop URL tests passed.\n");
	return 0;
}
