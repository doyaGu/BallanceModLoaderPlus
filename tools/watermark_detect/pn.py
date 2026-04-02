#!/usr/bin/env python3
"""PN sequence generation for GameTrace-WM v2."""

from __future__ import annotations

import numpy as np

from .keys import MASK64


from .layout import BLOCK_SIZE, TILE_CLASSES


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
