#!/usr/bin/env python3
"""PN sequence generation for GameTrace-WM v2."""

from __future__ import annotations

import numpy as np

from .keys import MASK64
from .layout import BLOCK_SIZE, TILE_CLASSES, BANDPASS_SMALL_RADIUS, BANDPASS_LARGE_RADIUS


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


def _box_blur_1d(arr: np.ndarray, radius: int, axis: int) -> np.ndarray:
    """1D box blur along given axis with zero-pad boundary."""
    size = 2 * radius + 1
    inv = np.float32(1.0 / size)
    padded = np.pad(arr, [(radius, radius) if i == axis else (0, 0)
                          for i in range(arr.ndim)], mode="constant")
    cs = np.cumsum(padded, axis=axis)
    # Prepend zeros along the blur axis
    zero_shape = list(cs.shape)
    zero_shape[axis] = 1
    cs = np.concatenate([np.zeros(zero_shape, dtype=cs.dtype), cs], axis=axis)
    n = arr.shape[axis]
    hi = [slice(None)] * arr.ndim
    lo = [slice(None)] * arr.ndim
    hi[axis] = slice(size, size + n)
    lo[axis] = slice(0, n)
    return (cs[tuple(hi)] - cs[tuple(lo)]) * inv


def _box_blur_2d(arr: np.ndarray, radius: int) -> np.ndarray:
    """Separable 2D box blur with zero-pad boundary. Works on 2D or batch 3D."""
    if radius <= 0:
        return arr.astype(np.float32, copy=True)
    # Determine which axes are spatial (last two)
    h_axis = arr.ndim - 2
    w_axis = arr.ndim - 1
    temp = _box_blur_1d(arr.astype(np.float32), radius, w_axis)
    return _box_blur_1d(temp, radius, h_axis)


def _bandpass_normalize(blocks: np.ndarray) -> np.ndarray:
    """Apply DoB bandpass and normalize. Input: (..., H, W), output: same shape float32."""
    smooth_small = _box_blur_2d(blocks, BANDPASS_SMALL_RADIUS)
    smooth_large = _box_blur_2d(blocks, BANDPASS_LARGE_RADIUS)
    filtered = smooth_small - smooth_large
    # Normalize per-block: max absolute value = 1
    if filtered.ndim == 2:
        max_abs = np.max(np.abs(filtered))
        if max_abs > 1e-10:
            filtered /= max_abs
    else:
        max_abs = np.max(np.abs(filtered), axis=(-2, -1), keepdims=True)
        max_abs = np.maximum(max_abs, 1e-10)
        filtered /= max_abs
    return filtered.astype(np.float32)


def generate_pn_block_np(seed: int, block_index: int) -> np.ndarray:
    state0 = np.uint64((seed ^ ((block_index * BLOCK_SEED_STRIDE) & MASK64)) & MASK64)
    z = _finalize_uint64(state0 + _PIXEL_OFFSETS)
    raw = np.where(z >> _SM_S63, np.float32(1.0), np.float32(-1.0)).reshape(BLOCK_SIZE, BLOCK_SIZE)
    return _bandpass_normalize(raw)


def precompute_templates(seed: int, count: int) -> list[np.ndarray]:
    block_indices = np.arange(count, dtype=np.uint64)
    base_states = np.uint64(seed & MASK64) ^ (block_indices * np.uint64(BLOCK_SEED_STRIDE))
    states = base_states[:, np.newaxis] + _PIXEL_OFFSETS[np.newaxis, :]
    z = _finalize_uint64(states)
    signs = np.where(z >> _SM_S63, np.float32(1.0), np.float32(-1.0))
    blocks = signs.reshape(count, BLOCK_SIZE, BLOCK_SIZE)
    filtered = _bandpass_normalize(blocks)
    return [filtered[i] for i in range(count)]


def precompute_v2_template_bank(derived: dict[str, bytes | int | dict[str, int]],
                                key: str,
                                count: int) -> dict[str, list[np.ndarray]]:
    seeds = derived[key]
    assert isinstance(seeds, dict)
    return {
        tile_class: precompute_templates(int(seeds[tile_class]), count)
        for tile_class in TILE_CLASSES
    }
