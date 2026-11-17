#!/usr/bin/env python3
"""Diagnostico APK para Nova: sin red, sin ejecutar APK ni exponer claves.

Usa solo biblioteca estandar. No escribe service.json ni device_state.json.
"""
import argparse
import json
import re
import struct
import zipfile

def u16(b, p):
    return struct.unpack_from("<H", b, p)[0]

def u32(b, p):
    return struct.unpack_from("<I", b, p)[0]

def read_length(b, p, utf8):
    if utf8:
        v = b[p]; p += 1
        if v & 128:
            v = ((v & 127) << 8) | b[p]; p += 1
    else:
        v = u16(b, p); p += 2
        if v & 32768:
            v = ((v & 32767) << 16) | u16(b, p); p += 2
    return v, p

def strings(b, pos):
    count, flags, start = u32(b, pos + 8), u32(b, pos + 16), u32(b, pos + 20)
    if u16(b, pos) != 1 or count > 2000000:
        raise ValueError("Tabla de recursos incorrecta")
    hdr = u16(b, pos + 2)
    result = []
    for i in range(count):
        p = pos + start + u32(b, pos + hdr + 4*i)
        utf8 = bool(flags & 256)
        if utf8:
            _, p = read_length(b, p, True)
            n, p = read_length(b, p, True)
            result.append(b[p:p+n].decode("utf-8", "replace"))
        else:
            n, p = read_length(b, p, False)
            result.append(b[p:p+2*n].decode("utf-16le", "replace"))
    return result

def manifest(b):
    if u16(b, 0) != 3:
        raise ValueError("AndroidManifest.xml no es XML binario")
    pos, pool, result = 8, [], {"portal_key_presente": False}
    while pos + 8 <= len(b):
        kind, size = u16(b, pos), u32(b, pos + 4)
        if size < 8 or pos + size > len(b):
            raise ValueError("Manifest corrupto")
        if kind == 1:
            pool = strings(b, pos)
        if kind == 0x102 and pool:
            ix = u32(b, pos + 20)
            tag = pool[ix] if ix < len(pool) else ""
            if tag in ("manifest", "meta-data"):
                attrs = {}
                for i in range(u16(b, pos + 28)):
                    p = pos + 16 + u16(b, pos + 24) + u16(b, pos + 26)*i
                    key, raw = u32(b, p + 4), u32(b, p + 8)
                    if key >= len(pool):
                        continue
                    if raw < len(pool):
                        value = pool[raw]
                    elif b[p + 15] == 3 and u32(b, p + 16) < len(pool):
                        value = pool[u32(b, p + 16)]
                    elif b[p + 15] in (0x10, 0x11):
                        value = str(u32(b, p + 16))
                    else:
                        value = None
                    attrs[pool[key]] = value
                if tag == "manifest":
                    result.update({key: attrs.get(key) for key in
                                   ("package", "versionCode", "versionName")})
                if tag == "meta-data" and attrs.get("name") == "PORTAL_KEY":
                    result["portal_key_presente"] = bool(attrs.get("value"))
        pos += size
    return result

def portal_resources(b):
    """Identifica recursos sin leer su contenido (ni exponer servidores)."""
    globals_pool = strings(b, u16(b, 2))
    found = set()
    pos = u16(b, 2) + u32(b, u16(b, 2) + 4)
    while pos + 8 <= len(b):
        typ, hd, size = u16(b, pos), u16(b, pos + 2), u32(b, pos + 4)
        if size < hd or pos + size > len(b):
            raise ValueError("resources.arsc corrupto")
        if typ == 0x200:
            types = strings(b, pos + u32(b, pos + 268))
            keys = strings(b, pos + u32(b, pos + 276))
            p = pos + hd
            while p + 8 <= pos + size:
                item_type, item_hdr, item_size = u16(b, p), u16(b, p + 2), u32(b, p + 4)
                if item_size < item_hdr or p + item_size > pos + size:
                    raise ValueError("Entrada de recursos incorrecta")
                if item_type == 0x201:
                    type_id, count, start = b[p + 8], u32(b, p + 12), u32(b, p + 16)
                    if 0 < type_id <= len(types) and types[type_id-1] == "string":
                        for i in range(count):
                            offset = u32(b, p + item_hdr + 4*i)
                            if offset == 0xffffffff:
                                continue
                            at = p + start + offset
                            idx = u32(b, at + 4)
                            if idx >= len(keys) or keys[idx] not in ("portal_main", "portal_backup"):
                                continue
                            if not u16(b, at + 2) & 1 and b[at + 11] == 3:
                                if u32(b, at + 12) < len(globals_pool):
                                    found.add(keys[idx])
                p += item_size
        pos += size
    return {key: key in found for key in ("portal_main", "portal_backup")}

def leb(b, pos):
    value = 0
    for shift in range(0, 35, 7):
        x = b[pos]; pos += 1
        value |= (x & 127) << shift
        if x < 128:
            return value, pos
    raise ValueError("ULEB128 incorrecto")

def dex_constants(b):
    """Lee exclusivamente las constantes de dos metodos conocidos."""
    if not b.startswith(b"dex\n"):
        raise ValueError("DEX incorrecto")
    pool = []
    for i in range(u32(b, 0x38)):
        p = u32(b, u32(b, 0x3c) + 4*i)
        _, p = leb(b, p)
        end = b.index(0, p)
        pool.append(b[p:end].decode("utf-8", "replace"))
    types = [pool[u32(b, u32(b, 0x44) + 4*i)] for i in range(u32(b, 0x40))]
    methods = [(types[u16(b, u32(b, 0x5c) + 8*i)],
                pool[u32(b, u32(b, 0x5c) + 8*i + 4)]) for i in range(u32(b, 0x58))]
    wanted = {"Lk2/b;": {"g"}, "Lo7/a;": {"b", "i"}}
    for i in range(u32(b, 0x60)):
        off = u32(b, 0x64) + 32*i
        cls_index, data = u32(b, off), u32(b, off + 24)
        if not data or types[cls_index] not in wanted:
            continue
        cls = types[cls_index]
        p = data
        sf, p = leb(b, p); inf, p = leb(b, p)
        direct, p = leb(b, p); virtual, p = leb(b, p)
        for _ in range(sf + inf):
            _, p = leb(b, p); _, p = leb(b, p)
        for count in (direct, virtual):
            idx = 0
            for _ in range(count):
                delta, p = leb(b, p); _, p = leb(b, p)
                codeoff, p = leb(b, p); idx += delta
                if not codeoff or idx >= len(methods) or methods[idx][1] not in wanted[cls]:
                    continue
                code = b[codeoff + 16:codeoff + 16 + u32(b, codeoff + 12)*2]
                literals, ints = [], []
                for k in range(0, len(code) - 3, 2):
                    if code[k] == 0x1a:
                        sidx = u16(code, k + 2)
                        if sidx < len(pool): literals.append(pool[sidx])
                    if code[k] == 0x14 and k + 6 <= len(code):
                        number = struct.unpack_from("<i", code, k + 2)[0]
                        if 10000 <= number <= 99999999: ints.append(number)
                yield cls, methods[idx][1], literals, ints

def inspect(path):
    with zipfile.ZipFile(path) as apk:
        result = manifest(apk.read("AndroidManifest.xml"))
        result["recursos_portal"] = portal_resources(apk.read("resources.arsc"))
        result["extractor_anterior"] = {"metodo": "Lk2/b;.g", "compatible": False}
        candidates = set()
        for name in apk.namelist():
            if not re.fullmatch(r"classes\d*\.dex", name):
                continue
            for cls, method, literals, ints in dex_constants(apk.read(name)):
                if (cls, method) == ("Lk2/b;", "g"):
                    if any(re.fullmatch(r"[0-9a-fA-F]{8}-[0-9a-fA-F-]{27,}", x) for x in literals):
                        kind = "UUID"
                    elif any(re.fullmatch(r"[A-Za-z0-9+/]{32}={0,2}", x) for x in literals):
                        kind = "posible base64"
                    else:
                        kind = "sin constante reconocible"
                    result["extractor_anterior"]["tipo_encontrado"] = kind
                    result["extractor_anterior"]["compatible"] = kind == "posible base64"
                if cls == "Lo7/a;":
                    candidates.update(ints)
                    for x in literals:
                        if re.fullmatch(r"\d+\.\d+\.\d+", x):
                            result["version_interna_textual"] = x
        if candidates: result["versiones_internas_posibles"] = sorted(candidates)
        result["nota"] = ("Las constantes no demuestran lo enviado al servidor. "
                           "No se extrajeron secretos ni se modifico la configuracion.")
        return result

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk")
    args = parser.parse_args()
    print(json.dumps(inspect(args.apk), indent=2, ensure_ascii=False))
