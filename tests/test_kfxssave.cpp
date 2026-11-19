// Tests the REAL KFXS title reader by compiling src/kfxssave.cpp into the test.
// The files are built here, chunk by chunk, the way the engine writes them
// (src/kfx/save/FORMAT.txt): a header, then META (the title record), SCHM (its
// layout, zlib-packed as the engine packs it) and a stand-in for the game state.
//
// KFXS_SAMPLES=<folder> also prints the title of every fx1g*.sav in that folder
// (a look at real saves; nothing is asserted about them and nothing is written).
#include "kfxssave.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <cstdio>
#include <zlib.h>

static int g_failures = 0;

static void expect(bool ok, const char *what)
{
	std::printf(ok ? "  ok   %s\n" : "  FAIL %s\n", what);
	if (!ok) ++g_failures;
}

static void u8(QByteArray &out, quint8 v) { out.append(char(v)); }
static void u16(QByteArray &out, quint16 v) { u8(out, v & 0xFF); u8(out, v >> 8); }
static void u32(QByteArray &out, quint32 v) { u16(out, v & 0xFFFF); u16(out, v >> 16); }
static void str(QByteArray &out, const QByteArray &s) { u16(out, quint16(s.size())); out.append(s); }
static void fixedStr(QByteArray &out, const QByteArray &s, int capacity)
{
	QByteArray bytes = s.left(capacity - 1);
	bytes.resize(capacity, '\0');
	out.append(bytes);
}

struct Field {
	QByteArray name;
	quint8 type;
	QList<quint32> dims;
};

static void field(QByteArray &out, const Field &f)
{
	str(out, f.name);
	u8(out, f.type);
	u8(out, 0x01); // SAVE
	u8(out, quint8(f.dims.size()));
	for (quint32 d : f.dims) u32(out, d);
	str(out, QByteArray());
	if (f.type == 14) { // a union with one raw member
		u16(out, 1);
		str(out, "raw");
		str(out, QByteArray());
		u32(out, 8);
	}
}

static QByteArray chunk(const char *id, const QByteArray &raw, bool pack)
{
	QByteArray body = raw;
	quint32 flags = 0;
	if (pack) {
		uLongf length = compressBound(uLong(raw.size()));
		body.resize(qsizetype(length));
		compress2(reinterpret_cast<Bytef *>(body.data()), &length, reinterpret_cast<const Bytef *>(raw.constData()), uLong(raw.size()), 1);
		body.resize(qsizetype(length));
		flags = 0x01;
	}
	QByteArray out(id, 4);
	u32(out, flags);
	u32(out, quint32(body.size()));
	u32(out, quint32(raw.size()));
	u32(out, 0); // crc: not checked by the title reader
	out.append(body);
	return out;
}

struct Options {
	quint16 major = 1;
	bool withCampaignField = true;
	bool withNameField = true;
};

// CatalogueEntry as the engine's table has it (schema_misc.c), and a record of it.
static QByteArray buildSave(const QByteArray &name, const QByteArray &campaign, const Options &options = Options())
{
	QList<Field> fields;
	fields << Field{"flags", 3, {}};
	if (options.withNameField) fields << Field{"textname", 12, {30}};
	fields << Field{"level_num", 6, {}};
	if (options.withCampaignField) fields << Field{"campaign_name", 12, {160}};
	fields << Field{"campaign_fname", 12, {48}} << Field{"player_name", 12, {33}}
	       << Field{"game_ver_major", 3, {}} << Field{"game_ver_minor", 3, {}}
	       << Field{"game_ver_release", 3, {}} << Field{"game_ver_build", 3, {}};

	QByteArray schema;
	u32(schema, 2);
	// Another struct first, with a union and a 3-dimensional array, which the reader must step over.
	str(schema, "Bystander");
	u16(schema, 2);
	field(schema, Field{"blob", 14, {}});
	field(schema, Field{"grid", 5, {4, 5, 6}});
	str(schema, "CatalogueEntry");
	u16(schema, quint16(fields.size()));
	for (const Field &f : fields) field(schema, f);

	QByteArray record;
	u16(record, 0);
	if (options.withNameField) fixedStr(record, name, 30);
	u32(record, 7);
	if (options.withCampaignField) fixedStr(record, campaign, 160);
	fixedStr(record, "keeporig.cfg", 48);
	fixedStr(record, "Keeper", 33);
	u16(record, 1); u16(record, 4); u16(record, 0); u16(record, 6230);

	QByteArray file("KFXS\x00\xFF\x1A\x0A", 8);
	u16(file, options.major);
	u16(file, 12);
	file.append("SAVE", 4);
	u32(file, 0);
	file.append(chunk("META", record, false));
	file.append(chunk("SCHM", schema, true));
	file.append(chunk("GAME", QByteArray(4096, '\x5A'), false));
	return file;
}

static std::optional<KfxsSave::Title> titleOf(const QByteArray &bytes)
{
	QBuffer buffer;
	buffer.setData(bytes);
	buffer.open(QIODevice::ReadOnly);
	return KfxsSave::readTitle(buffer);
}

static void testReadsTheTitle()
{
	std::printf("KfxsSave::readTitle\n");
	const QByteArray save = buildSave("Hatchery done", "Keeper Original");
	const auto title = titleOf(save);
	expect(title.has_value(), "a well-formed save has a title");
	expect(title && title->saveName == "Hatchery done", "the save name is the one the player gave");
	expect(title && title->campaignName == "Keeper Original", "the campaign name is read from its own offset");
	expect(KfxsSave::isKfxs(save.left(8)), "the KFXS magic is recognised");

	const QByteArray older("\x00\x00\x00\x00INFO", 8);
	expect(!KfxsSave::isKfxs(older), "an older save's header is not taken for KFXS");

	const auto noCampaign = titleOf(buildSave("Free play", "", Options{1, false, true}));
	expect(noCampaign && noCampaign->saveName == "Free play" && noCampaign->campaignName.isEmpty(),
		"a save whose title has no campaign field still has its name");

	expect(!titleOf(buildSave("Nameless", "Keeper Original", Options{1, true, false})).has_value(),
		"no title when the record has no name field");
	expect(!titleOf(buildSave("x", "y", Options{2, true, true})).has_value(),
		"a format major the reader does not know gives no title");

	const QByteArray longName(200, 'n');
	const auto clipped = titleOf(buildSave(longName, "Keeper Original"));
	expect(clipped && clipped->saveName.size() == 29, "a name fills at most its field, less the terminator");
}

static void testDamagedFiles()
{
	std::printf("KfxsSave::readTitle on damaged files\n");
	const QByteArray save = buildSave("Hatchery done", "Keeper Original");

	// Cut off anywhere: never a read past the end, and no title until META and SCHM are whole.
	const int schmEnd = save.indexOf("GAME");
	bool cutOk = true;
	for (int length = 0; length < save.size(); length++) {
		const auto title = titleOf(save.left(length));
		if (length < schmEnd && title.has_value()) cutOk = false;
	}
	expect(cutOk, "a file cut off before the end of SCHM gives no title, at every length");

	// Any single byte of the header and the two chunks set to 0x00 or 0xFF: no crash, no hang.
	for (const char value : {char(0x00), char(0xFF)}) {
		for (int at = 0; at < schmEnd; at++) {
			QByteArray damaged = save;
			damaged[at] = value;
			titleOf(damaged);
		}
	}
	expect(true, "every single damaged byte of header, META and SCHM was survived");

	// A chunk that claims more bytes than the file has.
	QByteArray huge = save;
	const int meta = huge.indexOf("META");
	huge[meta + 8] = char(0xFF); huge[meta + 9] = char(0xFF); huge[meta + 10] = char(0xFF); huge[meta + 11] = char(0x7F);
	expect(!titleOf(huge).has_value(), "a chunk longer than the file gives no title");

	// A packed chunk whose unpacked length is a lie.
	QByteArray lying = save;
	const int schm = lying.indexOf("SCHM");
	lying[schm + 12] = char(quint8(lying[schm + 12]) + 1);
	expect(!titleOf(lying).has_value(), "a packed chunk that unpacks to another length gives no title");
}

static void printSamples(const QString &folder)
{
	std::printf("KFXS_SAMPLES %s\n", qPrintable(folder));
	const QStringList files = QDir(folder).entryList({"fx1g*.sav"}, QDir::Files, QDir::Name);
	for (const QString &name : files) {
		QFile file(QDir(folder).filePath(name));
		if (!file.open(QIODevice::ReadOnly)) continue;
		const auto title = KfxsSave::readTitle(file);
		std::printf("  %s: %s\n", qPrintable(name),
			title ? qPrintable(title->saveName + " / " + title->campaignName) : "(no title)");
	}
}

int main()
{
	testReadsTheTitle();
	testDamagedFiles();
	if (qEnvironmentVariableIsSet("KFXS_SAMPLES")) {
		printSamples(qEnvironmentVariable("KFXS_SAMPLES"));
	}
	std::printf(g_failures ? "%d FAILED\n" : "all passed\n", g_failures);
	return g_failures ? 1 : 0;
}
