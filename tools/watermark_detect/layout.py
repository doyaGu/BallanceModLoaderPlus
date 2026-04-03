#!/usr/bin/env python3
"""Layout constants and tile class permutation logic for GameTrace-WM v2."""

from __future__ import annotations

import numpy as np

from .keys import mix64


BLOCK_SIZE = 32
VISIBLE_BLOCK_THRESHOLD = 0.25
BANDPASS_SMALL_RADIUS = 1
BANDPASS_LARGE_RADIUS = 5
TILE_COLS = 16
TILE_ROWS = 16
TILE_W = TILE_COLS * BLOCK_SIZE
TILE_H = TILE_ROWS * BLOCK_SIZE
NUM_PILOT_BLOCKS = 16
NUM_MESSAGE_BLOCKS = 240
NUM_TILE_CLASSES = 4
TILE_CLASSES = ("A", "B", "C", "D")
TILE_CLASS_TO_INDEX = {tile_class: index for index, tile_class in enumerate(TILE_CLASSES)}

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
