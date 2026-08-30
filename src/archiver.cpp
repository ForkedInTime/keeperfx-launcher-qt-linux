#include "archiver.h"

#ifdef WIN32
#include "helper.h" // For 64bit check on lib dll
#endif

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>

#include <cstring>

#include <bit7z/bitextractor.hpp>
#include <bit7z/bitabstractarchivehandler.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitfilecompressor.hpp>
#include <bit7z/bitfileextractor.hpp>

std::optional<bit7z::Bit7zLibrary> Archiver::lib;

// Initialize the library if it's not already loaded
void Archiver::loadBit7zLib()
{
    // Only load if it's not already initialized
    if (Archiver::lib) {
        return;
    }

#ifdef WIN32
    bit7z::tstring libPath;
    if (QFile(QCoreApplication::applicationDirPath() + "/7za.dll").exists()) {
        libPath = BIT7Z_STRING(QCoreApplication::applicationDirPath().toStdString()
                               + "/7za.dll");
    } else if (QFile(QCoreApplication::applicationDirPath() + "/7z.dll").exists()) {
        libPath = BIT7Z_STRING(QCoreApplication::applicationDirPath().toStdString() + "/7z.dll");
    } else {
        qWarning() << "Failed to find 7zip lib to load";
        return;
    }

    if (!Helper::is64BitDLL(libPath)) {
        qWarning() << "Not a 64 bit dll:" << libPath;
    }
#else
    bit7z::tstring libPath = BIT7Z_STRING(QCoreApplication::applicationDirPath().toStdString()
                                          + "/7z.so");
#endif

    qDebug() << "7z lib path:" << libPath;
    lib.emplace(libPath); // Initialize the static library

    // Make sure lib is loaded now
    if (!Archiver::lib) {
        throw std::runtime_error("Failed to load bit7z library");
    }
}

// Identify the archive format from its leading bytes.
//
// The extension alone is not enough. Workshop items are packed by whoever
// uploaded them -- .zip, .7z and .rar all occur (item 414 "Infernal Rift" is a
// RAR5) -- and an extension can simply be wrong, which is common in
// user-contributed content. Every format below announces itself in its header,
// so read that and only fall back to the extension.
//
// bit7z's own BitFormat::Auto is not an option here: it exists only when the
// library is compiled with BIT7Z_AUTO_FORMAT, which this build does not set.
//
// Returns nullptr when the signature is not recognised.
static const bit7z::BitInFormat *formatFromSignature(const std::string &filePath)
{
    QFile f(QString::fromStdString(filePath));
    if (!f.open(QIODevice::ReadOnly)) {
        return nullptr;
    }
    // 265 bytes: enough for every signature below, including tar's "ustar",
    // which sits at offset 257.
    const QByteArray head = f.read(265);

    auto startsWith = [&head](const char *sig, int len) {
        return head.size() >= len && std::memcmp(head.constData(), sig, len) == 0;
    };

    if (startsWith("\x37\x7A\xBC\xAF\x27\x1C", 6)) return &bit7z::BitFormat::SevenZip;
    // RAR5 extends the RAR4 signature by one byte, so test the longer one first.
    if (startsWith("Rar!\x1A\x07\x01\x00", 8))     return &bit7z::BitFormat::Rar5;
    if (startsWith("Rar!\x1A\x07\x00", 7))         return &bit7z::BitFormat::Rar;
    // Zip: local header, plus the empty-archive and spanned variants.
    if (startsWith("PK\x03\x04", 4)
        || startsWith("PK\x05\x06", 4)
        || startsWith("PK\x07\x08", 4))            return &bit7z::BitFormat::Zip;
    if (startsWith("\xFD" "7zXZ\x00", 6))          return &bit7z::BitFormat::Xz;
    if (startsWith("\x1F\x8B", 2))                 return &bit7z::BitFormat::GZip;
    if (startsWith("BZh", 3))                      return &bit7z::BitFormat::BZip2;
    if (head.size() >= 262 && std::memcmp(head.constData() + 257, "ustar", 5) == 0) {
        return &bit7z::BitFormat::Tar;
    }
    return nullptr;
}

bool Archiver::isRarArchive(const std::string &filePath)
{
    const bit7z::BitInFormat *f = formatFromSignature(filePath);
    return f == &bit7z::BitFormat::Rar || f == &bit7z::BitFormat::Rar5;
}

bit7z::BitArchiveReader Archiver::getReader(std::string filePath)
{
    // Make sure library is loaded
    Archiver::loadBit7zLib();

    // Prefer what the file says it is over what it is named.
    if (const bit7z::BitInFormat *detected = formatFromSignature(filePath)) {
        return bit7z::BitArchiveReader{*lib, filePath, *detected};
    }

    // Unrecognised header: fall back to the file extension.
    // Downloads are saved as "<original-name>.tmp" (e.g. "foo.zip.tmp"),
    // so strip a trailing ".tmp" before inspecting the extension.
    QString path = QString::fromStdString(filePath);
    if (path.endsWith(".tmp", Qt::CaseInsensitive)) {
        path.chop(4);
    }

    if (path.endsWith(".zip", Qt::CaseInsensitive)) {
        return bit7z::BitArchiveReader{*lib, filePath, bit7z::BitFormat::Zip};
    }
    if (path.endsWith(".rar", Qt::CaseInsensitive)) {
        return bit7z::BitArchiveReader{*lib, filePath, bit7z::BitFormat::Rar5};
    }

    // Default: 7zip archive
    return bit7z::BitArchiveReader{*lib, filePath, bit7z::BitFormat::SevenZip};
}

bit7z::BitFileExtractor Archiver::getExtractor()
{

    // Make sure library is loaded
    Archiver::loadBit7zLib();

    // Create the reader
    // For now only 7z because we use .tmp file extension
    return bit7z::BitFileExtractor{*lib, bit7z::BitFormat::SevenZip};
}

bit7z::BitFileCompressor Archiver::getCompressor()
{
    // Make sure library is loaded
    Archiver::loadBit7zLib();

    // Create the compressor
    // For now only 7z
    return bit7z::BitFileCompressor{*lib, bit7z::BitFormat::SevenZip};
}

bool Archiver::compressSingleFile(QFile *inputFile, std::string outputPath)
{
    bit7z::BitFileCompressor compressor = Archiver::getCompressor();

    qDebug() << inputFile->fileName().toStdString();
    qDebug() << outputPath;

    try {
        compressor.compress({inputFile->fileName().toStdString()}, outputPath);

        return true;

    } catch ( const bit7z::BitException& ex ) {

        qWarning() << "Failed to compress single file:" << ex.what();
    }

    return false;
}

int64_t Archiver::testArchiveAndGetSize(QFile *archiveFile)
{
    // Get file info for the archive file
    QFileInfo archiveFileInfo(archiveFile->filesystemFileName());

    // Get archive reader
    bit7z::BitArchiveReader archive = Archiver::getReader(
        archiveFileInfo.absoluteFilePath().toStdString()
    );

    try{

        // Test the archive
        // Throws a BitException when it is invalid
        archive.test();

        // Return the total size of the uncompressed files
        return static_cast<int64_t>(archive.size());

    } catch (const bit7z::BitException& ex) {

        qWarning() << "Archive test failure:" << ex.what();
        // Signed on purpose: this used to be uint64_t, so -1 wrapped to UINT64_MAX
        // and every caller's "if (archiveSize < 0)" was always false. A corrupt or
        // truncated archive sailed straight through to extraction.
        return -1;
    }
}
