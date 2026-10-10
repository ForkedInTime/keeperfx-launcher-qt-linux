#pragma once

#include <optional>

#include <QFile>
#include <QStringList>

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

    // Compress several files into one new archive in a single pass. Calling
    // compressSingleFile() once per file onto the same path does not work: the
    // second call finds the archive already there and bit7z refuses to update
    // it, so only the first file was ever kept.
    static bool compressFiles(const QStringList &inputFiles, std::string outputPath);

    // Extract everything except symbolic links, which are skipped and logged.
    //
    // Every archive the launcher opens is either a release payload (7z stores
    // the file a link points to, never the link) or a user-uploaded Workshop
    // item, so a link in one is never needed -- and it is dangerous. bit7z
    // checks each link's target only as text: a link "x" -> ".", then "x/l" ->
    // "..", then a file "l/f" lands one directory ABOVE the output directory,
    // and deeper chains reach $HOME (~/.bashrc, ~/.config/autostart). Skipping
    // links removes that whole class rather than re-checking each path.
    //
    // Throws bit7z::BitException like extractTo() does, including when the
    // archive held nothing but links.
    static void extractWithoutLinks(const bit7z::BitArchiveReader &reader, const std::string &outputDir);

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
