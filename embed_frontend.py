import gzip
import os

FILES = [
    ("index.html", "text/html",               "INDEX_HTML"),
    ("app.js",     "application/javascript",   "APP_JS"),
    ("app.css",    "text/css",                 "APP_CSS"),
]

WWW_DIR = os.path.join("data", "www")
OUT_FILE = os.path.join("src", "web_content.h")

def to_c_array(data, name):
    lines = [f"const uint8_t {name}_GZ[] PROGMEM = {{"]
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    lines.append("};")
    lines.append(f"const size_t {name}_GZ_LEN = sizeof({name}_GZ);")
    return "\n".join(lines)

def main():
    output = [
        "#pragma once",
        "#include <Arduino.h>",
        "",
    ]
    for filename, content_type, var_name in FILES:
        filepath = os.path.join(WWW_DIR, filename)
        if not os.path.exists(filepath):
            print(f"SKIP {filepath} (not found)")
            continue
        with open(filepath, "rb") as f:
            raw = f.read()
        compressed = gzip.compress(raw, compresslevel=9)
        print(f"{filename}: {len(raw)} -> {len(compressed)} bytes (gzip)")
        output.append(f"// {filename} — {len(raw)}B raw, {len(compressed)}B gzip")
        output.append(to_c_array(compressed, var_name))
        output.append("")

    with open(OUT_FILE, "w") as f:
        f.write("\n".join(output))
    print(f"Generated {OUT_FILE}")

if __name__ == "__main__":
    main()
