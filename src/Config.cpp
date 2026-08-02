#include "Config.h"
#include "WiFiSecrets.h"
#include <SD.h>

// Effective settings, seeded from the compiled defaults. A /config.ini on the SD
// card overrides these per-field at boot (see LoadConfigFromSD).
char SSID[CFG_STR_LEN]       = DEFAULT_SSID;
char PASSWORD[CFG_STR_LEN]   = DEFAULT_PASSWORD;
char FTP_SERVER[CFG_STR_LEN] = DEFAULT_FTP_SERVER;
char FTP_USER[CFG_STR_LEN]   = DEFAULT_FTP_USER;
char FTP_PASS[CFG_STR_LEN]   = DEFAULT_FTP_PASS;
char FTP_PATH[CFG_STR_LEN]   = DEFAULT_FTP_PATH;
int  FTP_PORT                = DEFAULT_FTP_PORT;

char driverNames[CFG_MAX_DRIVERS][CFG_DRIVER_LEN];
int  driverCount = 0;
char currentDriver[CFG_DRIVER_LEN] = "";

static void copyVal(char* dest, size_t destLen, const String& val)
{
    strncpy(dest, val.c_str(), destLen - 1);
    dest[destLen - 1] = '\0';
}

// Apply one trimmed, non-comment line. 'section' is the current [section] name
// (lowercased); it is updated in place when the line is a section header.
static void ApplyConfigLine(const String& line, char* section, size_t sectionLen)
{
    if (line.length() == 0 || line[0] == '#' || line[0] == ';')
    {
        return;
    }

    // Section header, e.g. [ftp]
    if (line[0] == '[' && line.endsWith("]"))
    {
        String s = line.substring(1, line.length() - 1);
        s.trim();
        s.toLowerCase();
        strncpy(section, s.c_str(), sectionLen - 1);
        section[sectionLen - 1] = '\0';
        return;
    }

    // In [drivers], each line is a bare driver name.
    if (strcmp(section, "drivers") == 0)
    {
        if (driverCount < CFG_MAX_DRIVERS)
        {
            strncpy(driverNames[driverCount], line.c_str(), CFG_DRIVER_LEN - 1);
            driverNames[driverCount][CFG_DRIVER_LEN - 1] = '\0';
            driverCount++;
        }
        else
        {
            Serial.println("Config: driver list full, ignoring extra entries");
        }
        return;
    }

    // Everything else is key=value.
    int eq = line.indexOf('=');
    if (eq <= 0)
    {
        return;
    }
    String key = line.substring(0, eq);
    String val = line.substring(eq + 1);
    key.trim();
    key.toLowerCase();
    val.trim();

    if (strcmp(section, "wifi") == 0)
    {
        if (key == "ssid")          copyVal(SSID, CFG_STR_LEN, val);
        else if (key == "password") copyVal(PASSWORD, CFG_STR_LEN, val);
    }
    else if (strcmp(section, "ftp") == 0)
    {
        if (key == "server")        copyVal(FTP_SERVER, CFG_STR_LEN, val);
        else if (key == "user")     copyVal(FTP_USER, CFG_STR_LEN, val);
        else if (key == "pass")     copyVal(FTP_PASS, CFG_STR_LEN, val);
        else if (key == "path")     copyVal(FTP_PATH, CFG_STR_LEN, val);
        else if (key == "port")
        {
            int p = val.toInt();
            if (p > 0 && p <= 65535)
            {
                FTP_PORT = p;
            }
        }
    }
}

bool LoadConfigFromSD(const char* path)
{
    driverCount = 0;

    if (!path || !SD.exists(path))
    {
        Serial.print("Config: no ");
        Serial.print(path ? path : "(null)");
        Serial.println(" - using compiled defaults");
        return false;
    }

    File f = SD.open(path, FILE_READ);
    if (!f)
    {
        Serial.print("Config: could not open ");
        Serial.println(path);
        return false;
    }

    char section[16] = "";
    String line;
    bool eof = false;
    while (!eof)
    {
        int c = f.read();
        if (c < 0)
        {
            eof = true;   // process the final (possibly newline-less) line, then stop
        }
        if (c == '\n' || eof)
        {
            line.trim();
            ApplyConfigLine(line, section, sizeof(section));
            line = "";
        }
        else if (c != '\r')
        {
            line += (char)c;
        }
    }
    f.close();

    Serial.print("Config: loaded ");
    Serial.print(path);
    Serial.print(" (SSID=");
    Serial.print(SSID);
    Serial.print(", FTP=");
    Serial.print(FTP_SERVER);
    Serial.print(":");
    Serial.print(FTP_PORT);
    Serial.print(", ");
    Serial.print(driverCount);
    Serial.println(" driver(s))");
    return true;
}

// Read config.ini into lines[], replace (or insert) section/key = value while
// preserving every other line, and write it back. Comment/blank lines are kept.
static bool SetConfigValue(const char* path, const char* wantSection, const char* wantKey, const char* value)
{
    static const int MAX_LINES = 128;
    String lines[MAX_LINES];
    int nLines = 0;
    bool truncatedRead = false;

    if (path && SD.exists(path))
    {
        File f = SD.open(path, FILE_READ);
        if (f)
        {
            String cur;
            bool eof = false;
            while (!eof)
            {
                int c = f.read();
                if (c < 0) eof = true;
                if (c == '\n' || eof)
                {
                    // drop the empty fragment after a trailing newline at EOF
                    if (!(eof && cur.length() == 0))
                    {
                        if (nLines < MAX_LINES) lines[nLines++] = cur;
                        else truncatedRead = true;
                    }
                    cur = "";
                }
                else if (c != '\r')
                {
                    cur += (char)c;
                }
            }
            f.close();
        }
    }

    // Refuse to rewrite a file we could not fully read - avoids truncating it.
    if (truncatedRead)
    {
        Serial.println("Config: file too large to update safely, not saved");
        return false;
    }

    String wantSec = String(wantSection); wantSec.toLowerCase();
    String wantK = String(wantKey); wantK.toLowerCase();

    // Find the target section span [secStart, secEnd).
    int secStart = -1;
    int secEnd = nLines;
    for (int i = 0; i < nLines; i++)
    {
        String t = lines[i]; t.trim();
        if (t.length() >= 2 && t[0] == '[' && t.endsWith("]"))
        {
            String s = t.substring(1, t.length() - 1); s.trim(); s.toLowerCase();
            if (secStart < 0 && s == wantSec)
            {
                secStart = i;
            }
            else if (secStart >= 0)
            {
                secEnd = i;
                break;
            }
        }
    }

    String newLine = String(wantKey) + "=" + value;

    if (secStart < 0)
    {
        // section absent: append "[section]" then the key
        if (nLines < MAX_LINES) lines[nLines++] = String("[") + wantSection + "]";
        if (nLines < MAX_LINES) lines[nLines++] = newLine;
    }
    else
    {
        int keyLine = -1;
        for (int i = secStart + 1; i < secEnd; i++)
        {
            String t = lines[i]; t.trim();
            if (t.length() == 0 || t[0] == '#' || t[0] == ';') continue;
            int eq = t.indexOf('=');
            if (eq > 0)
            {
                String k = t.substring(0, eq); k.trim(); k.toLowerCase();
                if (k == wantK) { keyLine = i; break; }
            }
        }
        if (keyLine >= 0)
        {
            lines[keyLine] = newLine;  // replace in place
        }
        else if (nLines < MAX_LINES)
        {
            // insert right after the section header
            for (int i = nLines; i > secStart + 1; i--) lines[i] = lines[i - 1];
            lines[secStart + 1] = newLine;
            nLines++;
        }
    }

    if (SD.exists(path)) SD.remove(path);
    File f = SD.open(path, FILE_WRITE);
    if (!f)
    {
        Serial.print("Config: could not write ");
        Serial.println(path);
        return false;
    }
    for (int i = 0; i < nLines; i++)
    {
        f.print(lines[i]);
        f.print("\n");
    }
    f.close();

    Serial.print("Config: saved ");
    Serial.print(wantSection); Serial.print("/"); Serial.print(wantKey);
    Serial.print(" = "); Serial.println(value);
    return true;
}

bool SaveFtpPortToConfig(const char* path, int port)
{
    FTP_PORT = port;
    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%d", port);
    return SetConfigValue(path, "ftp", "port", portStr);
}
