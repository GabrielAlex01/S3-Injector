#pragma once

// Wi-Fi Access Point
constexpr const char* AP_SSID     = "S3-Injector";
constexpr const char* AP_PASSWORD = "S3-Payloads@!";
constexpr int         AP_CHANNEL  = 6;
constexpr bool        AP_HIDDEN   = false;
constexpr int         AP_MAX_CONN = 2;

// Rede
constexpr const char* AP_LOCAL_IP = "10.0.0.1";
constexpr const char* AP_GATEWAY  = "10.0.0.1";
constexpr const char* AP_SUBNET   = "255.255.255.0";

// Web Server
constexpr int HTTP_PORT = 80;

// USB Composto (spoofing Microsoft)
constexpr uint16_t    C2_USB_VID     = 0x045E;
constexpr uint16_t    C2_USB_PID     = 0x07B9;
constexpr const char* C2_USB_MFR     = "Microsoft";
constexpr const char* C2_USB_PRODUCT = "USB Input Device";

// Timing
constexpr int KEY_DELAY      = 5;
constexpr int RUN_DIALOG_WAIT = 800;
constexpr int PRE_ENTER_WAIT  = 100;

// Autenticacao do Painel Web
constexpr const char* AUTH_USER = "admin";
constexpr const char* AUTH_PASS = "admin";

// Serial / Loot
constexpr unsigned long SERIAL_BAUD    = 115200;
constexpr unsigned long LOOT_TIMEOUT   = 3000;
constexpr size_t        SERIAL_BUF_CAP = 8192;
