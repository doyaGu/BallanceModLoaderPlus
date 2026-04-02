"""Temporal accumulation helpers for GameTrace-WM v2."""

from __future__ import annotations

import numpy as np

from .layout import NUM_MESSAGE_BLOCKS


def accumulate_bit_evidence(frame_results: list[dict]) -> dict[str, np.ndarray]:
    correlations = np.zeros(NUM_MESSAGE_BLOCKS, dtype=np.float32)
    reliability = np.zeros(NUM_MESSAGE_BLOCKS, dtype=np.float32)
    counts = np.zeros(NUM_MESSAGE_BLOCKS, dtype=np.float32)

    for frame_result in frame_results:
        weights = frame_result["weights"].astype(np.float32, copy=False)
        correlations += frame_result["correlations"].astype(np.float32, copy=False) * np.maximum(weights, 1.0)
        reliability += frame_result["reliability"].astype(np.float32, copy=False)
        counts += frame_result["counts"].astype(np.float32, copy=False)

    normalized = np.divide(
        correlations,
        np.maximum(reliability, 1.0),
        out=np.zeros_like(correlations),
        where=reliability > 0.0,
    )
    return {
        "correlations": normalized,
        "reliability": reliability,
        "counts": counts,
        "weights": np.maximum(reliability, 1.0),
    }
