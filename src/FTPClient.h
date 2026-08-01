#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

#include <Arduino.h>
#include <WiFiNINA.h>
#include <cstdint>
#include <cstring>

/**
 * @brief FTPClient - A simple FTP client for Teensy with WiFi support
 * 
 * This class provides basic FTP operations including listing directories,
 * downloading files, and uploading files (both from memory and SD card).
 */
class FTPClient {
public:
    static constexpr uint16_t DEFAULT_FTP_PORT = 21;
    int ftpListenerPort = 21;
    // Speed tuning result (measured on the AirLift over SPI):
    //   - 512 B chunks fail with a partial send regardless of pacing -> the
    //     nina-fw has a practical ~256 B per-sendData ceiling. Keep chunk = 256.
    //   - Pacing below 10 ms (tried 5 ms and 2 ms) also fails with a partial
    //     send: the module's TCP buffer needs ~10 ms to drain each 256 B chunk.
    // 256 B / 10 ms (~25 KB/s) is the reliable ceiling for this hardware/link.
    static constexpr uint16_t FTP_CHUNK_SIZE = 256;
    static constexpr unsigned long RESPONSE_TIMEOUT = 8000;
    // The car is out of WiFi range while running and only re-enters coverage in
    // the pits, so every upload re-associates from a cold state. Poll status for
    // this long per attempt, and re-issue begin() this many times, before giving up.
    static constexpr unsigned long WIFI_CONNECT_TIMEOUT = 20000;
    static constexpr int WIFI_CONNECT_ATTEMPTS = 3;
    // WiFiClient::flush() is a no-op in this fork and checkDataSent() only
    // confirms the ESP32 buffered a chunk, not that it reached the server. Pace
    // writes so the module's TCP buffer can't build a huge backlog, and let the
    // tail drain before closing the data socket - otherwise STOR closes early
    // and the server records a truncated file (still replying 226).
    static constexpr unsigned long FTP_CHUNK_PACING_MS = 10;
    static constexpr unsigned long FTP_DRAIN_MS = 400;

    /*
     * @brief Connect to FTP server with given credentials
     * @param host FTP server hostname or IP
     * @param port FTP server port (default 21)
     * @param user Username for authentication
     * @param pass Password for authentication
     * @return true if connection successful, false otherwise
     */
    bool FTPConnect(const char* host, uint16_t port, const char* user, const char* pass);

    /**
     * @brief Disconnect from FTP server
     */
    void FTPDisconnect();

    /**
     * @brief Check if currently connected to FTP server
     * @return true if connected, false otherwise
     */
    bool IsFTPConnected();

    /**
     * @brief List ftp server directory contents
     * @param path Remote directory path
     * @return true if successful, false otherwise
     */
    bool ListFTPServerDirectory(const char* path);
/**
     * @brief List local sd card directory contents
     * @param path local directory path
     * @return true if successful, false otherwise
     */
    bool ListLocalDirectory(const char* path);
    /**
     * @brief Download file from FTP server
     * @param remoteFile Remote filename to download
     * @return true if successful, false otherwise
     */
    bool DownloadFTPServerFile(const char* remoteFile);

    /**
     * @brief Download file from FTP server to local SD card
     * @param remoteFile Remote filename to download
     * @param localFilePath Local SD card file path to save into
     * @return true if successful, false otherwise
     */
    bool DownloadFileFromFTPServerToSD(const char* remoteFile, const char* localFilePath);

    /**
     * @brief Upload file to FTP server from memory
     * @param remoteFile Remote filename to create
     * @param data Pointer to data buffer
     * @param dataSize Size of data in bytes
     * @return true if successful, false otherwise
     */
    bool UploadFileToFTPServer(const char* remoteFile, const uint8_t* data, size_t dataSize);

    /**
     * @brief Upload file to FTP server from SD card
     * @param remoteFile Remote filename to create
     * @param localFilePath Local SD card file path
     * @return true if successful, false otherwise
     */
    bool UploadFileFromSDtoFTPServer(const char* remoteFile, const char* localFilePath, char* returnMessage);

private:
    WiFiClient ftpClient;
    WiFiClient dataClient;
    const char* ftpServer;

    /**
     * @brief Associate with the WiFi network, polling status with retries.
     *
     * WiFiNINA's begin() returns whatever status its internal poll happened to
     * end on, which is often WL_DISCONNECTED/WL_CONNECT_FAILED on a slow-but-fine
     * association. This polls WiFi.status() until WL_CONNECTED or a timeout, and
     * re-issues begin() a few times, treating "out of range" as a clean failure.
     * @return true once associated, false if it could not connect (likely out of range)
     */
    bool EnsureWiFiConnected();

    /**
     * @brief Read FTP response from server
     * @return Response string from server
     */
    String ReadFTPResponse();

    /**
     * @brief Send FTP command to server
     * @param cmd Command string to send
     * @return Server response
     */
    String SendFTPCommand(const char* cmd);

    /**
     * @brief Enter passive mode and get data port
     * @param dataPort Output parameter for data port number
     * @return true if successful, false otherwise
     */
    bool EnterFTPPassiveMode(uint16_t& dataPort);

    /**
     * @brief Check if response starts with expected code
     * @param response Response string
     * @param code Expected response code (e.g., "226")
     * @return true if response starts with code, false otherwise
     */
    static bool IsFTPResponseCode(const String& response, const char* code);

    /**
     * @brief Parse PASV response to extract data port
     * @param response PASV response string
     * @param dataPort Output parameter for data port
     * @return true if parsing successful, false otherwise
     */
    static bool ParseFTPPASVResponse(const String& response, uint16_t& dataPort);
};

#endif // FTP_CLIENT_H
