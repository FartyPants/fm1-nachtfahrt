#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Build an FM-1 update package (.fwsc) from the Felucca app and update loader.

  flash.bin  head [0, 0x4000): flash header (JieLi SDK default values), JLFS
             entries, the SDK SPL (uboot.boot, Apache-2.0) and an isd_config
             blob that decodes to the chip key. The update loader never writes
             the head.
             app area 0x4000..: app_area_head, app.bin, the SDK cfg_tool.bin and
             eq_cfg_hw.bin, the region descriptors; SFC-encrypted with the chip key
  ota.bin    the Felucca update loader (firmware/loader)

The SDK files come from the JieLi AC79 SDK (AC79_SDK, or --sdk).
All integrity checks in the format are CRC16 (poly 0x1021, init 0).

  fm1pkg_make.py APP.bin OTA.bin OUT.fwsc [--product FM-1_9XY] [--sdk DIR]
"""
import argparse
import os
import re
import struct
from pathlib import Path

APP_SLOT = 0x8DFBC
FLASH_SIZE = 0x93000
KEY = 0x980F
# ---- SMK-37 (--board smk37): the layout the stock firmware and smk37_open's sdk/Package.cs use, which
# is not the FM-1's. A 1 MiB part, the user area and BTIF reserved above the image, and no key_blob():
# the SMK's chip key descriptor is jl-misctools' deterministic form.
SMK = dict(
    area=0x4000, app_data=0x120, flash_limit=0xFF000, user_size=0x83000, bt_size=0x1000,
    minimum_vm=0x8000, public_code=0x980F,
    # the stock firmware's own boot resources, from smk37_open sdk/tools/pack (sizes and SHA-256 as its
    # manifest.json lists them); the AC79 SDK has uboot.boot but not this boot-config.bin
    pack=("boot-config.bin", 721), boot=("uboot.boot", 14488), tool=("cfg_tool.bin", 383),
)
# "SMK-37 Elite_8NNNNNN" / "SMK-37 Pro_8NNNNNN", exactly as the stock packager accepts it.
SMK_PRODUCT_RE = re.compile(r"\ASMK-37 (?:Pro|Elite)_8[0-9]{6}\Z")
# flash header: JieLi SDK defaults (burner_size 544, VID "0.01", flash_size 0xFF000,
# fs_ver 0x10, PID "AC791N_STORY"), plain, without its CRC
FLASH_HDR = bytes.fromhex("20022f104cc900f00f001010a4ff4e5d0b41e0cd781dc856417fb367cf9f")
UFW_CHIP = b"AC791N"
SDK = None


def _crc_table():
    t = []
    for i in range(256):
        c = i << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else (c << 1)
        t.append(c & 0xFFFF)
    return t


_T = _crc_table()


def crc16(data, crc=0):
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ _T[((crc >> 8) ^ b) & 0xFF]
    return crc


def enc(buf, off, size, key=0xFFFF):
    """JieLi ENC stream cipher (in place)"""
    for i in range(size):
        buf[off + i] ^= key & 0xFF
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xFFFF


def sfc(buf, off, size, base, key):
    """SFC cipher: ENC per 32-byte block, key mixed with the block address"""
    for i in range(0, size, 32):
        enc(buf, off + i, min(size - i, 32), key ^ ((off + i - base) >> 2))


def chipkey_decode(d):
    """chip key from the 32-byte isd_config blob"""
    thesum = sum(d[:16]) & 0xFF
    thesum = 0xAA if thesum >= 0xE0 else 0x55 if thesum <= 0x10 else thesum
    return sum(1 << i for i in range(16) if (d[16 + i] ^ d[15 - i]) < thesum)


def entry(dcrc, offset, size, flags, resvd, index, name, fill=0xFF):
    if isinstance(name, bytes):                     # 16 raw bytes: name + the packer's trailing fields
        nm = name
    else:
        nm = name.encode() + b"\0"
        nm = nm + bytes([fill]) * (16 - len(nm))
    body = struct.pack("<HIIBBH16s", dcrc, offset & 0xFFFFFFFF, size & 0xFFFFFFFF, flags, resvd, index, nm)
    return struct.pack("<H", crc16(body)) + body


def key_blob(key):
    """32 bytes that chipkey_decode() turns into `key` (+ CRC16)"""
    d = bytearray([0x10] * 16) + bytearray(16)        # sum 0 -> threshold 0x55
    for i in range(16):
        d[16 + i] = d[15 - i] ^ (0x00 if key >> i & 1 else 0xFF)
    assert chipkey_decode(bytes(d)) == key
    return bytes(d) + struct.pack("<H", crc16(d))


def sdk_file(rel, sdk=None):
    root = Path(sdk or SDK or os.environ.get("AC79_SDK", ""))
    path = root / "cpu" / "wl82" / "tools" / rel
    try:
        return path.read_bytes()
    except OSError as e:
        raise SystemExit(f"fm1pkg_make: {path}: {e.strerror}. Set AC79_SDK to a JieLi AC79 SDK checkout.")


def flash_image(app, key):
    spl = sdk_file("uboot.boot")                         # SDK SPL, BANKCB-encoded as in the SDK
    cfg_tool = sdk_file("cfg_tool.bin")
    eq = sdk_file("cfg/eq_cfg_hw.bin")
    isd = key_blob(key) + b"[FELUCCA]\r\n"
    f = bytearray(b"\xFF" * FLASH_SIZE)
    # ---- head
    spl_off = 0xA0
    isd_off = spl_off + len(spl)
    assert isd_off + len(isd) <= 0x4000
    f[spl_off:spl_off + len(spl)] = spl
    f[isd_off:isd_off + len(isd)] = isd
    top = (entry(crc16(spl), spl_off, len(spl), 0x00, 0x00, 0, "uboot.boot") +
           entry(crc16(isd), isd_off, len(isd), 0x02, 0x80, 0, "isd_config.ini") +
           entry(0xFFFF, 0x4000, 0xFFFFFFFF, 0x81, 0xFF, 0, "app_dir_head") +
           entry(0xFFFF, 0xFF000, 0x1000, 0x12, 0x01, 0xFFFF, "key_mac"))
    hdr = struct.pack("<H", crc16(FLASH_HDR)) + FLASH_HDR
    head = bytearray(hdr + top)
    for o in range(0, len(head), 32):                   # header and each JLFS entry: ENC 0xFFFF
        enc(head, o, 32)
    f[0:len(head)] = head
    # ---- app area (plain first, then SFC)
    if len(app) > APP_SLOT:
        raise SystemExit(f"app is {len(app)} B, the slot is {APP_SLOT} B")
    app = app + b"\xFF" * (APP_SLOT - len(app))
    blk = 0x120 + APP_SLOT + len(cfg_tool)              # app_area_head block size
    area = bytearray(b"\xFF" * blk)
    area[0x120:0x120 + APP_SLOT] = app
    area[0x120 + APP_SLOT:blk] = cfg_tool
    ents = [entry(crc16(app), 0x120, APP_SLOT, 0x82, 0xFF, 0, "app.bin"),
            entry(crc16(cfg_tool), 0x120 + APP_SLOT, len(cfg_tool), 0x82, 0xFF, 0, "cfg_tool.bin"),
            # region descriptors; the bytes after the names are SDK packer fields (VM size, "CODE", "AUTO")
            entry(0xFFFF, 0x93000, 352256, 0x12, 0x80, 0, bytes.fromhex("564d00ffffffffffffffffff00800000")),
            entry(0xFFFF, 0, FLASH_SIZE, 0x92, 0x82, 0, bytes.fromhex("5052435400ffffffffffffff434f4445")),
            entry(0xFFFF, 0xE9000, 0x1000, 0x92, 0x80, 0, bytes.fromhex("4254494600ffffffffffffff4155544f")),
            entry(0xFFFF, 0xEA000, 0x12000, 0x92, 0x81, 1, "USR")]
    area[32:32 + len(b"".join(ents))] = b"".join(ents)
    area[0:32] = entry(crc16(area[32:blk]), 0x02000120, blk, 0x83, 0xFF, 0, "app_area_head")
    # nested cfg directory right after the block: SDK default EQ table
    eq_e = entry(crc16(eq), 0x40, len(eq), 0x82, 0xFF, 1, "eq_cfg_hw.bin")
    cfg_body = eq_e + eq
    cfg = entry(crc16(cfg_body), 0x20, 0x20 + len(cfg_body), 0x83, 0xFF, 1, "cfg") + cfg_body
    region = bytearray(area + cfg)
    sfc(region, 0, len(region), 0, key)
    f[0x4000:0x4000 + len(region)] = region
    return bytes(f)


def ufw(flash, ota, product):
    files = [(0, "flash.bin", flash), (100, "ota.bin", ota)]
    off, ents, data = 0x400, [], bytearray()
    for i, (typ, name, d) in enumerate(files):
        off = (off + 0xFF) & ~0xFF
        e = bytearray(0x50)
        struct.pack_into("<HHHHIII", e, 0, typ, i, crc16(d), 0, off, len(d), (len(d) + 31) & ~31)
        e[0x40:0x40 + len(name)] = name.encode()
        ents.append((e, off, d))
        off += len(d)
    total = off
    hdr = bytearray(0x40)
    struct.pack_into("<IH", hdr, 4, total, len(files))
    struct.pack_into("<HHH", hdr, 10, 0x0004, 0x0200, 0)     # header fields as written by the SDK packer
    hdr[16:16 + len(UFW_CHIP)] = UFW_CHIP
    lst = bytearray()
    for e, _, _ in ents:
        x = bytearray(e)
        enc(x, 0, 0x50)
        lst += x
    struct.pack_into("<H", hdr, 2, crc16(lst))
    struct.pack_into("<H", hdr, 0, crc16(hdr[2:0x40]))
    enc(hdr, 0, 0x40)
    logical = bytearray(b"\xFF" * total)
    logical[0:0x40] = hdr
    logical[0x40:0x40 + len(lst)] = lst
    for e, o, d in ents:
        logical[o:o + len(d)] = d
    # .fwsc: one marker byte after each of the first 20 0x2F-byte blocks (the identity)
    if len(product) > 20 or any((ord(c) + i + 1) & 0xFF == 0x7D for i, c in enumerate(product)):
        raise SystemExit(f"product {product!r}: at most 20 characters, none encoding to the unused marker")
    raw = bytearray()
    for i in range(20):
        ch = product[i] if i < len(product) else None
        m = (ord(ch) + i + 1) & 0xFF if ch else 0x7D
        raw += logical[i * 0x2F:(i + 1) * 0x2F] + bytes([m])
    raw += logical[20 * 0x2F:]
    return bytes(raw)


# ---------------------------------------------------------------------------- SMK-37 ---
def smk_pack_file(name, size, pack=None):
    """A boot resource of the stock firmware, checked against its published size."""
    root = Path(pack or os.environ.get("SMK37_PACK", ""))
    path = root / name
    try:
        d = path.read_bytes()
    except OSError as e:
        raise SystemExit(f"fm1pkg_make: {path}: {e.strerror}. Set SMK37_PACK to smk37_open's sdk/tools/pack.")
    if len(d) != size:
        raise SystemExit(f"fm1pkg_make: {path}: {len(d)} B, the stock resource is {size} B")
    return d


def smk_flash_image(app, pack=None):
    """The 1 MiB flash image the SMK-37's updater expects, built as sdk/Package.cs builds it."""
    c = SMK
    boot = smk_pack_file(c["boot"][0], c["boot"][1], pack)
    config = smk_pack_file(c["pack"][0], c["pack"][1], pack)
    tool = smk_pack_file(c["tool"][0], c["tool"][1], pack)
    # jl-misctools' deterministic chip key descriptor: 16x0x10, then 0x10 XOR (key bit ? 0 : 0xFF)
    settings = bytearray(34 + len(config))
    for bit in range(16):
        settings[bit] = 0x10
        settings[16 + bit] = 0x10 ^ (0x00 if (c["public_code"] >> bit) & 1 else 0xFF)
    struct.pack_into("<H", settings, 32, crc16(bytes(settings[0:32])))
    settings[34:] = config
    image_size = (c["area"] + c["app_data"] + len(app) + len(tool) + 64 + 0xFFF) & ~0xFFF
    user_at = c["flash_limit"] - c["user_size"]
    vm_end = user_at - c["bt_size"]
    if image_size > vm_end - c["minimum_vm"]:
        raise SystemExit(f"SMK-37: app {len(app)} B leaves no room for the minimum VM")
    f = bytearray(b"\xFF" * image_size)
    # AC791N boot header, the fields the stock firmware ships
    f[0:32] = bytes.fromhex("000020022f104cc900f00f001010a4ff4e5d0b41e0cd781dc856417fb367cf9f")
    struct.pack_into("<H", f, 0, crc16(bytes(f[2:32])))
    boot_at = 160
    config_at = boot_at + len(boot)

    def rec(at, name, dcrc, address, size, flags, last, extra=None):
        e = bytearray(b"\xFF" * 32)
        struct.pack_into("<HIIHH", e, 2, dcrc, address & 0xFFFFFFFF, size & 0xFFFFFFFF, flags, last)
        nm = name.encode() + b"\0"
        e[16:16 + len(nm)] = nm
        if extra is not None:
            struct.pack_into("<I", e, 28, extra)
        struct.pack_into("<H", e, 0, crc16(bytes(e[2:32])))
        f[at:at + 32] = e

    rec(32, "uboot.boot", crc16(boot), boot_at, len(boot), 0, 0)
    rec(64, "isd_config.ini", crc16(bytes(settings)), config_at, len(settings), 0x8002, 0)
    rec(96, "app_dir_head", 0xFFFF, c["area"], 0xFFFFFFFF, 0xFF81, 0)
    rec(128, "key_mac", 0xFFFF, c["flash_limit"], 4096, 0x0112, 0xFFFF)
    for p in range(0, 160, 32):
        enc(f, p, 32, 0xFFFF)
    f[boot_at:boot_at + len(boot)] = boot
    f[config_at:config_at + len(settings)] = settings
    app_at = c["area"] + c["app_data"]
    f[app_at:app_at + len(app)] = app
    f[app_at + len(app):app_at + len(app) + len(tool)] = tool
    payload = c["app_data"] + len(app) + len(tool)
    rec(c["area"] + 32, "app.bin", crc16(app), c["app_data"], len(app), 0xFF82, 0)
    rec(c["area"] + 64, "cfg_tool.bin", crc16(tool), c["app_data"] + len(app), len(tool), 0xFF82, 0)
    rec(c["area"] + 96, "VM", 0xFFFF, image_size, vm_end - image_size, 0x8112, 0, c["minimum_vm"])
    rec(c["area"] + 128, "PRCT", 0xFFFF, 0, image_size, 0x8292, 0, 0x45444F43)
    rec(c["area"] + 160, "BTIF", 0xFFFF, vm_end, c["bt_size"], 0x8192, 1, 0x4F545541)
    rec(c["area"], "app_area_head", crc16(bytes(f[c["area"] + 32:c["area"] + payload])),
        0x02000120, payload, 0xFF83, 0)
    reserved = c["area"] + payload
    rec(reserved + 32, "USER", 0xFFFF, user_at, c["user_size"], 0x8192, 1, 0x4F545541)
    rec(reserved, "EXT_RESERVED", crc16(bytes(f[reserved + 32:reserved + 64])), 32, 64, 0xFF93, 1)
    # SFC over the app area, one starting key per 32-byte block, mixed with the block's own address
    # relative to the area (Package.cs: Cipher(Area+p, PublicCode^(p>>2))). base = off so sfc() does
    # not mix the address in a second time.
    for p in range(0, payload + 64, 32):
        at = c["area"] + p
        sfc(f, at, min(32, payload + 64 - p), at, c["public_code"] ^ (p >> 2))
    return bytes(f)


def smk_ufw(flash, ota, product):
    """Wrap the flash image and the loader, and lay the identity into the package."""
    if not SMK_PRODUCT_RE.match(product):
        raise SystemExit(f"product {product!r}: the SMK-37 takes 'SMK-37 Elite_8NNNNNN' or 'SMK-37 Pro_8NNNNNN'")
    loader_at = 1024 + ((len(flash) + 31) & ~31)
    total = loader_at + ((len(ota) + 31) & ~31)
    u = bytearray(b"\xFF" * total)
    u[0:224] = b"\0" * 224
    for i, (typ, name, d) in enumerate([(0, "flash.bin", flash), (100, "ota.bin", ota)]):
        r = 64 + i * 80
        at = 1024 if i == 0 else loader_at
        struct.pack_into("<HHHIIII", u, r, typ, i, crc16(d), at, len(d), (len(d) + 31) & ~31, 0)
        u[r + 64:r + 64 + len(name)] = name.encode()
        enc(u, r, 80, 0xFFFF)
    struct.pack_into("<IHH", u, 4, total, 2, 4)
    struct.pack_into("<I", u, 12, 512)
    u[16:16 + len(UFW_CHIP)] = UFW_CHIP
    struct.pack_into("<H", u, 2, crc16(bytes(u[64:224])))
    struct.pack_into("<H", u, 0, crc16(bytes(u[2:64])))
    enc(u, 0, 64, 0xFFFF)
    u[1024:1024 + len(flash)] = flash
    u[loader_at:loader_at + len(ota)] = ota
    out = bytearray()
    for src in range(total):
        out.append(u[src])
        if src < 940 and (src + 1) % 47 == 0:
            index = src // 47
            out.append((ord(product[index]) + index + 1) & 0xFF if index < len(product) else 0x7D)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("app", type=Path)
    ap.add_argument("ota", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--product", default="FM-1_900")
    ap.add_argument("--key", type=lambda s: int(s, 0), default=KEY)
    ap.add_argument("--board", choices=("fm1", "smk37"), default="fm1",
                    help="smk37: the 1 MiB layout and the 'SMK-37 Elite_8NNNNNN' identity")
    ap.add_argument("--sdk", type=Path, help="JieLi AC79 SDK checkout (default: $AC79_SDK)")
    ap.add_argument("--pack", type=Path,
                    help="smk37: the stock firmware's sdk/tools/pack (default: $SMK37_PACK)")
    a = ap.parse_args()
    global SDK
    SDK = a.sdk
    if a.board == "smk37":
        flash = smk_flash_image(a.app.read_bytes(), a.pack)
        pkg = smk_ufw(flash, a.ota.read_bytes(), a.product)
        a.out.write_bytes(pkg)
        print(f"package {a.out}: {len(pkg)} B, identity {a.product}, chip key {SMK['public_code']:#06x},"
              f" image {len(flash)} B of {SMK['flash_limit']} B")
        return
    pkg = ufw(flash_image(a.app.read_bytes(), a.key), a.ota.read_bytes(), a.product)
    a.out.write_bytes(pkg)
    print(f"package {a.out}: {len(pkg)} B, identity {a.product}, chip key {a.key:#06x}")


if __name__ == "__main__":
    main()
