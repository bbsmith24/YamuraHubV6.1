#include <Arduino.h>
#include "FTPClient.h"
#include "WiFiSecrets.h"
#include <SD.h>


static bool WriteAllToFTPClient(WiFiClient& client, const uint8_t* buffer, size_t length, unsigned long timeoutMs = 10000)
{
    size_t totalWritten = 0;
    unsigned long start = millis();

    while (totalWritten < length)
    {
        size_t written = client.write(buffer + totalWritten, length - totalWritten);
        if (written > 0)
        {
            totalWritten += written;
            start = millis();
        }
        else
        {
            // write() returned 0: NINA send buffer momentarily full, or the
            // socket is gone. Only bail if it is really gone or we have stalled
            // past the timeout - a transient blip must not abort (and truncate)
            // the transfer. connected() is checked here (not every iteration)
            // because it costs an SPI round-trip and can close a socket that is
            // briefly in a transitional TCP state.
            if (!client.connected())
            {
                Serial.println("ERROR: data connection dropped mid-write");
                return false;
            }
            if (millis() - start > timeoutMs)
            {
                Serial.println("ERROR: timed out waiting for NINA send buffer to drain");
                return false;
            }
            delay(5);
        }
    }

    return true;
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

bool FTPClient::EnsureWiFiConnected()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("OK: WiFi already connected");
        return true;
    }

    for (int attempt = 1; attempt <= WIFI_CONNECT_ATTEMPTS; attempt++)
    {
        Serial.print("Connecting to WiFi network '");
        Serial.print(SSID);
        Serial.print("/");
        Serial.print(PASSWORD);
        Serial.print("' (attempt ");
        Serial.print(attempt);
        Serial.print(" of ");
        Serial.print(WIFI_CONNECT_ATTEMPTS);
        Serial.println(")...");

        WiFi.begin(SSID, PASSWORD);

        // Poll status ourselves rather than trusting begin()'s single return.
        unsigned long start = millis();
        while (millis() - start < WIFI_CONNECT_TIMEOUT)
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                Serial.print("OK: WiFi connected, IP address ");
                Serial.println(WiFi.localIP());
                return true;
            }
            delay(250);
        }

        Serial.print("WiFi not connected within ");
        Serial.print(WIFI_CONNECT_TIMEOUT / 1000);
        Serial.print("s (status = ");
        Serial.print(WiFi.status());
        Serial.println("), likely still out of range");

        // Drop the half-open association so the next begin() starts clean.
        WiFi.disconnect();
        delay(500);
    }

    Serial.println("ERROR: WiFi connection failed (out of range?)");
    return false;
}

bool FTPClient::FTPConnect(const char* host, uint16_t port, const char* user, const char* pass)
{
    if (!host || !user || !pass) {
        Serial.println("ERROR: NULL parameter passed to connect()");
        return false;
    }
    // Connect to WiFi (re-associates from cold every upload; car is only in
    // range while in the pits)
    if (!EnsureWiFiConnected())
    {
        return false;
    }

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
    if (!IsFTPResponseCode(status, "150") && !IsFTPResponseCode(status, "125")) {
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
    if (!IsFTPResponseCode(status, "150") && !IsFTPResponseCode(status, "125")) {
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
    ftpClient.flush();
    String status = ReadFTPResponse();
    if (!IsFTPResponseCode(status, "150") && !IsFTPResponseCode(status, "125")) {
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
    Serial.print(remoteFile);
    Serial.print(" FTP_SERVER ");    
    Serial.print(FTP_SERVER);
    Serial.print(" PORT ");    
    Serial.println(ftpListenerPort);

    if (!FTPConnect(FTP_SERVER, ftpListenerPort/*21*/, FTP_USER, FTP_PASS))
    {
        // FTPConnect owns its own WiFi/FTP retry + timeout; failing here most
        // likely means the car is still out of range. Clean up and let the
        // caller's retry loop try again.
        Serial.println("ERROR: FTP connection failed!");
        sprintf(returnMessage, "ERROR: FTP/WiFi connection failed (out of range?)");
        FTPDisconnect();
        return false;
    }

    if (!remoteFile || !localFilePath)
    {
        Serial.println("ERROR: NULL parameter passed to uploadFileFromStorage()");
        sprintf(returnMessage, "ERROR: NULL parameter passed to uploadFileFromStorage()");
        FTPDisconnect();
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
        sprintf(returnMessage, "ERROR: Failed to open local file");
        FTPDisconnect();
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
        FTPDisconnect();
        return false;
    }

    uint16_t dataPort = 0;
    if (!EnterFTPPassiveMode(dataPort))
    {
        sprintf(returnMessage, "ERROR: PASV command failed");
        localFile.close();
        FTPDisconnect();
        return false;
    }

    if (!dataClient.connect(ftpServer, dataPort))
    {
        Serial.println("ERROR: Data connection failed");
        sprintf(returnMessage, "ERROR: Data connection failed");
        localFile.close();
        FTPDisconnect();
        return false;
    }
    Serial.println("OK: Data connection established, begin upload");

    String storCmd = String("STOR ") + remoteFile;
    ftpClient.println(storCmd);
    ftpClient.flush();
    String status = ReadFTPResponse();
    if (!IsFTPResponseCode(status, "150") && !IsFTPResponseCode(status, "125"))
    {
        Serial.println("ERROR: STOR command failed");
        sprintf(returnMessage, "ERROR: STOR command failed");
        dataClient.stop();
        localFile.close();
        FTPDisconnect();
        return false;
    }

    uint8_t buffer[FTP_CHUNK_SIZE];
    size_t sent = 0;
    size_t bytesRead;
    unsigned long lastProgress = millis();
    unsigned long uploadStart = millis();

    while ((bytesRead = localFile.read(buffer, FTP_CHUNK_SIZE)) > 0)
    {
        if (!WriteAllToFTPClient(dataClient, buffer, bytesRead))
        {
            // Link likely dropped mid-transfer (car rolled out of range).
            Serial.println("ERROR: Failed to write to FTP server");
            sprintf(returnMessage, "ERROR: Failed to write to FTP server");
            localFile.close();
            dataClient.stop();
            FTPDisconnect();
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
        // Pace writes so the ESP32's TCP buffer can't build a huge backlog that
        // stop() would then discard (see FTP_CHUNK_PACING_MS in the header).
        delay(FTP_CHUNK_PACING_MS);
    }

    localFile.close();

    // Guard against a short read/write path silently truncating the file.
    if (sent != fileSize)
    {
        Serial.print("ERROR: only queued ");
        Serial.print(sent);
        Serial.print(" of ");
        Serial.print(fileSize);
        Serial.println(" bytes - aborting to avoid a truncated upload");
        sprintf(returnMessage, "ERROR: incomplete upload (%u/%u bytes)",
                (unsigned)sent, (unsigned)fileSize);
        dataClient.stop();
        FTPDisconnect();
        return false;
    }

    // Throughput of the data phase (excludes the drain/close below) for tuning.
    unsigned long uploadMs = millis() - uploadStart;
    Serial.print("Transfer: ");
    Serial.print(sent);
    Serial.print(" bytes in ");
    Serial.print(uploadMs);
    Serial.print(" ms (");
    Serial.print(uploadMs > 0 ? (sent * 1000UL) / uploadMs : 0);
    Serial.println(" bytes/sec)");

    // flush() is a no-op in this fork, so give the last in-flight bytes time to
    // leave the module before closing; otherwise the server sees EOF early and
    // writes a truncated file while still replying 226.
    delay(FTP_DRAIN_MS);
    dataClient.stop();
    Serial.print("Sent ");
    Serial.print(sent);
    Serial.print(" bytes, expected to send ");
    Serial.print(fileSize);
    Serial.println(" bytes");
    Serial.println("Upload complete, waiting for server confirmation...");

    String finish = ReadFTPResponse();
    FTPDisconnect();
    if (!IsFTPResponseCode(finish, "226"))
    {
        // Server did not confirm the transfer completed - report failure so the
        // caller retries instead of treating a partial upload as success.
        Serial.print("ERROR: ");
        Serial.print(finish);
        Serial.println(" - upload did not complete successfully");
        sprintf(returnMessage, "ERROR: upload did not complete successfully");
        return false;
    }
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
    if (!IsFTPResponseCode(status, "150") && !IsFTPResponseCode(status, "125")) {
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

int FTPClient::GetFTPServerFileList(const char* path, String* outNames, int maxNames, char* returnMessage)
{
    if (!path || !outNames || maxNames <= 0)
    {
        Serial.println("ERROR: bad parameters to GetFTPServerFileList()");
        sprintf(returnMessage, "ERROR: bad list parameters");
        return -1;
    }

    if (!FTPConnect(FTP_SERVER, ftpListenerPort, FTP_USER, FTP_PASS))
    {
        Serial.println("ERROR: FTP connection failed!");
        sprintf(returnMessage, "ERROR: FTP/WiFi connection failed (out of range?)");
        FTPDisconnect();
        return -1;
    }

    uint16_t dataPort = 0;
    if (!EnterFTPPassiveMode(dataPort))
    {
        sprintf(returnMessage, "ERROR: PASV command failed");
        FTPDisconnect();
        return -1;
    }

    if (!dataClient.connect(ftpServer, dataPort))
    {
        Serial.println("ERROR: Data connection failed");
        sprintf(returnMessage, "ERROR: Data connection failed");
        FTPDisconnect();
        return -1;
    }

    // NLST returns bare filenames (one per line), which is far easier to parse
    // into a menu than the ls -l style output of LIST.
    String listCmd = String("NLST ") + path;
    ftpClient.println(listCmd);
    ftpClient.flush();
    String status = ReadFTPResponse();
    if (!IsFTPResponseCode(status, "150") && !IsFTPResponseCode(status, "125"))
    {
        // Some servers answer an empty directory with 226 and no data.
        if (IsFTPResponseCode(status, "226"))
        {
            dataClient.stop();
            FTPDisconnect();
            sprintf(returnMessage, "No files on server");
            return 0;
        }
        Serial.println("ERROR: NLST command failed");
        sprintf(returnMessage, "ERROR: NLST command failed");
        dataClient.stop();
        FTPDisconnect();
        return -1;
    }

    int count = 0;
    String line;
    unsigned long start = millis();
    while (dataClient.connected() && millis() - start < RESPONSE_TIMEOUT)
    {
        while (dataClient.available())
        {
            char c = (char)dataClient.read();
            start = millis();
            if (c == '\n')
            {
                line.trim();
                // Strip any directory prefix in case the server returns paths.
                int slash = line.lastIndexOf('/');
                if (slash >= 0)
                {
                    line = line.substring(slash + 1);
                }
                if (line.length() > 0 && count < maxNames)
                {
                    outNames[count++] = line;
                }
                line = "";
            }
            else if (c != '\r')
            {
                line += c;
            }
        }
    }
    // Handle a final line with no trailing newline.
    line.trim();
    if (line.length() > 0 && count < maxNames)
    {
        int slash = line.lastIndexOf('/');
        if (slash >= 0)
        {
            line = line.substring(slash + 1);
        }
        outNames[count++] = line;
    }

    dataClient.stop();
    String finish = ReadFTPResponse();
    FTPDisconnect();

    if (!IsFTPResponseCode(finish, "226"))
    {
        Serial.println("ERROR: file list did not complete");
        sprintf(returnMessage, "ERROR: file list incomplete");
        return -1;
    }

    Serial.print("Server file list: ");
    Serial.print(count);
    Serial.println(" file(s)");
    sprintf(returnMessage, "%d file(s) on server", count);
    return count;
}

bool FTPClient::GetFileFromFTPServer(const char* remoteFile, const char* localFilePath, char* returnMessage)
{
    if (!remoteFile || !localFilePath)
    {
        Serial.println("ERROR: NULL parameter passed to GetFileFromFTPServer()");
        sprintf(returnMessage, "ERROR: NULL parameter");
        return false;
    }

    Serial.print("DEBUG: GetFileFromFTPServer() remote ");
    Serial.print(remoteFile);
    Serial.print(" -> local ");
    Serial.println(localFilePath);

    if (!FTPConnect(FTP_SERVER, ftpListenerPort, FTP_USER, FTP_PASS))
    {
        Serial.println("ERROR: FTP connection failed!");
        sprintf(returnMessage, "ERROR: FTP/WiFi connection failed (out of range?)");
        FTPDisconnect();
        return false;
    }

    // Set binary mode
    SendFTPCommand("TYPE I");

    // SD FILE_WRITE appends - remove any existing copy so we get a clean file.
    if (SD.exists(localFilePath))
    {
        SD.remove(localFilePath);
    }

    File localFile = SD.open(localFilePath, FILE_WRITE);
    if (!localFile)
    {
        Serial.print("ERROR: Failed to open local file: ");
        Serial.println(localFilePath);
        sprintf(returnMessage, "ERROR: cannot open local file");
        FTPDisconnect();
        return false;
    }

    uint16_t dataPort = 0;
    if (!EnterFTPPassiveMode(dataPort))
    {
        sprintf(returnMessage, "ERROR: PASV command failed");
        localFile.close();
        FTPDisconnect();
        return false;
    }

    if (!dataClient.connect(ftpServer, dataPort))
    {
        Serial.println("ERROR: Data connection failed");
        sprintf(returnMessage, "ERROR: Data connection failed");
        localFile.close();
        FTPDisconnect();
        return false;
    }

    String retrCmd = String("RETR ") + remoteFile;
    ftpClient.println(retrCmd);
    ftpClient.flush();
    String status = ReadFTPResponse();
    if (!IsFTPResponseCode(status, "150") && !IsFTPResponseCode(status, "125"))
    {
        Serial.println("ERROR: RETR command failed");
        sprintf(returnMessage, "ERROR: RETR command failed");
        dataClient.stop();
        localFile.close();
        FTPDisconnect();
        return false;
    }

    uint8_t buffer[FTP_CHUNK_SIZE];
    unsigned long totalBytes = 0;
    unsigned long lastProgress = millis();
    unsigned long downloadStart = millis();
    unsigned long start = millis();
    while (dataClient.connected() && millis() - start < RESPONSE_TIMEOUT)
    {
        int n = dataClient.read(buffer, FTP_CHUNK_SIZE);
        if (n > 0)
        {
            localFile.write(buffer, n);
            totalBytes += n;
            start = millis();

            if (millis() - lastProgress >= 500)
            {
                Serial.print("Download progress: ");
                Serial.print(totalBytes);
                Serial.println(" bytes");
                lastProgress = millis();
            }
        }
        else
        {
            delay(1);
        }
    }

    localFile.close();
    dataClient.stop();

    unsigned long downloadMs = millis() - downloadStart;
    Serial.print("Transfer: ");
    Serial.print(totalBytes);
    Serial.print(" bytes in ");
    Serial.print(downloadMs);
    Serial.print(" ms (");
    Serial.print(downloadMs > 0 ? (totalBytes * 1000UL) / downloadMs : 0);
    Serial.println(" bytes/sec)");

    String finish = ReadFTPResponse();
    FTPDisconnect();
    if (!IsFTPResponseCode(finish, "226"))
    {
        Serial.print("ERROR: ");
        Serial.print(finish);
        Serial.println(" - download did not complete successfully");
        sprintf(returnMessage, "ERROR: download did not complete");
        return false;
    }

    Serial.println("Download successful!");
    sprintf(returnMessage, "Got %lu bytes", totalBytes);
    return true;
}
