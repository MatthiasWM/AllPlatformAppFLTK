//
//  IAGeometryReader.cpp
//
//  Copyright (c) 2013-2018 Matthias Melcher. All rights reserved.
//


#include "IAGeometryReader.h"

#include "Iota.h"
#include "IAGeometryReaderBinaryStl.h"
#include "IAGeometryReaderTextStl.h"

#include <FL/fl_utf8.h>

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
# include <io.h>
#else
# include <unistd.h>
# include <sys/mman.h>
#endif


/**
 * Create a file reader for the indicated file.
 *
 * \param filename read from this file
 *
 * \return 0 if the format is not supported
 *
 * \todo first look at the filename extension and prefer that file format
 * \todo set error to Unsupported File Format
 */
std::shared_ptr<IAGeometryReader> IAGeometryReader::findReaderFor(const char *filename)
{
    std::shared_ptr<IAGeometryReader> reader = nullptr;
    if (!reader)
        reader = IAGeometryReaderBinaryStl::findReaderFor(filename);
    if (!reader)
        reader = IAGeometryReaderTextStl::findReaderFor(filename);
    return reader;
}


/**
 * Create a reader for the indicated memory block.
 *
 * \param name similar to a filename, the extension of the name will help to
 *      determine the file type.
 * \param data verbatim copy of the file in memory
 * \param size number of bytes in that memory block
 *
 * \return 0 if the format is not supported
 *
 * \todo first look at the filename extension and prefer that file format
 * \todo set error to Unsupported File Format
 */
std::shared_ptr<IAGeometryReader> IAGeometryReader::findReaderFor(const char *name, unsigned char *data, size_t size)
{
    std::shared_ptr<IAGeometryReader> reader = nullptr;
    if (!reader)
        reader = IAGeometryReaderBinaryStl::findReaderFor(name, data, size);
    if (!reader)
        reader = IAGeometryReaderTextStl::findReaderFor(name, data, size);
    return reader;
}


/**
 * Create a universal reader by mapping the file to memory.
 *
 * If the file can't be opened or read, the Iota error system will be used
 * to produce an error message.
 *
 * \param filename read this file
 *
 * \todo Use memory mapped files on MSWindows as well.
 */
IAGeometryReader::IAGeometryReader(const char *filename)
:   pName(strdup(filename))
{
    int fd = fl_open(filename, O_RDONLY, 0);
    if (fd==-1) {
		Iota.Error.set("Geometry reader", IAError::CantOpenFile_STR_BSD, filename);
		pData = nullptr;
		pSize = 0;
		return;
    }
    struct stat st; fstat(fd, &st);
    size_t len = st.st_size;
    ::lseek(fd, 0, SEEK_SET);

#ifdef _WIN32
	pData = (uint8_t*)malloc(len);
	read(fd, pData, len);
#else
    pData = (uint8_t*)mmap(nullptr, len, PROT_READ,MAP_PRIVATE|MAP_FILE, fd, 0);
//    pData = (uint8_t*)mmap(nullptr, 0, PROT_READ, MAP_FILE, fd, 0);
    if (pData==MAP_FAILED) {
        perror("mmap");
        // panic
    }
#endif

    pCurrData = pData;
    pSize = len;
    pMustUnmapOnDelete = true;

    ::close(fd);
}


/**
 * Create a reader.
 *
 * \param name similar to a filename, the extension of the name will help to
 *      determine the file type.
 * \param data verbatim copy of the file in memory
 * \param size number of bytes in that memory block
 */
IAGeometryReader::IAGeometryReader(const char *name, uint8_t *data, size_t size)
:   pMustUnmapOnDelete( false ),
    pData( data ),
    pCurrData( data ),
    pSize( size ),
    pName(strdup(name))
{
}


/**
 * Release any allocated resources.
 *
 * \todo Use memory mapped files on MSWindows
 */
IAGeometryReader::~IAGeometryReader()
{
    if (pMustUnmapOnDelete) {
#ifdef _WIN32
		free(pData);
#else
        ::munmap((void*)pData, pSize);
#endif
    }
    if (pName)
        free(pName);
}


/**
 * Get a LSB first 32-bit word from memory.
 *
 * \return the next word in the stream.
 */
uint32_t IAGeometryReader::getUInt32LSB() {
    uint32_t ret = 0;
    ret |= *pCurrData++;
    ret |= (*pCurrData++)<<8;
    ret |= (*pCurrData++)<<16;
    ret |= (*pCurrData++)<<24;
    return ret;
}


/**
 Get a LSB first 16-bit word from memory.
 *
 * \return the next word in the stream.
 */
uint16_t IAGeometryReader::getUInt16LSB() {
    uint16_t ret = 0;
    ret |= *pCurrData++;
    ret |= (*pCurrData++)<<8;
    return ret;
}


/**
 * Get a 32bit float from memory
 *
 * \return the next float in the stream.
 */
float IAGeometryReader::getFloatLSB()
{
    union { float f; uint8_t u[4]; };
    u[0] = pCurrData[0];
    u[1] = pCurrData[1];
    u[2] = pCurrData[2];
    u[3] = pCurrData[3];
    float ret = f;
    pCurrData += 4;
    return ret;
}


/**
 * Skip the next n bytes when reading.
 *
 * \param n number of bytes to skip.
 */
void IAGeometryReader::skip(size_t n)
{
    pCurrData += n;
}


/**
 * Find the next keyword in a text file.
 *
 * \return false, if we reached beyond the end of the file.
 *
 * \todo test for end of buffer and other errors!
 * \bug the function returns false, when reading the last word in the file!
 */
bool IAGeometryReader::getWord()
{
    // skip whitespace
    for (;;) {
        uint8_t c = *pCurrData;
        if (c!=' ' && c!='\t' && c!='\r' && c!='\n')
            break;
        pCurrData++;
        if (pCurrData-pData > pSize)
            return false;
    }
    pCurrWord = pCurrData;
    char c = (char)*pCurrData;
    if (isalpha(c) || c=='_') {
        // find the end of a standard 'C' style keyword
        pCurrData++;
        for (;;) {
            char c = (char)*pCurrData;
            if (!isalnum(c) && c!='_')
                break;
            pCurrData++;
        }
        return true;
    }
    if (c=='"') {
        // find the end of a quoted string
        pCurrData++;
        for (;;) {
            char c = (char)*pCurrData;
            if (c=='\\')
                pCurrData++;
            else if (c=='"')
                break;
            pCurrData++;
        }
        return true;
    }
    if (c=='+' || c=='-' || c=='.' || isdigit(c)) {
        // find the end of a number
        pCurrData++;
        for (;;) {
            char c = (char)*pCurrData;
            if (!(isdigit(c) || c=='-' || c=='+' || c=='E' || c=='e' || c=='.'))
                break;
            pCurrData++;
        }
        return true;
    }
    pCurrData++;
    return true;
}


/**
 * Find the next keyword in a text file.
 *
 * \return the next floating point number in the stream.
 */
double IAGeometryReader::getDouble()
{
    getWord();
    double ret = atof((char *)pCurrWord);
    return ret;
}


/**
 * Get the rest of this line
 *
 * \return false, if we reached the end of the file
 *
 * \bug the function returns false too early, when reading the last line.
 */
bool IAGeometryReader::getLine()
{
    pCurrWord = pCurrData;
    for (;;) {
        uint8_t c = *pCurrData;
        if (c=='\r' || c=='\n')
            break;
        pCurrData++;
        if (pCurrData-pData > pSize)
            return false;
    }
    if ( (pCurrData-pData<=pSize) && pCurrData[0]=='\r' && pCurrData[1]=='\n')
        pCurrData++;
    pCurrData++;
    return true;
}


/**
 * Check if the result of getWord() is the specified string.
 *
 * \param key check for this string, case sensitive
 *
 * \return true, if this was the last word read
 */
bool IAGeometryReader::wordIs(const char *key)
{
    size_t len = strlen(key);
    if (pCurrData-pCurrWord != len)
        return false;
    return (strncmp((char*)pCurrWord, key, len)==0);
}


/**
 * Print the result of getWord() to the console.
 */
void IAGeometryReader::printWord()
{
    size_t len = pCurrData-pCurrWord;
    printf("%.*s\n", (int)len, (char*)pCurrWord);
}


