#include "kfxssave.h"

#include <QMap>
#include <QPair>

#include <zlib.h>

// The layout below is the engine's src/kfx/save/FORMAT.txt (format major 1).
// The team's launcher reads it the same way (their #69); this edition checks
// every read against the bytes there are.

namespace {

const QByteArray MAGIC("KFXS\x00\xFF\x1A\x0A", 8);
constexpr int HEADER_SIZE = 20;       // magic, major, minor, kind, crc
constexpr int CHUNK_HEADER_SIZE = 20; // id, flags, stored_len, raw_len, crc
constexpr quint32 CHUNK_ZLIB = 0x01;
constexpr quint32 CHUNK_BODY_MAX = 4 * 1024 * 1024; // META and SCHM are a few kilobytes
constexpr int CHUNKS_MAX = 64;

constexpr quint8 TYPE_STR = 12;
constexpr quint8 TYPE_UNION = 14;
// Bytes per element of stored types 0..12 (5.1); 13 (struct) and 14 (union) have none of their own.
constexpr int TYPE_SIZES[] = {0, 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 1, 1};

quint32 le32(const QByteArray &bytes, int at)
{
    return quint32(quint8(bytes[at])) | quint32(quint8(bytes[at + 1])) << 8
           | quint32(quint8(bytes[at + 2])) << 16 | quint32(quint8(bytes[at + 3])) << 24;
}

/** Reads little-endian values from a buffer; any read past its end fails and stays failed. */
class Cursor
{
public:
    explicit Cursor(const QByteArray &bytes) : data(bytes) {}

    bool ok() const { return good; }
    bool atEnd() const { return pos >= data.size(); }

    quint8 u8()
    {
        if (!take(1)) return 0;
        return quint8(data[pos - 1]);
    }
    quint16 u16()
    {
        if (!take(2)) return 0;
        return quint16(quint8(data[pos - 2]) | quint8(data[pos - 1]) << 8);
    }
    quint32 u32()
    {
        if (!take(4)) return 0;
        return le32(data, int(pos - 4));
    }
    QByteArray str()
    {
        const quint16 length = u16();
        if (!take(length)) return QByteArray();
        return data.mid(pos - length, length);
    }
    qsizetype left() const { return good ? data.size() - pos : 0; }

private:
    bool take(qsizetype count)
    {
        if (!good || count < 0 || count > data.size() - pos) {
            good = false;
            return false;
        }
        pos += count;
        return true;
    }

    const QByteArray &data;
    qsizetype pos = 0;
    bool good = true;
};

/** The body of a chunk, unpacked; empty when it is damaged. */
QByteArray chunkBody(const QByteArray &stored, quint32 flags, quint32 rawLength)
{
    if ((flags & CHUNK_ZLIB) == 0) {
        return (quint32(stored.size()) == rawLength) ? stored : QByteArray();
    }
    QByteArray raw(qsizetype(rawLength), '\0');
    uLongf length = rawLength;
    if (uncompress(reinterpret_cast<Bytef *>(raw.data()), &length,
                   reinterpret_cast<const Bytef *>(stored.constData()), uLong(stored.size())) != Z_OK
        || length != rawLength) {
        return QByteArray();
    }
    return raw;
}

/** Where each string field of CatalogueEntry sits in a META record: name -> (offset, capacity). */
bool titleStringFields(const QByteArray &schema, QMap<QByteArray, QPair<quint64, quint64>> &strings)
{
    Cursor in(schema);
    const quint32 structCount = in.u32();
    // A struct takes at least four bytes (an empty name and a field count).
    if (!in.ok() || structCount > in.left() / 4) {
        return false;
    }
    for (quint32 s = 0; s < structCount; s++) {
        const QByteArray structName = in.str();
        const quint16 fieldCount = in.u16();
        const bool isTitle = (structName == "CatalogueEntry");
        quint64 offset = 0;
        bool layoutKnown = true;
        for (quint16 f = 0; f < fieldCount && in.ok(); f++) {
            const QByteArray fieldName = in.str();
            const quint8 type = in.u8();
            in.u8(); // flags
            const quint8 dimCount = in.u8();
            if (type > TYPE_UNION || dimCount > 3) {
                return false;
            }
            quint64 count = 1;
            for (quint8 d = 0; d < dimCount; d++) {
                count *= in.u32();
                if (count > quint64(UINT32_MAX)) {
                    count = quint64(UINT32_MAX) + 1; // far past any record: offsets after it are never read
                }
            }
            in.str(); // sub_type
            if (type == TYPE_UNION) {
                const quint16 members = in.u16();
                if (members > 254) {
                    return false;
                }
                for (quint16 m = 0; m < members && in.ok(); m++) {
                    in.str(); // name
                    in.str(); // sub_type
                    in.u32(); // raw_size
                }
            }
            if (!isTitle) {
                continue;
            }
            if (type == 0 || type > TYPE_STR) {
                layoutKnown = false; // a struct or union: where the fields after it start is not known here
            }
            if (!layoutKnown) {
                continue;
            }
            if (type == TYPE_STR) {
                strings.insert(fieldName, qMakePair(offset, count));
            }
            offset += quint64(TYPE_SIZES[type]) * count;
        }
        if (!in.ok()) {
            return false;
        }
    }
    return true;
}

QString recordString(const QByteArray &record, const QMap<QByteArray, QPair<quint64, quint64>> &strings,
                     const QByteArray &field)
{
    const auto found = strings.constFind(field);
    if (found == strings.constEnd()) {
        return QString();
    }
    const quint64 offset = found->first;
    const quint64 capacity = found->second;
    if (offset > quint64(record.size()) || capacity > quint64(record.size()) - offset) {
        return QString();
    }
    QByteArray bytes = record.mid(qsizetype(offset), qsizetype(capacity));
    const qsizetype end = bytes.indexOf('\0');
    if (end >= 0) {
        bytes.truncate(end);
    }
    return QString::fromLatin1(bytes);
}

} // namespace

bool KfxsSave::isKfxs(const QByteArray &start)
{
    return start.startsWith(MAGIC);
}

std::optional<KfxsSave::Title> KfxsSave::readTitle(QIODevice &device)
{
    if (device.isSequential() || !device.isOpen()) {
        return std::nullopt;
    }
    const qint64 size = device.size();
    if (!device.seek(0)) {
        return std::nullopt;
    }
    const QByteArray header = device.read(HEADER_SIZE);
    if (header.size() != HEADER_SIZE || !header.startsWith(MAGIC)
        || (quint8(header[8]) | quint8(header[9]) << 8) != 1) { // only format major 1 is known
        return std::nullopt;
    }

    QByteArray meta;
    QByteArray schema;
    for (int chunk = 0; chunk < CHUNKS_MAX && (meta.isEmpty() || schema.isEmpty()); chunk++) {
        const QByteArray chunkHeader = device.read(CHUNK_HEADER_SIZE);
        if (chunkHeader.size() != CHUNK_HEADER_SIZE) {
            break; // the end of the file
        }
        const QByteArray id = chunkHeader.left(4);
        const quint32 flags = le32(chunkHeader, 4);
        const quint32 storedLength = le32(chunkHeader, 8);
        const quint32 rawLength = le32(chunkHeader, 12);
        if (qint64(storedLength) > size - device.pos()) {
            return std::nullopt; // the chunk runs past the end of the file
        }
        if (id != "META" && id != "SCHM") {
            if (!device.seek(device.pos() + storedLength)) {
                return std::nullopt;
            }
            continue;
        }
        if (storedLength > CHUNK_BODY_MAX || rawLength > CHUNK_BODY_MAX) {
            return std::nullopt;
        }
        const QByteArray stored = device.read(storedLength);
        if (quint32(stored.size()) != storedLength) {
            return std::nullopt;
        }
        const QByteArray body = chunkBody(stored, flags, rawLength);
        if (body.isEmpty()) {
            return std::nullopt;
        }
        (id == "META" ? meta : schema) = body;
    }
    if (meta.isEmpty() || schema.isEmpty()) {
        return std::nullopt;
    }

    QMap<QByteArray, QPair<quint64, quint64>> strings;
    if (!titleStringFields(schema, strings)) {
        return std::nullopt;
    }
    Title title;
    title.saveName = recordString(meta, strings, "textname");
    title.campaignName = recordString(meta, strings, "campaign_name");
    if (title.saveName.isEmpty()) {
        return std::nullopt;
    }
    return title;
}
