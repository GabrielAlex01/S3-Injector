#pragma once
#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

extern USBHIDKeyboard Keyboard;

namespace Ducky {

// Mapeamento ABNT2
inline bool typeCharABNT2(char c) {
    uint8_t key = 0;
    bool shift = false;
    bool dead = false;

    switch (c) {
        case '\'': key = 0x35; break;
        case '"':  key = 0x35; shift = true; break;
        case '~':  key = 0x34; dead = true; break;
        case '^':  key = 0x34; shift = true; dead = true; break;
        case '`':  key = 0x2F; shift = true; dead = true; break;
        case '[':  key = 0x30; break;
        case '{':  key = 0x30; shift = true; break;
        case ']':  key = 0x31; break;
        case '}':  key = 0x31; shift = true; break;
        case '\\': key = 0x64; break;
        case '|':  key = 0x64; shift = true; break;
        case '/':  key = 0x87; break;
        case '?':  key = 0x87; shift = true; break;
        case ';':  key = 0x38; break;
        case ':':  key = 0x38; shift = true; break;
        default: return false;
    }

    if (shift) Keyboard.pressRaw(HID_KEY_SHIFT_LEFT);
    Keyboard.pressRaw(key);
    delay(8);
    Keyboard.releaseAll();
    delay(8);

    if (dead) {
        Keyboard.pressRaw(HID_KEY_SPACE);
        delay(8);
        Keyboard.releaseAll();
        delay(8);
    }

    return true;
}

// Digitar string com ABNT2
inline void typeString(const String& text) {
    for (unsigned int i = 0; i < text.length(); i++) {
        if (!typeCharABNT2(text[i])) {
            Keyboard.press(text[i]);
            delay(8);
            Keyboard.release(text[i]);
            delay(8);
        }
    }
}

// Lookup de teclas especiais
inline uint8_t specialKey(const String& name) {
    if (name == "ENTER" || name == "RETURN")       return HID_KEY_ENTER;
    if (name == "ESC" || name == "ESCAPE")         return HID_KEY_ESCAPE;
    if (name == "BACKSPACE")                       return HID_KEY_BACKSPACE;
    if (name == "TAB")                             return HID_KEY_TAB;
    if (name == "SPACE")                           return HID_KEY_SPACE;
    if (name == "DELETE")                          return HID_KEY_DELETE;
    if (name == "DOWNARROW" || name == "DOWN")     return HID_KEY_ARROW_DOWN;
    if (name == "UPARROW" || name == "UP")         return HID_KEY_ARROW_UP;
    if (name == "LEFTARROW" || name == "LEFT")     return HID_KEY_ARROW_LEFT;
    if (name == "RIGHTARROW" || name == "RIGHT")   return HID_KEY_ARROW_RIGHT;
    if (name == "CAPSLOCK")                        return HID_KEY_CAPS_LOCK;
    if (name == "PRINTSCREEN")                     return HID_KEY_PRINT_SCREEN;
    if (name == "HOME")                            return HID_KEY_HOME;
    if (name == "END")                             return HID_KEY_END;
    if (name == "PAGEUP")                          return HID_KEY_PAGE_UP;
    if (name == "PAGEDOWN")                        return HID_KEY_PAGE_DOWN;
    if (name == "INSERT")                          return HID_KEY_INSERT;
    if (name.startsWith("F") && name.length() <= 3) {
        int n = name.substring(1).toInt();
        if (n >= 1 && n <= 12) return HID_KEY_F1 + (n - 1);
    }
    return 0;
}

inline void pressKeyArg(const String& arg) {
    if (arg.length() == 1) {
        Keyboard.press(arg[0]);
    } else {
        uint8_t k = specialKey(arg);
        if (k) Keyboard.pressRaw(k);
    }
}

inline void singleKey(uint8_t k) {
    Keyboard.pressRaw(k);
    delay(50);
    Keyboard.releaseAll();
}

inline void modCombo(uint8_t mod, const String& arg) {
    Keyboard.pressRaw(mod);
    pressKeyArg(arg);
    delay(50);
    Keyboard.releaseAll();
}

inline void modCombo2(uint8_t m1, uint8_t m2, const String& arg) {
    Keyboard.pressRaw(m1);
    Keyboard.pressRaw(m2);
    pressKeyArg(arg);
    delay(50);
    Keyboard.releaseAll();
}

// Executar uma linha DuckyScript
inline void executeLine(const String& rawLine, String& lastCmd) {
    String line = rawLine;
    line.trim();
    if (line.length() == 0 || line.startsWith("REM")) return;

    if (line.startsWith("DELAY ")) {
        delay(line.substring(6).toInt());
    }
    else if (line.startsWith("STRING ")) {
        typeString(line.substring(7));
    }
    else if (line.startsWith("REPEAT ")) {
        int n = line.substring(7).toInt();
        if (lastCmd.length() > 0 && !lastCmd.startsWith("REPEAT")) {
            String frozen = lastCmd;
            for (int i = 0; i < n; i++) {
                executeLine(frozen, lastCmd);
            }
        }
        return;
    }
    else if (line.startsWith("GUI ") || line.startsWith("WINDOWS ")) {
        String arg = line.substring(line.indexOf(' ') + 1);
        arg.trim();
        modCombo(HID_KEY_GUI_LEFT, arg);
    }
    else if (line.startsWith("CTRL-ALT ")) {
        modCombo2(HID_KEY_CONTROL_LEFT, HID_KEY_ALT_LEFT, line.substring(9));
    }
    else if (line.startsWith("CTRL-SHIFT ")) {
        modCombo2(HID_KEY_CONTROL_LEFT, HID_KEY_SHIFT_LEFT, line.substring(11));
    }
    else if (line.startsWith("CTRL ")) {
        modCombo(HID_KEY_CONTROL_LEFT, line.substring(5));
    }
    else if (line.startsWith("ALT ")) {
        modCombo(HID_KEY_ALT_LEFT, line.substring(4));
    }
    else if (line.startsWith("SHIFT ")) {
        modCombo(HID_KEY_SHIFT_LEFT, line.substring(6));
    }
    else {
        uint8_t k = specialKey(line);
        if (k) singleKey(k);
    }

    lastCmd = line;
}

// Executar script DuckyScript completo
inline void execute(const String& script) {
    String lastCmd;
    int start = 0;
    int len = script.length();
    Serial.printf("[*] DuckyScript: %d bytes\n", len);
    while (start < len) {
        int end = script.indexOf('\n', start);
        if (end == -1) end = len;
        String line = script.substring(start, end);
        executeLine(line, lastCmd);
        start = end + 1;
        yield();
    }
    Serial.println("[+] DuckyScript: done");
}

} // namespace Ducky
