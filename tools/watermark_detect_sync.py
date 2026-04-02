#!/usr/bin/env python3
"""Synchronization and block aggregation helpers for GameTrace-WM v2."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import numpy as np
from PIL import Image, ImageFilter

try:
    from .watermark_detect_codec import (
        BLOCK_SIZE,
        MESSAGE_BLOCK_INDICES,
        NUM_MESSAGE_BLOCKS,
        NUM_PILOT_BLOCKS,
        PILOT_BLOCK_INDICES,
        TILE_CLASSES,
        TILE_CLASS_TO_INDEX,
        TILE_COLS,
        TILE_H,
        TILE_ROWS,
        TILE_W,
        get_v2_message_permutation,
        get_v2_message_sign_mask,
        get_v2_tile_class_for_tile,
    )
except ImportError:
    from watermark_detect_codec import (
        BLOCK_SIZE,
        MESSAGE_BLOCK_INDICES,
        NUM_MESSAGE_BLOCKS,
        NUM_PILOT_BLOCKS,
        PILOT_BLOCK_INDICES,
        TILE_CLASSES,
        TILE_CLASS_TO_INDEX,
        TILE_COLS,
        TILE_H,
        TILE_ROWS,
        TILE_W,
        get_v2_message_permutation,
        get_v2_message_sign_mask,
        get_v2_tile_class_for_tile,
    )


DEFAULT_TOP_SYNC_CANDIDATES = 16
VISIBLE_BLOCK_THRESHOLD = 0.25
DEFAULT_TOP_TILE_ORIGINS = 4
PILOT_BLOCK_COORDS = [(block_index % TILE_COLS, block_index // TILE_COLS) for block_index in PILOT_BLOCK_INDICES]


@dataclass(slots=True)
class SyncCandidate:
    offset: tuple[int, int]
    pilot_score: float
    pilot_confidence: float
    tile_phase: tuple[int, int]
    tile_origin: tuple[int, int]
    tile_class_scores: np.ndarray
    tile_visible_weights: np.ndarray
    blocks: tuple[np.ndarray, np.ndarray]


def smooth_signal(values: np.ndarray, window: int = 9) -> np.ndarray:
    if values.size == 0 or window <= 1:
        return values.astype(np.float32, copy=False)
    kernel = np.ones(window, dtype=np.float32) / float(window)
    return np.convolve(values.astype(np.float32, copy=False), kernel, mode="same")


def find_active_edge(scores: np.ndarray, max_scan: int, window: int = 4) -> int:
    if scores.size == 0:
        return 0

    smoothed = smooth_signal(scores)
    threshold = max(
        float(np.percentile(smoothed, 75)) * 0.5,
        float(np.max(smoothed)) * 0.15,
        0.1,
    )
    limit = min(max_scan, max(0, smoothed.size - window))
    for index in range(limit + 1):
        run = smoothed[index:index + window]
        if run.size < window:
            break
        if np.count_nonzero(run > threshold) >= max(2, window - 1):
            return index
    return 0


def estimate_content_bounds(gray: np.ndarray) -> tuple[int, int, int, int]:
    height, width = gray.shape
    row_activity = np.mean(np.abs(np.diff(gray, axis=1)), axis=1)
    col_activity = np.mean(np.abs(np.diff(gray, axis=0)), axis=0)

    top = find_active_edge(row_activity, min(96, height // 5 if height >= 5 else height))
    left = find_active_edge(col_activity, min(48, width // 6 if width >= 6 else width))
    bottom_trim = find_active_edge(row_activity[::-1], min(64, height // 6 if height >= 6 else height))
    right_trim = find_active_edge(col_activity[::-1], min(32, width // 8 if width >= 8 else width))

    bottom = max(top + TILE_H, height - bottom_trim)
    right = max(left + TILE_W, width - right_trim)
    return left, top, right, bottom


def crop_gray(gray: np.ndarray, bounds: tuple[int, int, int, int]) -> np.ndarray:
    left, top, right, bottom = bounds
    return gray[top:bottom, left:right]


def build_crop_candidates(gray: np.ndarray) -> list[dict[str, Any]]:
    height, width = gray.shape
    full_bounds = (0, 0, width, height)
    candidates = [{"name": "full", "bounds": full_bounds, "gray": gray, "priority": 10_000}]

    content_bounds = estimate_content_bounds(gray)
    if content_bounds == full_bounds:
        return candidates

    seen_bounds = set()
    top_adjustments = [0, 2, -2]
    left_adjustments = [0]
    for top_adjust in top_adjustments:
        for left_adjust in left_adjustments:
            left = min(max(0, content_bounds[0] + left_adjust), max(0, width - BLOCK_SIZE))
            top = min(max(0, content_bounds[1] + top_adjust), max(0, height - BLOCK_SIZE))
            bounds = (left, top, content_bounds[2], content_bounds[3])
            if bounds in seen_bounds:
                continue
            seen_bounds.add(bounds)

            candidate_gray = crop_gray(gray, bounds)
            if candidate_gray.size == 0:
                continue
            candidates.append({
                "name": "content",
                "bounds": bounds,
                "gray": candidate_gray,
                "priority": (
                    abs(top_adjust) + abs(left_adjust),
                    abs(left_adjust),
                    abs(top_adjust),
                    0 if top_adjust >= 0 else 1,
                ),
            })
    return candidates


def resize_gray(gray: np.ndarray, scale: float) -> np.ndarray:
    if abs(scale - 1.0) < 1e-6:
        return gray.astype(np.float32, copy=False)
    height, width = gray.shape
    resized_width = max(1, int(round(width * scale)))
    resized_height = max(1, int(round(height * scale)))
    image = Image.fromarray(np.clip(gray, 0.0, 255.0).astype(np.uint8), mode="L")
    resized = image.resize((resized_width, resized_height), Image.Resampling.BICUBIC)
    return np.array(resized, dtype=np.float32)


def high_pass_gray(gray: np.ndarray, radius: int = 8) -> np.ndarray:
    image = Image.fromarray(np.clip(gray, 0.0, 255.0).astype(np.uint8), mode="L")
    blurred = image.filter(ImageFilter.BoxBlur(radius))
    return gray.astype(np.float32, copy=False) - np.array(blurred, dtype=np.float32)


def build_preprocessed_candidates(gray: np.ndarray, prefer_highpass: bool = False) -> list[tuple[str, np.ndarray]]:
    candidates = [("raw", gray.astype(np.float32, copy=False))]
    if prefer_highpass:
        candidates.append(("highpass", high_pass_gray(gray)))
    if prefer_highpass:
        return candidates
    return candidates


def build_scale_candidates(exhaustive: bool, image_shape: tuple[int, int] | None = None) -> list[float]:
    if exhaustive:
        scales = [1.0, 0.5, 4.0 / 3.0, 2.0, 0.75, 1.25, 1.5, 0.95, 0.975,
                  1.025, 1.05, 0.875, 0.8, 2.0 / 3.0]
        scales.extend(np.linspace(0.5, 2.0, 31).tolist())
    else:
        scales = [1.0, 0.5, 4.0 / 3.0]

    unique = []
    seen = set()
    for scale in scales:
        rounded = round(float(scale), 6)
        if rounded in seen:
            continue
        seen.add(rounded)
        unique.append(float(scale))

    if image_shape is None:
        return unique

    height, width = image_shape
    size_hint = min(height, width)
    # Very large images likely have DPI scaling — try downscale first
    if size_hint > 1200:
        preferred = [0.5, 1.0, 0.75, 4.0 / 3.0]
    elif size_hint < 640:
        preferred = [2.0, 1.0, 4.0 / 3.0, 0.5]
    else:
        preferred = [1.0, 0.5, 4.0 / 3.0]

    ordered = []
    seen.clear()
    for scale in preferred + unique:
        rounded = round(float(scale), 6)
        if rounded in seen:
            continue
        seen.add(rounded)
        ordered.append(float(scale))
    return ordered


def prepare_block_source(gray: np.ndarray) -> dict[str, np.ndarray]:
    pad = ((TILE_H, 0), (TILE_W, 0))
    padded_gray = np.pad(gray, pad, mode="constant")
    padded_mask = np.pad(np.ones_like(gray, dtype=np.float32), pad, mode="constant")
    return {
        "gray": padded_gray.astype(np.float32, copy=False),
        "mask": padded_mask,
    }


def build_block_grid(block_source: dict[str, np.ndarray], dx: int, dy: int) -> tuple[np.ndarray, np.ndarray] | None:
    start_y = TILE_H + dy
    start_x = TILE_W + dx
    aligned = block_source["gray"][start_y:, start_x:]
    aligned_mask = block_source["mask"][start_y:, start_x:]
    block_rows = aligned.shape[0] // BLOCK_SIZE
    block_cols = aligned.shape[1] // BLOCK_SIZE
    if block_rows <= 0 or block_cols <= 0:
        return None

    trimmed = aligned[:block_rows * BLOCK_SIZE, :block_cols * BLOCK_SIZE]
    trimmed_mask = aligned_mask[:block_rows * BLOCK_SIZE, :block_cols * BLOCK_SIZE]

    blocks = trimmed.reshape(block_rows, BLOCK_SIZE, block_cols, BLOCK_SIZE)
    blocks = blocks.transpose(0, 2, 1, 3).astype(np.float32, copy=False)
    block_masks = trimmed_mask.reshape(block_rows, BLOCK_SIZE, block_cols, BLOCK_SIZE)
    block_masks = block_masks.transpose(0, 2, 1, 3).astype(np.float32, copy=False)

    valid_counts = np.sum(block_masks, axis=(2, 3), keepdims=True)
    safe_counts = np.maximum(valid_counts, 1.0)
    means = np.sum(blocks * block_masks, axis=(2, 3), keepdims=True) / safe_counts
    centered_blocks = (blocks - means) * block_masks
    coverage = valid_counts[..., 0, 0] / float(BLOCK_SIZE * BLOCK_SIZE)
    return centered_blocks, coverage


def _block_correlation(blocks: np.ndarray, coverage: np.ndarray, row: int, col: int,
                       template: np.ndarray) -> tuple[float, float]:
    block_coverage = float(coverage[row, col])
    if block_coverage < VISIBLE_BLOCK_THRESHOLD:
        return 0.0, 0.0
    numerator = float(np.tensordot(blocks[row, col], template, axes=([0, 1], [0, 1])))
    valid_pixels = max(block_coverage * float(BLOCK_SIZE * BLOCK_SIZE), 1.0)
    return numerator / valid_pixels, block_coverage


def _compute_pilot_correlation_maps(block_grid: tuple[np.ndarray, np.ndarray],
                                    pilot_bank: dict[str, list[np.ndarray]]) -> tuple[np.ndarray, np.ndarray]:
    blocks, coverage = block_grid
    block_rows, block_cols = coverage.shape
    pilot_maps = np.zeros((len(TILE_CLASSES), NUM_PILOT_BLOCKS, block_rows, block_cols), dtype=np.float32)
    pilot_visibility = np.zeros((block_rows, block_cols), dtype=np.float32)

    valid_pixels = np.maximum(coverage * float(BLOCK_SIZE * BLOCK_SIZE), 1.0)
    visible_mask = coverage >= VISIBLE_BLOCK_THRESHOLD

    for class_index, tile_class in enumerate(TILE_CLASSES):
        for pilot_index in range(NUM_PILOT_BLOCKS):
            numerators = np.tensordot(
                blocks,
                pilot_bank[tile_class][pilot_index],
                axes=([2, 3], [0, 1]),
            )
            correlations = numerators / valid_pixels
            pilot_maps[class_index, pilot_index] = np.where(
                visible_mask,
                np.maximum(correlations, 0.0),
                0.0,
            ).astype(np.float32, copy=False)

    pilot_visibility[visible_mask] = coverage[visible_mask]
    return pilot_maps, pilot_visibility


def _score_tile_class_maps(pilot_maps: np.ndarray, pilot_visibility: np.ndarray, *,
                           tile_origin_x: int = 0,
                           tile_origin_y: int = 0) -> tuple[np.ndarray, np.ndarray]:
    _, _, block_rows, block_cols = pilot_maps.shape
    if tile_origin_y >= block_rows or tile_origin_x >= block_cols:
        return (
            np.full((0, 0, len(TILE_CLASSES)), -np.inf, dtype=np.float32),
            np.zeros((0, 0, len(TILE_CLASSES)), dtype=np.float32),
        )

    tile_rows = max(0, (block_rows - tile_origin_y + TILE_ROWS - 1) // TILE_ROWS)
    tile_cols = max(0, (block_cols - tile_origin_x + TILE_COLS - 1) // TILE_COLS)
    class_scores = np.full((tile_rows, tile_cols, len(TILE_CLASSES)), -np.inf, dtype=np.float32)
    visible_weights = np.zeros((tile_rows, tile_cols, len(TILE_CLASSES)), dtype=np.float32)

    for tile_row in range(tile_rows):
        for tile_col in range(tile_cols):
            base_row = tile_origin_y + tile_row * TILE_ROWS
            base_col = tile_origin_x + tile_col * TILE_COLS
            for class_index, _tile_class in enumerate(TILE_CLASSES):
                correlation_sum = 0.0
                visible_count = 0
                coverage_sum = 0.0
                for pilot_index, (pilot_col, pilot_row) in enumerate(
                    ((col, row) for col, row in ((block_index % TILE_COLS, block_index // TILE_COLS)
                     for block_index in PILOT_BLOCK_INDICES))
                ):
                    block_row = base_row + pilot_row
                    block_col = base_col + pilot_col
                    if block_row >= block_rows or block_col >= block_cols:
                        continue
                    block_coverage = float(pilot_visibility[block_row, block_col])
                    if block_coverage <= 0.0:
                        continue
                    correlation_sum += float(pilot_maps[class_index, pilot_index, block_row, block_col])
                    visible_count += 1
                    coverage_sum += block_coverage

                if visible_count == 0:
                    continue

                visible_fraction = coverage_sum / float(NUM_PILOT_BLOCKS)
                visible_weights[tile_row, tile_col, class_index] = visible_fraction
                class_scores[tile_row, tile_col, class_index] = (
                    correlation_sum / float(visible_count)
                ) * visible_fraction

    return class_scores, visible_weights


def _score_supertile_phase(class_scores: np.ndarray, visible_weights: np.ndarray,
                           phase_x: int, phase_y: int) -> tuple[float, float]:
    tile_rows, tile_cols, _ = class_scores.shape
    total_score = 0.0
    matched_tiles = 0
    for tile_row in range(tile_rows):
        for tile_col in range(tile_cols):
            tile_class = get_v2_tile_class_for_tile(tile_col + phase_x, tile_row + phase_y)
            class_index = TILE_CLASS_TO_INDEX[tile_class]
            score = float(class_scores[tile_row, tile_col, class_index])
            visible = float(visible_weights[tile_row, tile_col, class_index])
            if not np.isfinite(score) or visible <= 0.0:
                continue
            total_score += score
            matched_tiles += 1

    if matched_tiles == 0:
        return float("-inf"), 0.0

    mean_score = total_score / float(matched_tiles)
    coverage = matched_tiles / float(tile_rows * tile_cols)
    return mean_score * coverage, mean_score


def _score_alignment_from_maps(pilot_maps: np.ndarray, pilot_visibility: np.ndarray, *,
                               tile_origin_x: int = 0,
                               tile_origin_y: int = 0) -> tuple[float, float, tuple[int, int], np.ndarray, np.ndarray]:
    class_scores, visible_weights = _score_tile_class_maps(
        pilot_maps,
        pilot_visibility,
        tile_origin_x=tile_origin_x,
        tile_origin_y=tile_origin_y,
    )
    best_phase = None
    best_score = float("-inf")
    best_confidence = 0.0
    for phase_y in range(2):
        for phase_x in range(2):
            pilot_score, pilot_confidence = _score_supertile_phase(
                class_scores,
                visible_weights,
                phase_x,
                phase_y,
            )
            if pilot_score > best_score:
                best_score = pilot_score
                best_confidence = pilot_confidence
                best_phase = (phase_x, phase_y)

    if best_phase is None:
        best_phase = (0, 0)
    return best_score, best_confidence, best_phase, class_scores, visible_weights


def _score_tile_class_maps_slow_path(block_grid: tuple[np.ndarray, np.ndarray],
                                     pilot_bank: dict[str, list[np.ndarray]]) -> tuple[np.ndarray, np.ndarray]:
    pilot_maps, pilot_visibility = _compute_pilot_correlation_maps(block_grid, pilot_bank)
    return _score_tile_class_maps(pilot_maps, pilot_visibility)


def _iter_tile_origin_candidates(block_rows: int, block_cols: int) -> list[tuple[int, int]]:
    candidates = [(0, 0)]
    seen = {(0, 0)}
    max_origin_y = min(TILE_ROWS, block_rows)
    max_origin_x = min(TILE_COLS, block_cols)
    for origin_y in range(max_origin_y):
        for origin_x in range(max_origin_x):
            if (origin_x, origin_y) in seen:
                continue
            seen.add((origin_x, origin_y))
            candidates.append((origin_x, origin_y))
    return candidates


def rank_tile_origins(pilot_maps: np.ndarray, block_rows: int, block_cols: int,
                      limit: int = DEFAULT_TOP_TILE_ORIGINS) -> list[tuple[int, int]]:
    pilot_presence = np.max(pilot_maps, axis=(0, 1))
    max_origin_y = min(TILE_ROWS, block_rows)
    max_origin_x = min(TILE_COLS, block_cols)
    scores: list[tuple[float, tuple[int, int]]] = [(0.0, (0, 0))]

    for origin_y in range(max_origin_y):
        for origin_x in range(max_origin_x):
            score = 0.0
            visible = 0
            for pilot_col, pilot_row in PILOT_BLOCK_COORDS:
                rows = slice(origin_y + pilot_row, block_rows, TILE_ROWS)
                cols = slice(origin_x + pilot_col, block_cols, TILE_COLS)
                samples = pilot_presence[rows, cols]
                if samples.size == 0:
                    continue
                score += float(np.mean(samples))
                visible += 1
            if visible == 0:
                continue
            scores.append((score / float(visible), (origin_x, origin_y)))

    scores.sort(key=lambda item: item[0], reverse=True)
    ranked = []
    seen = set()
    for _score, origin in scores:
        if origin in seen:
            continue
        seen.add(origin)
        ranked.append(origin)
        if len(ranked) >= limit:
            break
    return ranked


def refine_sync_candidate_tile_origin(candidate: SyncCandidate,
                                      pilot_bank: dict[str, list[np.ndarray]],
                                      limit: int = 4) -> list[SyncCandidate]:
    block_rows, block_cols = candidate.blocks[1].shape
    pilot_maps, pilot_visibility = _compute_pilot_correlation_maps(candidate.blocks, pilot_bank)
    refined: list[SyncCandidate] = []

    for tile_origin_x, tile_origin_y in rank_tile_origins(pilot_maps, block_rows, block_cols, limit=max(limit, 4)):
        pilot_score, pilot_confidence, tile_phase, class_scores, visible_weights = _score_alignment_from_maps(
            pilot_maps,
            pilot_visibility,
            tile_origin_x=tile_origin_x,
            tile_origin_y=tile_origin_y,
        )
        if pilot_score == float("-inf"):
            continue

        keep_top_sync_candidates(
            refined,
            SyncCandidate(
                offset=candidate.offset,
                pilot_score=pilot_score,
                pilot_confidence=pilot_confidence,
                tile_phase=tile_phase,
                tile_origin=(tile_origin_x, tile_origin_y),
                tile_class_scores=class_scores,
                tile_visible_weights=visible_weights,
                blocks=candidate.blocks,
            ),
            limit=limit,
        )

    return refined


def _legacy_score_tile_class_maps(block_grid: tuple[np.ndarray, np.ndarray],
                                  pilot_bank: dict[str, list[np.ndarray]]) -> tuple[np.ndarray, np.ndarray]:
    return _score_tile_class_maps_slow_path(block_grid, pilot_bank)


def keep_top_sync_candidates(candidates: list[SyncCandidate], candidate: SyncCandidate,
                             limit: int = DEFAULT_TOP_SYNC_CANDIDATES) -> None:
    signature = (candidate.offset, candidate.tile_origin, candidate.tile_phase)
    for index, existing in enumerate(candidates):
        if (existing.offset, existing.tile_origin, existing.tile_phase) == signature:
            if candidate.pilot_score > existing.pilot_score:
                candidates[index] = candidate
            break
    else:
        candidates.append(candidate)

    candidates.sort(key=lambda item: item.pilot_score, reverse=True)
    del candidates[limit:]


def build_refine_offsets(center: tuple[int, int], radius: int = 3) -> list[tuple[int, int]]:
    offsets = []
    seen = set()
    center_x, center_y = center
    for dy in range(max(0, center_y - radius), min(BLOCK_SIZE - 1, center_y + radius) + 1):
        for dx in range(max(0, center_x - radius), min(BLOCK_SIZE - 1, center_x + radius) + 1):
            if (dx, dy) in seen:
                continue
            seen.add((dx, dy))
            offsets.append((dx, dy))
    return offsets


def score_sync_offset(block_source: dict[str, np.ndarray], pilot_bank: dict[str, list[np.ndarray]],
                      dx: int, dy: int) -> SyncCandidate | None:
    block_grid = build_block_grid(block_source, dx, dy)
    if block_grid is None:
        return None

    pilot_maps, pilot_visibility = _compute_pilot_correlation_maps(block_grid, pilot_bank)
    best_score, best_confidence, best_phase, class_scores, visible_weights = _score_alignment_from_maps(
        pilot_maps,
        pilot_visibility,
        tile_origin_x=0,
        tile_origin_y=0,
    )

    if best_phase is None or best_score == float("-inf"):
        return None

    return SyncCandidate(
        offset=(dx, dy),
        pilot_score=best_score,
        pilot_confidence=best_confidence,
        tile_phase=best_phase,
        tile_origin=(0, 0),
        tile_class_scores=class_scores,
        tile_visible_weights=visible_weights,
        blocks=block_grid,
    )


def find_v2_sync_candidates(block_source: dict[str, np.ndarray], pilot_bank: dict[str, list[np.ndarray]],
                            exhaustive: bool = False) -> list[SyncCandidate]:
    if exhaustive:
        offsets = [(dx, dy) for dy in range(BLOCK_SIZE) for dx in range(BLOCK_SIZE)]
        best_candidates: list[SyncCandidate] = []
        for dx, dy in offsets:
            candidate = score_sync_offset(block_source, pilot_bank, dx, dy)
            if candidate is not None:
                keep_top_sync_candidates(best_candidates, candidate)
        return best_candidates

    coarse_candidates: list[SyncCandidate] = []
    coarse_offsets = [(dx, dy) for dy in range(0, BLOCK_SIZE, 8) for dx in range(0, BLOCK_SIZE, 8)]
    for dx, dy in coarse_offsets:
        candidate = score_sync_offset(block_source, pilot_bank, dx, dy)
        if candidate is not None:
            keep_top_sync_candidates(coarse_candidates, candidate, limit=8)

    refined_candidates: list[SyncCandidate] = []
    seen_offsets = set()
    for coarse in coarse_candidates[:2]:
        for dx, dy in build_refine_offsets(coarse.offset, radius=2):
            if (dx, dy) in seen_offsets:
                continue
            seen_offsets.add((dx, dy))
            candidate = score_sync_offset(block_source, pilot_bank, dx, dy)
            if candidate is not None:
                keep_top_sync_candidates(refined_candidates, candidate)

    ordered_candidates = list(coarse_candidates)
    existing = {
        (candidate.offset, candidate.tile_origin, candidate.tile_phase)
        for candidate in ordered_candidates
    }
    for candidate in refined_candidates:
        signature = (candidate.offset, candidate.tile_origin, candidate.tile_phase)
        if signature in existing:
            continue
        existing.add(signature)
        ordered_candidates.append(candidate)
    return ordered_candidates[:DEFAULT_TOP_SYNC_CANDIDATES]


def aggregate_v2_message_evidence(
    candidate: SyncCandidate,
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


__all__ = [
    "SyncCandidate",
    "aggregate_v2_message_evidence",
    "build_crop_candidates",
    "build_preprocessed_candidates",
    "build_scale_candidates",
    "find_v2_sync_candidates",
    "high_pass_gray",
    "prepare_block_source",
    "refine_sync_candidate_tile_origin",
    "resize_gray",
]
