"""Evidence aggregation and decoding for watermark detection."""

from __future__ import annotations
from datetime import datetime, timezone
import itertools
import struct
import numpy as np
from .rs import rs_decode, crc16_ccitt_false, RS_CODE, RS_DATA
from .layout import (
    TILE_CLASSES, TILE_CLASS_TO_INDEX, TILE_COLS, TILE_ROWS, BLOCK_SIZE,
    NUM_MESSAGE_BLOCKS, MESSAGE_BLOCK_INDICES,
    get_v2_tile_class_for_tile, get_v2_message_permutation, get_v2_message_sign_mask,
)
from .keys import DEFAULT_SOFT_BITS, DEFAULT_SOFT_FLIPS, unpack_build_version

VISIBLE_BLOCK_THRESHOLD = 0.25


def _block_correlation(blocks: np.ndarray, coverage: np.ndarray, row: int, col: int,
                       template: np.ndarray) -> tuple[float, float]:
    block_coverage = float(coverage[row, col])
    if block_coverage < VISIBLE_BLOCK_THRESHOLD:
        return 0.0, 0.0
    numerator = float(np.tensordot(blocks[row, col], template, axes=([0, 1], [0, 1])))
    valid_pixels = max(block_coverage * float(BLOCK_SIZE * BLOCK_SIZE), 1.0)
    return numerator / valid_pixels, block_coverage


def aggregate_v2_message_evidence(
    candidate: object,
    message_bank: dict[str, list[np.ndarray]],
) -> dict[str, np.ndarray | float]:
    blocks, coverage = candidate.blocks
    block_rows, block_cols = coverage.shape
    tile_rows, tile_cols, _ = candidate.tile_class_scores.shape
    tile_origin_x, tile_origin_y = candidate.tile_origin

    samples: list[list[float]] = [[] for _ in range(NUM_MESSAGE_BLOCKS)]
    sample_weights: list[list[float]] = [[] for _ in range(NUM_MESSAGE_BLOCKS)]

    for tile_row in range(tile_rows):
        for tile_col in range(tile_cols):
            tile_class = get_v2_tile_class_for_tile(
                tile_col + candidate.tile_phase[0],
                tile_row + candidate.tile_phase[1],
            )
            class_index = TILE_CLASS_TO_INDEX[tile_class]
            tile_weight = float(candidate.tile_visible_weights[tile_row, tile_col, class_index])
            if tile_weight <= 0.0:
                continue

            base_row = tile_origin_y + tile_row * TILE_ROWS
            base_col = tile_origin_x + tile_col * TILE_COLS
            permutation = get_v2_message_permutation(tile_class)
            sign_mask = get_v2_message_sign_mask(tile_class)
            templates = message_bank[tile_class]

            for local_index, block_index in enumerate(MESSAGE_BLOCK_INDICES):
                block_row = base_row + (block_index // TILE_COLS)
                block_col = base_col + (block_index % TILE_COLS)
                if block_row >= block_rows or block_col >= block_cols:
                    continue

                correlation, block_coverage = _block_correlation(
                    blocks,
                    coverage,
                    block_row,
                    block_col,
                    templates[local_index],
                )
                if block_coverage <= 0.0:
                    continue

                canonical_index = permutation[local_index]
                canonical_correlation = -correlation if sign_mask[canonical_index] else correlation
                samples[canonical_index].append(canonical_correlation)
                sample_weights[canonical_index].append(block_coverage * tile_weight)

    aggregated = np.zeros(NUM_MESSAGE_BLOCKS, dtype=np.float32)
    reliability = np.zeros(NUM_MESSAGE_BLOCKS, dtype=np.float32)
    coverage_weight = np.zeros(NUM_MESSAGE_BLOCKS, dtype=np.float32)
    counts = np.zeros(NUM_MESSAGE_BLOCKS, dtype=np.float32)

    for canonical_index in range(NUM_MESSAGE_BLOCKS):
        if not samples[canonical_index]:
            continue
        values = np.array(samples[canonical_index], dtype=np.float32)
        weights = np.array(sample_weights[canonical_index], dtype=np.float32)
        aggregated[canonical_index] = float(np.median(values))
        coverage_weight[canonical_index] = float(np.mean(weights))
        reliability[canonical_index] = float(np.median(np.abs(values))) * max(
            coverage_weight[canonical_index],
            1.0,
        )
        counts[canonical_index] = float(values.size)

    visible = counts > 0
    confidence = float(np.mean(np.abs(aggregated[visible]))) if np.any(visible) else 0.0
    return {
        "correlations": aggregated,
        "reliability": reliability,
        "weights": coverage_weight,
        "counts": counts,
        "confidence": confidence,
    }


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


def try_decode(coded_bytes: list[int]) -> dict | None:
    decoded = rs_decode(coded_bytes)
    if decoded is None:
        return None
    return decode_payload(decoded)


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


__all__ = [
    "VISIBLE_BLOCK_THRESHOLD",
    "_block_correlation",
    "aggregate_v2_message_evidence",
    "correlations_to_coded_bytes",
    "flip_message_bit",
    "try_decode",
    "try_decode_soft",
    "decode_payload",
]
