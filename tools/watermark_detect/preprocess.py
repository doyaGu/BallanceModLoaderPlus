from __future__ import annotations
from pathlib import Path
from typing import Any
import numpy as np
from PIL import Image, ImageFilter
from .layout import BLOCK_SIZE, TILE_W, TILE_H, TILE_COLS, TILE_ROWS


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


def _load_gray_array(image_input: np.ndarray | str | Path) -> np.ndarray:
    if isinstance(image_input, np.ndarray):
        array = np.asarray(image_input)
        if array.ndim != 2:
            raise ValueError("Expected a 2D grayscale array")
        return array.astype(np.float32, copy=False)

    image = Image.open(image_input).convert("L")
    return np.array(image, dtype=np.float32)


def _load_input_frames(path: str | Path) -> list[np.ndarray]:
    input_path = Path(path)
    if input_path.is_dir():
        frames = []
        for child in sorted(input_path.iterdir()):
            if child.suffix.lower() not in {".png", ".jpg", ".jpeg", ".bmp", ".webp"}:
                continue
            frames.append(_load_gray_array(child))
        return frames
    return [_load_gray_array(input_path)]
