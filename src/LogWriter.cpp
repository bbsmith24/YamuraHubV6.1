#include "LogWriter.h"
#include <SD.h>

static FsFile   logFile;
static bool     logOpen = false;
static bool     writeError = false;
static uint8_t  buf[LOG_BUFFER_SIZE];
static size_t   bufLen = 0;
static uint64_t bytesAccepted = 0;
static uint32_t lastFlushMs = 0;

// Write `n` bytes from the start of the buffer to the card and shift the rest
// down. Callers pass whole sectors (except at close) so card writes stay
// 512-byte aligned.
static void WriteOut(size_t n)
{
    if (n == 0)
    {
        return;
    }
    size_t written = logFile.write(buf, n);
    if (written != n)
    {
        writeError = true;
    }
    if (n < bufLen)
    {
        memmove(buf, buf + n, bufLen - n);
    }
    bufLen -= n;
}

static void WriteWholeSectors()
{
    WriteOut(bufLen & ~(size_t)511);
}

bool LogOpen(const char* path, uint64_t preallocBytes, bool* preallocated)
{
    if (logOpen)
    {
        LogClose();
    }
    bufLen = 0;
    bytesAccepted = 0;
    writeError = false;
    if (preallocated)
    {
        *preallocated = false;
    }

    logFile = SD.sdfs.open(path, O_RDWR | O_CREAT | O_TRUNC);
    if (!logFile)
    {
        return false;
    }
    if (preallocBytes > 0)
    {
        // Needs one contiguous free run; fails on a full or fragmented card.
        bool ok = logFile.preAllocate(preallocBytes);
        if (preallocated)
        {
            *preallocated = ok;
        }
        if (!ok)
        {
            Serial.printf("Log: could not pre-allocate %llu bytes - logging without it\n",
                          (unsigned long long)preallocBytes);
        }
    }
    logOpen = true;
    lastFlushMs = millis();
    return true;
}

void LogWrite(const void* data, size_t len)
{
    if (!logOpen)
    {
        return;
    }
    const uint8_t* p = (const uint8_t*)data;
    bytesAccepted += len;
    while (len > 0)
    {
        if (bufLen == LOG_BUFFER_SIZE)
        {
            WriteOut(LOG_BUFFER_SIZE);
        }
        size_t room = LOG_BUFFER_SIZE - bufLen;
        size_t n = len < room ? len : room;
        memcpy(buf + bufLen, p, n);
        bufLen += n;
        p += n;
        len -= n;
    }
}

void LogWriteByte(uint8_t b)
{
    LogWrite(&b, 1);
}

void LogService(uint32_t nowMs)
{
    if (!logOpen || (uint32_t)(nowMs - lastFlushMs) < LOG_FLUSH_INTERVAL_MS)
    {
        return;
    }
    lastFlushMs = nowMs;
    WriteWholeSectors();
    // sync() writes the cached sector and the directory entry. With
    // pre-allocation no FAT changes are pending, so this stays short.
    logFile.sync();
}

uint64_t LogClose()
{
    if (!logOpen)
    {
        return 0;
    }
    WriteOut(bufLen);
    // Drop the unused part of the reservation: truncate at the current
    // position (= bytes written). Also correct without pre-allocation.
    logFile.truncate();
    logFile.sync();
    uint64_t size = logFile.fileSize();
    logFile.close();
    logOpen = false;
    return size;
}

bool LogIsOpen()
{
    return logOpen;
}

uint64_t LogBytesWritten()
{
    return bytesAccepted;
}

bool LogWriteError()
{
    return writeError;
}
