#pragma once

#include <optional>

#include <QFile>

#include <bit7z/bitextractor.hpp>
#include <bit7z/bitabstractarchivehandler.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitfilecompressor.hpp>
#include <bit7z/bitfileextractor.hpp>

class Archiver
{
public:
    static bit7z::BitArchiveReader getReader(std::string filePath);
    static bit7z::BitFileExtractor getExtractor();
    static bit7z::BitFileCompressor getCompressor();

    static bool compressSingleFile(QFile *inputFile, std::string outputPath);

    // True when the file is a RAR archive, judged by its signature.
    //
    // Worth asking separately because the 7-Zip library shipped beside the
    // launcher can LIST a RAR but cannot decompress one: it carries the
    // NArchive::NRar5 handler and no NCompress::NRar5 decoder, so extraction
    // dies part-way with the opaque "Unsupported method". Callers use this to
    // say something useful instead of repeating that.
    static bool isRarArchive(const std::string &filePath);

    // Uncompressed total, or -1 when the archive fails its integrity test.
    static int64_t testArchiveAndGetSize(QFile *archiveFile);

private:

    static std::optional<bit7z::Bit7zLibrary> lib;
    static void loadBit7zLib();

};
