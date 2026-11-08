#ifndef LOG_WRITER_H
#define LOG_WRITER_H

#include <Arduino.h>

// Buffered, pre-allocated log file writer (SdFat FsFile underneath SD.h).
//
// - LogOpen() creates the file and reserves `preallocBytes` of contiguous space
//   up front, so the card never allocates clusters (FAT writes) during the run.
//   If the card has no contiguous run that large it logs without pre-allocation.
// - LogWrite() only copies into a RAM buffer. Full sectors go to the card when
//   the buffer fills, and LogService() pushes whole sectors + syncs every
//   LOG_FLUSH_INTERVAL_MS, so a power cut loses at most about that much data.
// - LogClose() writes the rest, truncates the file to the bytes actually
//   written (dropping the unused reservation) and closes it.
//
// All calls must come from the main context (CAN callbacks run from
// canFD.events(), not the ISR, once events() has been called).

#define LOG_BUFFER_SIZE        16384   // RAM buffer; multiple of 512
#define LOG_FLUSH_INTERVAL_MS  1000    // periodic flush to the card

// Open (create/overwrite) path for logging. Returns false if the file could not
// be created. *preallocated (optional) reports whether the reservation worked.
bool LogOpen(const char* path, uint64_t preallocBytes, bool* preallocated = nullptr);

// Append bytes to the log (buffered). No-op if no log is open.
void LogWrite(const void* data, size_t len);
void LogWriteByte(uint8_t b);

// Call often from the logging loop: periodic flush of whole sectors + sync.
void LogService(uint32_t nowMs);

// Flush, truncate to real length, close. Safe to call when nothing is open.
// Returns the final file size in bytes.
uint64_t LogClose();

bool     LogIsOpen();
uint64_t LogBytesWritten();   // bytes accepted so far (buffered + on card)
bool     LogWriteError();     // a card write fell short at some point

#endif // LOG_WRITER_H
