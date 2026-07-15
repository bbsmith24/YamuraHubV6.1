#include <Arduino.h>
#include "FTPClient.h"
#include "WiFiSecrets.h"
#include <SD.h>


static bool WriteAllToFTPClient(WiFiClient& client, const uint8_t* buffer, size_t length, unsigned long timeoutMs = 5000) 
{
    size_t totalWritten = 0;
    unsigned long start = millis();

    while (totalWritten < length && client.connected()) 
    {
        size_t written = client.write(buffer + totalWritten, length - totalWritten);
        if (written > 0) 
        {
            totalWritten += written;
            start = millis();
        }
        else if (millis() - start > timeoutMs) 
        {
            return false;
        } 
        else 
        {
            delay(10);
        }
    }

    return totalWritten == length;
}

bool FTPClient::IsFTPResponseCode(const String& response, const char* code) {
    return response.length() >= 4 && 
           response[0] == code[0] &&
           response[1] == code[1] &&
           response[2] == code[2];
}

bool FTPClient::ParseFTPPASVResponse(const String& response, uint16_t& dataPort) {
    int values[6] = {0, 0, 0, 0, 0, 0};
    int valueIndex = 0;
    bool inParen = false;
    String digits;

    for (unsigned int i = 0; i < response.length(); i++) {
        char c = response[i];
        if (c == '(') {
            inParen = true;
            continue;
        }
        if (c == ')') {
            if (digits.length() > 0 && valueIndex < 6) {
                values[valueIndex++] = digits.toInt();
            }
            break;
        }
        if (!inParen) {
            continue;
        }
        if (isDigit(c)) {
            digits += c;
        } else if (c == ',' && digits.length() > 0) {
            if (valueIndex < 6) {
                values[valueIndex++] = digits.toInt();
            }
            digits = "";
        }
    }

    if (valueIndex != 6) {
        Serial.println("ERROR: Failed to parse PASV response");
        return false;
    }

    dataPort = values[4] * 256 + values[5];
    return true;
}

String FTPClient::ReadFTPResponse() {
    String response;
    unsigned long start = millis();
    bool finished = false;

    Serial.println("DEBUG: Starting ReadFTPResponse()");

    while (!finished && millis() - start < RESPONSE_TIMEOUT)
    {
        // Try to read one line using readStringUntil with a very short timeout
        // The trick: readStringUntil WILL block, but combined with available()
        // and careful timeout handling, we can work around it
        
        if (ftpClient.available()) {
            // Data is available! Read a complete line
            String line = ftpClient.readStringUntil('\n');
            line.trim();
            
            if (line.length() > 0) {
                Serial.print("Received: >>>");
                Serial.print(line);
                Serial.println("<<<");
                
                response += line;
                response += '\n';
                
                // FTP responses can be multi-line
                if (line.length() >= 4 && isDigit(line[0]) && isDigit(line[1]) &&
                    isDigit(line[2])) {
                    if (line[3] == ' ') {
                        // Final line (space after code means last line)
                        finished = true;
                    } else if (line[3] == '-') {
                        // Intermediate line (dash after code means more lines)
                        continue;
                    }
                }
            }
        }
        else
        {
            // No data available, wait a tiny bit and loop again
            // This yields to the WiFi stack to service SPI
            delay(1);
        }
    }

    if (!finished) 
    {
        Serial.println("\nERROR: Response timeout or incomplete");
        Serial.print("Received so far: >>>");
        Serial.print(response);
        Serial.println("<<<");
        Serial.print("Elapsed time: ");
        Serial.print(millis() - start);
        Serial.print("ms, Response length: ");
        Serial.println(response.length());
    }

    return response;
}

String FTPClient::SendFTPCommand(const char* cmd) {
    if (!ftpClient.connected()) {
        Serial.println("ERROR: FTP control connection lost");
        return String();
    }

    Serial.print("> ");
    Serial.println(cmd);
    ftpClient.println(cmd);
    ftpClient.flush();  // Ensure command is sent
    delay(200);  // Give server time to respond
    return ReadFTPResponse();
}

bool FTPClient::FTPConnect(const char* host, uint16_t port, const char* user, const char* pass) 
{
    if (!host || !user || !pass) {
        Serial.println("ERROR: NULL parameter passed to connect()");
        return false;
    }
    // Connect to WiFi
    Serial.print("Connecting to WiFi network '");
    Serial.print(SSID);
    Serial.print("'... ");

    int status = WiFi.begin(SSID, PASSWORD);
    if (status != WL_CONNECTED) {
        Serial.println("FAILED");
        Serial.print("ERROR: WiFi connection failed, status = ");
        Serial.println(status);
        while (true) {
            delay(1000);
        }
    }

    Serial.println("OK");
    Serial.print("Connected to: ");
    Serial.println(SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println();


    Serial.print("Connecting to FTP server ");
    Serial.print(host);
    Serial.print(":");
    Serial.println(port);

    ftpServer = host;

    if (!ftpClient.connect(host, port)) 
    {
        Serial.println("ERROR: Failed to open FTP control connection");
        return false;
    }
    Serial.println("OK: FTP control connection opened");
    
    // Wait for welcome message - give it time to arrive
    delay(500);
    
    String resp = ReadFTPResponse();
    if (!IsFTPResponseCode(resp, "220")) 
    {
        Serial.print("ERROR: Unexpected FTP welcome response: >>>");
        Serial.print(resp);
        Serial.println("<<<");
        ftpClient.stop();
        return false;
    }
    Serial.println("OK: Welcome message received");

    // Send USER command
    String userCmd = String("USER ") + user;
    resp = SendFTPCommand(userCmd.c_str());
    if (!IsFTPResponseCode(resp, "331") && !IsFTPResponseCode(resp, "230")) 
    {
        Serial.println("ERROR: FTP user authentication failed");
        ftpClient.stop();
        return false;
    }
    Serial.println("OK: FTP USER sent");

    // Send PASS command if needed (230 means logged in without password)
    if (!IsFTPResponseCode(resp, "230")) 
    {
        String passCmd = String("PASS ") + pass;
        resp = SendFTPCommand(passCmd.c_str());
        if (!IsFTPResponseCode(resp, "230")) 
        {
            Serial.println("ERROR: FTP password authentication failed");
            Serial.print("Received response: >>>");
            Serial.print(resp);
            Serial.println("<<<");
            ftpClient.stop();
            return false;
        }
    }
    Serial.println("OK: FTP PASSWORD sent");

    // Set binary mode
    String typeResp = SendFTPCommand("TYPE I");
    if (!IsFTPResponseCode(typeResp, "200")) {
        Serial.println("ERROR: Failed to set FTP binary mode");
        Serial.print("Received: >>>");
        Serial.print(typeResp);
        Serial.println("<<<");
        ftpClient.stop();
        return false;
    }
    Serial.println("OK: Binary mode enabled");
    Serial.println("FTP connection established");
    return true;
}

void FTPClient::FTPDisconnect() {
    if (ftpClient.connected()) 
    {
        SendFTPCommand("QUIT");
        ftpClient.stop();
    }
    if (dataClient.connected()) 
    {
        dataClient.stop();
    }
    Serial.println("FTP connection closed");
    WiFi.disconnect();
    Serial.println("WiFi connection closed");
}

bool FTPClient::IsFTPConnected() {
    return ftpClient.connected();
}

bool FTPClient::EnterFTPPassiveMode(uint16_t& dataPort) {
    String resp = SendFTPCommand("PASV");
    if (!IsFTPResponseCode(resp, "227")) {
        Serial.println("ERROR: PASV command failed");
        return false;
    }

    return ParseFTPPASVResponse(resp, dataPort);
}

bool FTPClient::ListFTPServerDirectory(const char* path) 
{
    if (!ftpClient.connected()) {
        Serial.println("ERROR: Not connected to FTP server");
        return false;
    }

    if (!path) {
        Serial.println("ERROR: NULL path parameter");
        return false;
    }

    Serial.print("Listing remote directory: ");
    Serial.println(path);

    uint16_t dataPort = 0;
    if (!EnterFTPPassiveMode(dataPort)) {
        return false;
    }

    Serial.print("Opening data connection to port ");
    Serial.println(dataPort);

    if (!dataClient.connect(ftpServer, dataPort)) {
        Serial.println("ERROR: Data connection failed");
        return false;
    }

    String listCmd = String("LIST ") + path;
    ftpClient.println(listCmd);
    String status = ReadFTPResponse();
    if (!IsFTPResponseCode(status, "150")) {
        Serial.println("ERROR: LIST command failed");
        dataClient.stop();
        return false;
    }

    Serial.println("Directory contents:");
    unsigned long start = millis();
    while (dataClient.connected() && millis() - start < RESPONSE_TIMEOUT) {
        while (dataClient.available()) {
            Serial.write(dataClient.read());
            start = millis();
        }
    }

    dataClient.stop();
    String finish = ReadFTPResponse();
    return IsFTPResponseCode(finish, "226");
}

bool FTPClient::ListLocalDirectory(const char* path) 
{
    if (!path) {
        Serial.println("ERROR: NULL path parameter");
        return false;
    }

    Serial.print("Listing local directory: ");
    Serial.println(path);

    File dir = SD.open("/");

    while (true) 
    {
        File entry = dir.openNextFile();
        if (!entry) 
        {
            //Serial.println("** no more files **");
            break;
        }
        //printSpaces(numSpaces);
        Serial.print(entry.isDirectory() ? "DIR " : "FILE");
        Serial.print(" ");
        Serial.print(entry.name());
        if (entry.isDirectory())
        // && 
        //     strcmp(entry.name(), ".") != 0 && 
        //     strcmp(entry.name(), "..") != 0 && 
        //     strcmp(entry.name(), "System Volume Information") != 0 &&
        //     strstr(entry.name(), "FOUND.") != 0)
        {
            //Serial.println();
            //ListLocalDirectory(entry.name());
            Serial.print("skipping directory ");
            Serial.println(entry.name());
        } 
        else 
        {
            // files have sizes, directories do not
            int n = log10f(entry.size());
            if (n < 0) n = 10;
            if (n > 10) n = 10;
            //printSpaces(50 - numSpaces - strlen(entry.name()) - n);
            Serial.print("  ");
            Serial.print(entry.size(), DEC);
            //DateTimeFields datetime;
            //if (entry.getModifyTime(datetime)) 
            //{
            //  //printSpaces(4);
            //  //printTime(datetime);
            //}
            Serial.println();
        }
        entry.close();
    }
    dir.close();
    return true;
}

bool FTPClient::DownloadFTPServerFile(const char* remoteFile) {
    if (!ftpClient.connected()) {
        Serial.println("ERROR: Not connected to FTP server");
        return false;
    }

    if (!remoteFile) {
        Serial.println("ERROR: NULL remoteFile parameter");
        return false;
    }

    Serial.print("Downloading file: ");
    Serial.println(remoteFile);

    uint16_t dataPort = 0;
    if (!EnterFTPPassiveMode(dataPort)) {
        return false;
    }

    if (!dataClient.connect(ftpServer, dataPort)) {
        Serial.println("ERROR: Data connection failed");
        return false;
    }

    String retrieveCmd = String("RETR ") + remoteFile;
    ftpClient.println(retrieveCmd);
    String status = ReadFTPResponse();
    if (!IsFTPResponseCode(status, "150")) {
        Serial.println("ERROR: RETR command failed");
        dataClient.stop();
        return false;
    }

    Serial.println("File contents:");
    unsigned long start = millis();
    while (dataClient.connected() && millis() - start < RESPONSE_TIMEOUT) {
        while (dataClient.available()) {
            dataClient.read(); //Serial.write(dataClient.read());
            start = millis();
        }
    }

    dataClient.stop();
    String finish = ReadFTPResponse();
    return IsFTPResponseCode(finish, "226");
}

bool FTPClient::UploadFileToFTPServer(const char* remoteFile, const uint8_t* data, size_t dataSize) {
    if (!ftpClient.connected()) {
        Serial.println("ERROR: Not connected to FTP server");
        return false;
    }

    if (!remoteFile || !data) {
        Serial.println("ERROR: NULL parameter passed to uploadFile()");
        return false;
    }

    if (dataSize == 0) {
        Serial.println("ERROR: Cannot upload empty file (dataSize = 0)");
        return false;
    }

    Serial.print("Uploading file: ");
    Serial.print(remoteFile);
    Serial.print(" (");
    Serial.print(dataSize);
    Serial.println(" bytes)");

    uint16_t dataPort = 0;
    if (!EnterFTPPassiveMode(dataPort)) {
        return false;
    }

    if (!dataClient.connect(ftpServer, dataPort)) {
        Serial.println("ERROR: Data connection failed");
        return false;
    }

    String storCmd = String("STOR ") + remoteFile;
    ftpClient.println(storCmd);
    String status = ReadFTPResponse();
    if (!IsFTPResponseCode(status, "150")) {
        Serial.println("ERROR: STOR command failed");
        dataClient.stop();
        return false;
    }

    // Send data in chunks
    size_t sent = 0;
    size_t chunkSize;
    unsigned long lastProgress = millis();

    while (sent < dataSize) {
        chunkSize = (dataSize - sent > FTP_CHUNK_SIZE) ? FTP_CHUNK_SIZE : (dataSize - sent);

        if (!WriteAllToFTPClient(dataClient, data + sent, chunkSize)) {
            Serial.println("ERROR: Failed to write data to FTP server");
            dataClient.stop();
            return false;
        }
        sent += chunkSize;

        // Report progress every 500ms
        if (millis() - lastProgress >= 500) {
            Serial.print("Upload progress: ");
            Serial.print((sent * 100) / dataSize);
            Serial.print("% (");
            Serial.print(sent);
            Serial.print("/");
            Serial.print(dataSize);
            Serial.println(" bytes)");
            lastProgress = millis();
        }

        // Yield to WiFi stack
        delay(1);
    }

    dataClient.stop();
    Serial.println("Upload complete, waiting for server confirmation...");

    String finish = ReadFTPResponse();
    if (!IsFTPResponseCode(finish, "226")) {
        Serial.println("ERROR: FTP upload did not complete successfully");
        Serial.println(finish);
        return false;
    }

    Serial.println("Upload successful!");
    return true;
}

bool FTPClient::UploadFileFromSDtoFTPServer(const char* remoteFile, const char* localFilePath, char* returnMessage) 
{
    Serial.print("DEBUG: uploadFileFromStorage() local ");
    Serial.print(localFilePath);
    Serial.print(" remote ");    
    Serial.println(remoteFile);

    if (!FTPConnect(FTP_SERVER, 21, FTP_USER, FTP_PASS)) 
    {
        Serial.println("ERROR: FTP connection failed!");
        int waitCount = 0;
        while (true) 
        {
            delay(1000);
            waitCount++;
            if (waitCount >= 20) 
            {
                Serial.println("Waiting for FTP connection...");
                sprintf(returnMessage, "ERROR: FTP connection failed after %d seconds", waitCount);
                return false;
            }
        }
    }

    if (!remoteFile || !localFilePath) 
    {
        Serial.println("ERROR: NULL parameter passed to uploadFileFromStorage()");
        sprintf(returnMessage, "ERROR: NULL parameter passed to uploadFileFromStorage()");
        return false;
    }

    //Serial.print("Uploading file from storage: ");
    //Serial.print(localFilePath);
    //Serial.print(" -> ");
    //Serial.println(remoteFile);

    // Note: This requires #include <SD.h> in the main file
    // File handling code is commented here as this is a demonstration
    // Uncomment if SD.h is available in your project
    
    // Set binary mode
    SendFTPCommand("TYPE I");

    File localFile = SD.open(localFilePath, FILE_READ);
    if (!localFile) 
    {
        Serial.print("ERROR: Failed to open local file: ");
        Serial.println(localFilePath);
        return false;
    }
    Serial.println("OK: Local file opened successfully");

    size_t fileSize = localFile.size();
    Serial.print("Local file size: ");
    Serial.print(fileSize);
    Serial.println(" bytes");

    if (fileSize == 0) 
    {
        Serial.println("ERROR: Cannot upload empty file");
        sprintf(returnMessage, "ERROR: Cannot upload empty file");
        localFile.close();
        return false;
    }

    uint16_t dataPort = 0;
    if (!EnterFTPPassiveMode(dataPort)) 
    {
        localFile.close();
        return false;
    }

    if (!dataClient.connect(ftpServer, dataPort)) 
    {
        Serial.println("ERROR: Data connection failed");
        localFile.close();
        return false;
    }
    Serial.println("OK: Data connection established, begin upload");  

    String storCmd = String("STOR ") + remoteFile;
    ftpClient.println(storCmd);
    String status = ReadFTPResponse();
    if (!IsFTPResponseCode(status, "150")) 
    {
        Serial.println("ERROR: STOR command failed");
        sprintf(returnMessage, "ERROR: STOR command failed");
        dataClient.stop();
        localFile.close();
        return false;
    }

    uint8_t buffer[FTP_CHUNK_SIZE];
    size_t sent = 0;
    size_t bytesRead;
    unsigned long lastProgress = millis();

    while ((bytesRead = localFile.read(buffer, FTP_CHUNK_SIZE)) > 0) 
    {
        if (!WriteAllToFTPClient(dataClient, buffer, bytesRead)) 
        {
            Serial.println("ERROR: Failed to write to FTP server");
            sprintf(returnMessage, "ERROR: Failed to write to FTP server");
            localFile.close();
            dataClient.stop();
            return false;
        }
        sent += bytesRead;

        if (millis() - lastProgress >= 500) {
            Serial.print("Upload progress: ");
            Serial.print((sent * 100) / fileSize);
            Serial.print("% (");
            Serial.print(sent);
            Serial.print("/");
            Serial.print(fileSize);
            Serial.println(" bytes)");
            lastProgress = millis();
        }
        delay(10);
    }

    localFile.close();
    dataClient.stop();
    Serial.print("Sent ");
    Serial.print(sent);
    Serial.print(" bytes, expected to send ");
    Serial.print(fileSize);
    Serial.println(" bytes");    
    Serial.println("Upload complete, waiting for server confirmation...");

    String finish = ReadFTPResponse();
    if (!IsFTPResponseCode(finish, "226")) 
    {
        Serial.print("ERROR: ");
        Serial.print(finish);
        Serial.println(" - upload did not complete successfully");
        sprintf(returnMessage, "ERROR: upload did not complete successfully");
    }
    FTPDisconnect();   
    Serial.println("Upload successful!");
    sprintf(returnMessage, "Upload successful");
    return true;
}

bool FTPClient::DownloadFileFromFTPServerToSD(const char* remoteFile, const char* localFilePath) 
{
    int bytesRead = 0;
    int totalBytesRead = 0;
    if (!ftpClient.connected()) {
        Serial.println("ERROR: Not connected to FTP server");
        return false;
    }

    if (!remoteFile) {
        Serial.println("ERROR: NULL remoteFile parameter");
        return false;
    }

    Serial.print("Downloading file: ");
    Serial.println(remoteFile);

    // Set binary mode
    SendFTPCommand("TYPE I");

    File localFile = SD.open(localFilePath, FILE_WRITE);
    if (!localFile) 
    {
        Serial.print("ERROR: Failed to open local file: ");
        Serial.println(localFilePath);
        return false;
    }
    Serial.println("OK: Local file opened successfully");

    uint16_t dataPort = 0;
    if (!EnterFTPPassiveMode(dataPort)) {
        return false;
    }

    if (!dataClient.connect(ftpServer, dataPort)) {
        Serial.println("ERROR: Data connection failed");
        return false;
    }

    String retrieveCmd = String("RETR ") + remoteFile;
    ftpClient.println(retrieveCmd);
    String status = ReadFTPResponse();
    if (!IsFTPResponseCode(status, "150")) {
        Serial.println("ERROR: RETR command failed");
        dataClient.stop();
        return false;
    }

    unsigned long start = millis();
    while (dataClient.connected() && millis() - start < RESPONSE_TIMEOUT) {
        while (dataClient.available()) {
            bytesRead = dataClient.read(); //Serial.write(dataClient.read());
            localFile.write(bytesRead);
            totalBytesRead += bytesRead;
            start = millis();
        }
    }

    Serial.println("Upload complete");
    Serial.print("Total bytes sent: ");    
    Serial.println(totalBytesRead);

    localFile.close();
    dataClient.stop();
    String finish = ReadFTPResponse();
    return IsFTPResponseCode(finish, "226");
}
