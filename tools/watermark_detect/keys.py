#!/usr/bin/env python3
"""Key derivation and cryptographic utilities for GameTrace-WM v2."""

from __future__ import annotations

import hashlib
import hmac
from pathlib import Path
import re
import struct


MASK64 = 0xFFFFFFFFFFFFFFFF
DEFAULT_SOFT_BITS = 12
DEFAULT_SOFT_FLIPS = 3


def splitmix64(state: int) -> tuple[int, int]:
    state = (state + 0x9E3779B97F4A7C15) & MASK64
    z = state
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK64
    z = z ^ (z >> 31)
    return state, z


def mix64(value: int) -> int:
    value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & MASK64
    return value ^ (value >> 31)


def pack_build_id(major: int, minor: int, patch: int) -> int:
    return ((major & 0x1F) << 11) | ((minor & 0x1F) << 6) | (patch & 0x3F)


def unpack_build_version(build_id: int) -> str:
    major = (build_id >> 11) & 0x1F
    minor = (build_id >> 6) & 0x1F
    patch = build_id & 0x3F
    return f"{major}.{minor}.{patch}"


def normalize_build_ids(build_ids: list[int | str] | None) -> list[int]:
    if build_ids:
        normalized = []
        for build_id in build_ids:
            if isinstance(build_id, int):
                normalized.append(build_id & 0xFFFF)
                continue
            text = build_id.strip().lower()
            normalized.append(int(text, 16) if text.startswith("0x") else int(text))
        return normalized

    version_header = Path(__file__).resolve().parents[2] / "include" / "BML" / "Version.h"
    if version_header.is_file():
        contents = version_header.read_text(encoding="utf-8", errors="ignore")
        major_match = re.search(r"#define\s+BML_MAJOR_VERSION\s+(\d+)", contents)
        minor_match = re.search(r"#define\s+BML_MINOR_VERSION\s+(\d+)", contents)
        patch_match = re.search(r"#define\s+BML_PATCH_VERSION\s+(\d+)", contents)
        if major_match and minor_match and patch_match:
            return [pack_build_id(
                int(major_match.group(1)),
                int(minor_match.group(1)),
                int(patch_match.group(1)),
            )]

    return [pack_build_id(0, 3, 11)]


def hkdf_sha256(ikm: bytes, info: bytes, length: int, salt: bytes = b"") -> bytes:
    if not salt:
        salt = b"\x00" * hashlib.sha256().digest_size
    prk = hmac.new(salt, ikm, hashlib.sha256).digest()

    output = b""
    previous = b""
    counter = 1
    while len(output) < length:
        previous = hmac.new(prk, previous + info + bytes([counter]), hashlib.sha256).digest()
        output += previous
        counter += 1
    return output[:length]


def key_bytes_to_seed(key_bytes: bytes) -> int:
    if len(key_bytes) < 16:
        raise ValueError("Expected at least 16 key bytes")
    lo = int.from_bytes(key_bytes[:8], "little")
    hi = int.from_bytes(key_bytes[8:16], "little")
    return lo ^ hi


def derive_v2_keys(master_key_hex: str, build_id: int,
                   tile_classes: tuple[str, ...],
                   message_seed_offsets: dict[str, int],
                   sync_seed_offsets: dict[str, int]) -> dict[str, bytes | int | dict[str, int]]:
    master_key = bytes.fromhex(master_key_hex)
    if len(master_key) != 16:
        raise ValueError("Expected 128-bit master key")

    build_bytes = struct.pack(">H", build_id & 0xFFFF)
    message_key = hkdf_sha256(master_key, b"wm-msg" + build_bytes, 16)
    sync_key = hkdf_sha256(master_key, b"wm-sync" + build_bytes, 16)
    message_seed = key_bytes_to_seed(message_key)
    sync_seed = key_bytes_to_seed(sync_key)
    return {
        "message_key": message_key,
        "sync_key": sync_key,
        "message_seed": message_seed,
        "sync_seed": sync_seed,
        "message_seeds": {
            tile_class: message_seed ^ message_seed_offsets[tile_class]
            for tile_class in tile_classes
        },
        "sync_seeds": {
            tile_class: sync_seed ^ sync_seed_offsets[tile_class]
            for tile_class in tile_classes
        },
    }


__all__ = [
    "DEFAULT_SOFT_BITS",
    "DEFAULT_SOFT_FLIPS",
    "MASK64",
    "derive_v2_keys",
    "hkdf_sha256",
    "key_bytes_to_seed",
    "mix64",
    "normalize_build_ids",
    "pack_build_id",
    "splitmix64",
    "unpack_build_version",
]
