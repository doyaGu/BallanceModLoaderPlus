#!/usr/bin/env python3
"""Codec, layout, and payload helpers for GameTrace-WM v2."""

from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import hmac
import itertools
from pathlib import Path
import re
import struct

import numpy as np


MASK64 = 0xFFFFFFFFFFFFFFFF
BLOCK_SIZE = 32
TILE_COLS = 16
TILE_ROWS = 16
TILE_W = TILE_COLS * BLOCK_SIZE
TILE_H = TILE_ROWS * BLOCK_SIZE
NUM_PILOT_BLOCKS = 16
NUM_MESSAGE_BLOCKS = 240
NUM_TILE_CLASSES = 4
TILE_CLASSES = ("A", "B", "C", "D")
TILE_CLASS_TO_INDEX = {tile_class: index for index, tile_class in enumerate(TILE_CLASSES)}
RS_DATA = 14
RS_CODE = 30
RS_PARITY = 16
DEFAULT_SOFT_BITS = 12
DEFAULT_SOFT_FLIPS = 3

PILOT_COORDS = [
    (0, 0), (15, 0), (0, 15), (15, 15),
    (7, 0), (8, 0), (0, 7), (0, 8),
    (15, 7), (15, 8), (7, 15), (8, 15),
    (5, 5), (10, 5), (5, 10), (10, 10),
]
PILOT_BLOCK_INDICES = [row * TILE_COLS + col for col, row in PILOT_COORDS]
MESSAGE_BLOCK_INDICES = [
    block_index
    for block_index in range(TILE_COLS * TILE_ROWS)
    if block_index not in set(PILOT_BLOCK_INDICES)
]

MESSAGE_SEED_OFFSETS = {
    "A": 0x13579BDF2468ACE0,
    "B": 0x2A3B4C5D6E7F8091,
    "C": 0x55AA33CC77EE11FF,
    "D": 0x89ABCDEF01234567,
}
SYNC_SEED_OFFSETS = {
    "A": 0x0F1E2D3C4B5A6978,
    "B": 0x1122334455667788,
    "C": 0x99AABBCCDDEEFF00,
    "D": 0x7766554433221100,
}

MESSAGE_PERMUTATION_PARAMS = {
    "A": (1, 0),
    "B": (37, 17),
    "C": (53, 91),
    "D": (71, 29),
}
MESSAGE_SIGN_SEEDS = {
    "A": 0xA16B2C3D4E5F6071,
    "B": 0xB27C3D4E5F607182,
    "C": 0xC38D4E5F60718293,
    "D": 0xD49E5F60718293A4,
}


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


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


GF_EXP = [0] * 512
GF_LOG = [0] * 256


def _init_gf_tables() -> None:
    x = 1
    for i in range(255):
        GF_EXP[i] = x
        GF_LOG[x] = i
        x <<= 1
        if x & 0x100:
            x ^= 0x11D
    for i in range(255, 512):
        GF_EXP[i] = GF_EXP[i - 255]


_init_gf_tables()


def gf_mul(a: int, b: int) -> int:
    if a == 0 or b == 0:
        return 0
    return GF_EXP[GF_LOG[a] + GF_LOG[b]]


def gf_pow(base: int, exp: int) -> int:
    if base == 0:
        return 0
    return GF_EXP[(GF_LOG[base] * exp) % 255]


def gf_inv(a: int) -> int:
    if a == 0:
        return 0
    return GF_EXP[255 - GF_LOG[a]]


def rs_build_generator() -> list[int]:
    generator = [0] * (RS_PARITY + 1)
    generator[0] = 1
    generator_len = 1

    for i in range(RS_PARITY):
        root = gf_pow(2, i)
        tmp = [0] * (RS_PARITY + 1)
        for j in range(generator_len):
            tmp[j] ^= gf_mul(generator[j], root)
            tmp[j + 1] ^= generator[j]
        generator_len += 1
        generator[:generator_len] = tmp[:generator_len]

    return generator


RS_GENERATOR = rs_build_generator()


def rs_encode(data: list[int]) -> list[int]:
    message = list(data[:RS_DATA])
    if len(message) != RS_DATA:
        raise ValueError(f"Expected {RS_DATA} data bytes, got {len(message)}")

    out = message + [0] * RS_PARITY
    for i in range(RS_DATA):
        feedback = out[i]
        if feedback == 0:
            continue
        for j in range(1, RS_PARITY + 1):
            out[i + j] ^= gf_mul(RS_GENERATOR[RS_PARITY - j], feedback)

    out[:RS_DATA] = message
    return out


def rs_calc_syndromes(message: list[int]) -> list[int]:
    syndromes = [0] * RS_PARITY
    for i in range(RS_PARITY):
        syndrome = 0
        for j in range(RS_CODE):
            syndrome = gf_mul(syndrome, gf_pow(2, i)) ^ message[j]
        syndromes[i] = syndrome
    return syndromes


def rs_has_errors(syndromes: list[int]) -> bool:
    return any(value != 0 for value in syndromes)


def rs_berlekamp_massey(syndromes: list[int]) -> list[int]:
    n = len(syndromes)
    c_poly = [0] * (n + 1)
    b_poly = [0] * (n + 1)
    c_poly[0] = 1
    b_poly[0] = 1
    length = 0
    shift = 1
    discrepancy_base = 1

    for n_iter in range(n):
        discrepancy = syndromes[n_iter]
        for i in range(1, length + 1):
            discrepancy ^= gf_mul(c_poly[i], syndromes[n_iter - i])
        if discrepancy == 0:
            shift += 1
            continue

        if 2 * length <= n_iter:
            old = c_poly[:]
            coeff = gf_mul(discrepancy, gf_inv(discrepancy_base))
            for i in range(shift, n + 1):
                c_poly[i] ^= gf_mul(coeff, b_poly[i - shift])
            length = n_iter + 1 - length
            b_poly = old
            discrepancy_base = discrepancy
            shift = 1
            continue

        coeff = gf_mul(discrepancy, gf_inv(discrepancy_base))
        for i in range(shift, n + 1):
            c_poly[i] ^= gf_mul(coeff, b_poly[i - shift])
        shift += 1

    return c_poly[: length + 1]


def rs_find_errors(err_loc: list[int]) -> list[int]:
    positions = []
    for i in range(255):
        value = 0
        for j, coeff in enumerate(err_loc):
            value ^= gf_mul(coeff, gf_pow(gf_pow(2, i), j))
        if value == 0:
            pos = (i + RS_CODE - 1) % 255
            if 0 <= pos < RS_CODE:
                positions.append(pos)
    return positions


def rs_solve_error_magnitudes(syndromes: list[int], err_pos: list[int]) -> list[int] | None:
    """Solve error magnitudes via Vandermonde system on syndromes.

    S_i = sum_j e_j * A_j^i where A_j = alpha^(n-1-pos_j) for descending polynomial.
    """
    v = len(err_pos)
    if v == 0:
        return []

    # A_j = alpha^(RS_CODE - 1 - pos_j) for each error position
    a_vals = [gf_pow(2, RS_CODE - 1 - pos) for pos in err_pos]

    # Build v x (v+1) augmented matrix [Vandermonde | syndromes]
    matrix = []
    for i in range(v):
        row = [gf_pow(a_vals[j], i) for j in range(v)]
        row.append(syndromes[i])
        matrix.append(row)

    # Gaussian elimination in GF(2^8)
    for col in range(v):
        # Find pivot
        pivot = -1
        for row in range(col, v):
            if matrix[row][col] != 0:
                pivot = row
                break
        if pivot == -1:
            return None
        if pivot != col:
            matrix[col], matrix[pivot] = matrix[pivot], matrix[col]

        inv_pivot = gf_inv(matrix[col][col])
        for k in range(col, v + 1):
            matrix[col][k] = gf_mul(matrix[col][k], inv_pivot)

        for row in range(v):
            if row == col or matrix[row][col] == 0:
                continue
            factor = matrix[row][col]
            for k in range(col, v + 1):
                matrix[row][k] ^= gf_mul(factor, matrix[col][k])

    return [matrix[j][v] for j in range(v)]


def rs_decode(data: list[int]) -> list[int] | None:
    message = list(data[:RS_CODE])
    if len(message) != RS_CODE:
        return None

    if rs_encode(message[:RS_DATA]) == message:
        return message[:RS_DATA]

    syndromes = rs_calc_syndromes(message)
    if not rs_has_errors(syndromes):
        return message[:RS_DATA]

    err_loc = rs_berlekamp_massey(syndromes)
    err_pos = rs_find_errors(err_loc)
    if len(err_pos) != len(err_loc) - 1:
        return None
    if len(err_pos) > RS_PARITY // 2:
        return None

    magnitudes = rs_solve_error_magnitudes(syndromes, err_pos)
    if magnitudes is None:
        return None
    for pos, mag in zip(err_pos, magnitudes):
        message[pos] ^= mag

    if rs_has_errors(rs_calc_syndromes(message)):
        return None
    return message[:RS_DATA]


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

    version_header = Path(__file__).resolve().parents[1] / "include" / "BML" / "Version.h"
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


def get_v2_tile_class_for_tile(tile_x: int, tile_y: int) -> str:
    if (tile_y & 1) == 0:
        return "B" if (tile_x & 1) else "A"
    return "D" if (tile_x & 1) else "C"


def _build_affine_permutation(multiplier: int, offset: int) -> tuple[int, ...]:
    return tuple((local_index * multiplier + offset) % NUM_MESSAGE_BLOCKS
                 for local_index in range(NUM_MESSAGE_BLOCKS))


V2_MESSAGE_PERMUTATIONS = {
    tile_class: _build_affine_permutation(*MESSAGE_PERMUTATION_PARAMS[tile_class])
    for tile_class in TILE_CLASSES
}
V2_INVERSE_MESSAGE_PERMUTATIONS = {
    tile_class: tuple(np.argsort(np.array(V2_MESSAGE_PERMUTATIONS[tile_class], dtype=np.int32)).tolist())
    for tile_class in TILE_CLASSES
}
V2_MESSAGE_SIGN_MASKS = {
    tile_class: tuple(bool(mix64(MESSAGE_SIGN_SEEDS[tile_class] + canonical_index) & 1)
                      for canonical_index in range(NUM_MESSAGE_BLOCKS))
    for tile_class in TILE_CLASSES
}


def get_v2_message_permutation(tile_class: str) -> tuple[int, ...]:
    return V2_MESSAGE_PERMUTATIONS[tile_class]


def get_v2_inverse_message_permutation(tile_class: str) -> tuple[int, ...]:
    return V2_INVERSE_MESSAGE_PERMUTATIONS[tile_class]


def get_v2_message_sign_mask(tile_class: str) -> tuple[bool, ...]:
    return V2_MESSAGE_SIGN_MASKS[tile_class]


def derive_v2_keys(master_key_hex: str, build_id: int) -> dict[str, bytes | int | dict[str, int]]:
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
            tile_class: message_seed ^ MESSAGE_SEED_OFFSETS[tile_class]
            for tile_class in TILE_CLASSES
        },
        "sync_seeds": {
            tile_class: sync_seed ^ SYNC_SEED_OFFSETS[tile_class]
            for tile_class in TILE_CLASSES
        },
    }


BLOCK_SEED_STRIDE = 0x517CC1B727220A95


_SM_INC = np.uint64(0x9E3779B97F4A7C15)
_SM_MUL1 = np.uint64(0xBF58476D1CE4E5B9)
_SM_MUL2 = np.uint64(0x94D049BB133111EB)
_SM_S1 = np.uint64(30)
_SM_S2 = np.uint64(27)
_SM_S3 = np.uint64(31)
_SM_S63 = np.uint64(63)
_PIXEL_OFFSETS = np.arange(1, BLOCK_SIZE * BLOCK_SIZE + 1, dtype=np.uint64) * _SM_INC


def _finalize_uint64(z: np.ndarray) -> np.ndarray:
    z = (z ^ (z >> _SM_S1)) * _SM_MUL1
    z = (z ^ (z >> _SM_S2)) * _SM_MUL2
    return z ^ (z >> _SM_S3)


def generate_pn_block_np(seed: int, block_index: int) -> np.ndarray:
    state0 = np.uint64((seed ^ ((block_index * BLOCK_SEED_STRIDE) & MASK64)) & MASK64)
    z = _finalize_uint64(state0 + _PIXEL_OFFSETS)
    return np.where(z >> _SM_S63, np.float32(1.0), np.float32(-1.0)).reshape(BLOCK_SIZE, BLOCK_SIZE)


def precompute_templates(seed: int, count: int) -> list[np.ndarray]:
    block_indices = np.arange(count, dtype=np.uint64)
    base_states = np.uint64(seed & MASK64) ^ (block_indices * np.uint64(BLOCK_SEED_STRIDE))
    states = base_states[:, np.newaxis] + _PIXEL_OFFSETS[np.newaxis, :]
    z = _finalize_uint64(states)
    signs = np.where(z >> _SM_S63, np.float32(1.0), np.float32(-1.0))
    return [signs[i].reshape(BLOCK_SIZE, BLOCK_SIZE) for i in range(count)]


def precompute_v2_template_bank(derived: dict[str, bytes | int | dict[str, int]],
                                key: str,
                                count: int) -> dict[str, list[np.ndarray]]:
    seeds = derived[key]
    assert isinstance(seeds, dict)
    return {
        tile_class: precompute_templates(int(seeds[tile_class]), count)
        for tile_class in TILE_CLASSES
    }


def decode_payload(data: list[int]) -> dict | None:
    if len(data) < 14:
        return None

    trace_id = bytes(data[0:6]).hex()
    session_time_raw = struct.unpack(">I", bytes(data[6:10]))[0]
    build_id = struct.unpack(">H", bytes(data[10:12]))[0]
    crc_received = struct.unpack(">H", bytes(data[12:14]))[0]
    crc_computed = crc16_ccitt_false(bytes(data[0:12]))
    if crc_received != crc_computed:
        return None

    return {
        "trace_id": trace_id,
        "trace_source": "device_fallback",
        "session_time": datetime.fromtimestamp(
            session_time_raw,
            tz=timezone.utc,
        ).strftime("%Y-%m-%d %H:%M:%S UTC"),
        "build_id": build_id,
        "build_version": unpack_build_version(build_id),
    }


def try_decode(coded_bytes: list[int]) -> dict | None:
    decoded = rs_decode(coded_bytes)
    if decoded is None:
        return None
    return decode_payload(decoded)


def correlations_to_coded_bytes(correlations: np.ndarray) -> list[int]:
    coded_bytes = [0] * RS_CODE
    for message_index in range(NUM_MESSAGE_BLOCKS):
        byte_index = message_index // 8
        bit_index = 7 - (message_index % 8)
        if correlations[message_index] < 0:
            coded_bytes[byte_index] |= (1 << bit_index)
    return coded_bytes


def flip_message_bit(coded_bytes: list[int], message_index: int) -> None:
    byte_index = message_index // 8
    bit_index = 7 - (message_index % 8)
    coded_bytes[byte_index] ^= (1 << bit_index)


def try_decode_soft(correlations: np.ndarray,
                    reliabilities: np.ndarray | None = None,
                    max_uncertain_bits: int = DEFAULT_SOFT_BITS,
                    max_flip_count: int = DEFAULT_SOFT_FLIPS) -> tuple[dict | None, list[int]]:
    coded_bytes = correlations_to_coded_bytes(correlations)
    result = try_decode(coded_bytes)
    if result is not None:
        return result, coded_bytes

    ranking = np.abs(correlations) if reliabilities is None else reliabilities
    ranked_bits = np.argsort(ranking)[:max_uncertain_bits].tolist()
    for flip_count in range(1, max_flip_count + 1):
        for combo in itertools.combinations(ranked_bits, flip_count):
            candidate_bytes = coded_bytes.copy()
            for message_index in combo:
                flip_message_bit(candidate_bytes, int(message_index))
            result = try_decode(candidate_bytes)
            if result is not None:
                return result, candidate_bytes

    return None, coded_bytes


__all__ = [
    "BLOCK_SEED_STRIDE",
    "BLOCK_SIZE",
    "DEFAULT_SOFT_BITS",
    "DEFAULT_SOFT_FLIPS",
    "MASK64",
    "MESSAGE_BLOCK_INDICES",
    "NUM_MESSAGE_BLOCKS",
    "NUM_PILOT_BLOCKS",
    "PILOT_BLOCK_INDICES",
    "RS_CODE",
    "RS_DATA",
    "RS_PARITY",
    "TILE_CLASSES",
    "TILE_CLASS_TO_INDEX",
    "TILE_COLS",
    "TILE_H",
    "TILE_ROWS",
    "TILE_W",
    "correlations_to_coded_bytes",
    "crc16_ccitt_false",
    "decode_payload",
    "derive_v2_keys",
    "flip_message_bit",
    "generate_pn_block_np",
    "get_v2_inverse_message_permutation",
    "get_v2_message_permutation",
    "get_v2_message_sign_mask",
    "get_v2_tile_class_for_tile",
    "hkdf_sha256",
    "key_bytes_to_seed",
    "normalize_build_ids",
    "pack_build_id",
    "precompute_templates",
    "precompute_v2_template_bank",
    "rs_decode",
    "rs_encode",
    "splitmix64",
    "try_decode",
    "try_decode_soft",
    "unpack_build_version",
]
