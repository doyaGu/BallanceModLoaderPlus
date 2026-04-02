"""Synchronization and block aggregation helpers for GameTrace-WM v2."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import numpy as np

from .layout import (
    TILE_CLASSES,
    TILE_CLASS_TO_INDEX,
    TILE_COLS,
    TILE_ROWS,
    NUM_PILOT_BLOCKS,
    PILOT_BLOCK_INDICES,
    BLOCK_SIZE,
    VISIBLE_BLOCK_THRESHOLD,
    get_v2_tile_class_for_tile,
)
from .preprocess import build_block_grid


DEFAULT_TOP_SYNC_CANDIDATES = 16
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
                np.abs(correlations),
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
