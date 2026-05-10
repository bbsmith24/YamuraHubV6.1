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
    //static constexpr uint16_t FTP_CHUNK_SIZE = 512;
    static constexpr uint16_t FTP_CHUNK_SIZE = 256;
    static constexpr unsigned long RESPONSE_TIMEOUT = 8000;

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
    bool UploadFileFromSDtoFTPServer(const char* remoteFile, const char* localFilePath);

private:
    WiFiClient ftpClient;
    WiFiClient dataClient;
    const char* ftpServer;

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
