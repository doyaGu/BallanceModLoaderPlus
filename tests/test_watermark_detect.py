from pathlib import Path
import sys

import numpy as np
from PIL import Image, ImageEnhance
import pytest

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools import watermark_detect


CPP_CROSS_VALIDATION_CODEWORD = [
    0xA1,
    0xB2,
    0xC3,
    0xD4,
    0xE5,
    0xF6,
    0x67,
    0xF3,
    0x1A,
    0x00,
    0x00,
    0xCB,
    0x76,
    0x84,
    0x02,
    0xAF,
    0x08,
    0x2F,
    0x98,
    0x5E,
    0x79,
    0x6C,
    0x15,
    0x94,
    0xD2,
    0x4F,
    0x37,
    0xAC,
    0x69,
    0x17,
]

MASTER_KEY_HEX = "9e57b26d3feb74a3da46f009cdb11c88"
BUILD_ID = 0x00CB


def _synthesize_base_frame(width: int, height: int) -> np.ndarray:
    y_coords = np.linspace(35.0, 205.0, height, dtype=np.float32)[:, None]
    x_coords = np.linspace(15.0, 75.0, width, dtype=np.float32)[None, :]
    base = y_coords + x_coords
    base += 18.0 * np.sin(np.linspace(0.0, 12.0, width, dtype=np.float32))[None, :]
    base += 12.0 * np.cos(np.linspace(0.0, 9.0, height, dtype=np.float32))[:, None]
    return np.clip(base, 0.0, 255.0)


def _apply_occlusions(gray: np.ndarray, occlusion_rects: list[tuple[int, int, int, int]] | None) -> np.ndarray:
    if not occlusion_rects:
        return gray

    occluded = gray.copy()
    for left, top, right, bottom in occlusion_rects:
        occluded[top:bottom, left:right] = 210.0
    return occluded


def _scale_frame(gray: np.ndarray, scale: float) -> np.ndarray:
    image = Image.fromarray(gray.astype(np.uint8), mode="L")
    resized = image.resize(
        (round(image.width * scale), round(image.height * scale)),
        Image.Resampling.BICUBIC,
    )
    return np.array(resized, dtype=np.uint8)


def _wrap_window_frame(client_frame: np.ndarray, title_bar: int = 36, border: int = 2) -> np.ndarray:
    height, width = client_frame.shape
    screenshot = np.full(
        (height + title_bar + border * 2, width + border * 2),
        242,
        dtype=np.uint8,
    )
    screenshot[: title_bar + border, :] = 238
    screenshot[border + title_bar :, :border] = 210
    screenshot[border + title_bar :, width + border :] = 210
    screenshot[border + title_bar : border + title_bar + height, border : border + width] = client_frame
    screenshot[title_bar // 2 : title_bar // 2 + 2, border + 20 : border + 240] = 180
    return screenshot


def _adjust_brightness_contrast(gray: np.ndarray, brightness: float = 1.08, contrast: float = 1.12) -> np.ndarray:
    image = Image.fromarray(gray.astype(np.uint8), mode="L")
    adjusted = ImageEnhance.Brightness(image).enhance(brightness)
    adjusted = ImageEnhance.Contrast(adjusted).enhance(contrast)
    return np.array(adjusted, dtype=np.uint8)


def _render_v2_frame(width: int = 1024, height: int = 1024, *,
                     crop_top: int = 0, crop_left: int = 0,
                     occlusion_rects: list[tuple[int, int, int, int]] | None = None,
                     scale: float = 1.0,
                     message_alpha: float = 1.6,
                     pilot_alpha: float = 2.5) -> np.ndarray:
    derived = watermark_detect.derive_v2_keys(MASTER_KEY_HEX, BUILD_ID)
    frame = _synthesize_base_frame(width, height)

    message_alpha_ratio = message_alpha / 255.0
    pilot_alpha_ratio = pilot_alpha / 255.0

    for tile_y in range(0, height, watermark_detect.TILE_H):
        for tile_x in range(0, width, watermark_detect.TILE_W):
            tile_class = watermark_detect.get_v2_tile_class_for_tile(
                tile_x // watermark_detect.TILE_W,
                tile_y // watermark_detect.TILE_H,
            )

            pilot_templates = [
                watermark_detect.generate_pn_block_np(derived["sync_seeds"][tile_class], pilot_index)
                for pilot_index in range(watermark_detect.NUM_PILOT_BLOCKS)
            ]
            message_templates = [
                watermark_detect.generate_pn_block_np(derived["message_seeds"][tile_class], local_index)
                for local_index in range(watermark_detect.NUM_MESSAGE_BLOCKS)
            ]
            permutation = watermark_detect.get_v2_message_permutation(tile_class)
            sign_mask = watermark_detect.get_v2_message_sign_mask(tile_class)

            for pilot_idx, block_index in enumerate(watermark_detect.PILOT_BLOCK_INDICES):
                block_col = block_index % watermark_detect.TILE_COLS
                block_row = block_index // watermark_detect.TILE_COLS
                bx = tile_x + block_col * watermark_detect.BLOCK_SIZE
                by = tile_y + block_row * watermark_detect.BLOCK_SIZE
                if bx >= width or by >= height:
                    continue

                x1 = min(bx + watermark_detect.BLOCK_SIZE, width)
                y1 = min(by + watermark_detect.BLOCK_SIZE, height)
                pn_block = pilot_templates[pilot_idx][: y1 - by, : x1 - bx]
                pattern = np.where(pn_block > 0.0, 255.0, 0.0)
                region = frame[by:y1, bx:x1]
                frame[by:y1, bx:x1] = region * (1.0 - pilot_alpha_ratio) + pattern * pilot_alpha_ratio

            for local_index, block_index in enumerate(watermark_detect.MESSAGE_BLOCK_INDICES):
                canonical_index = permutation[local_index]
                block_col = block_index % watermark_detect.TILE_COLS
                block_row = block_index // watermark_detect.TILE_COLS
                bx = tile_x + block_col * watermark_detect.BLOCK_SIZE
                by = tile_y + block_row * watermark_detect.BLOCK_SIZE
                if bx >= width or by >= height:
                    continue

                x1 = min(bx + watermark_detect.BLOCK_SIZE, width)
                y1 = min(by + watermark_detect.BLOCK_SIZE, height)
                byte_idx = canonical_index // 8
                bit_idx = 7 - (canonical_index % 8)
                coded_bit = (CPP_CROSS_VALIDATION_CODEWORD[byte_idx] >> bit_idx) & 1
                if sign_mask[canonical_index]:
                    coded_bit ^= 1
                pn_block = message_templates[local_index][: y1 - by, : x1 - bx]
                pattern = np.where(
                    np.logical_xor(coded_bit == 1, pn_block > 0.0),
                    255.0,
                    0.0,
                )
                region = frame[by:y1, bx:x1]
                frame[by:y1, bx:x1] = region * (1.0 - message_alpha_ratio) + pattern * message_alpha_ratio

    frame = _apply_occlusions(frame, occlusion_rects)
    if crop_top or crop_left:
        frame = frame[crop_top:, crop_left:]
    if scale != 1.0:
        frame = _scale_frame(frame.astype(np.uint8), scale).astype(np.float32)
    return frame.astype(np.uint8)


def test_rs_decode_accepts_valid_cpp_codeword():
    decoded = watermark_detect.rs_decode(CPP_CROSS_VALIDATION_CODEWORD)
    assert decoded == CPP_CROSS_VALIDATION_CODEWORD[: watermark_detect.RS_DATA]


def test_try_decode_parses_cross_validation_payload():
    decoded = watermark_detect.try_decode(CPP_CROSS_VALIDATION_CODEWORD)
    assert decoded == {
        "trace_id": "a1b2c3d4e5f6",
        "trace_source": "device_fallback",
        "session_time": "2025-04-07 00:19:12 UTC",
        "build_id": 0x00CB,
        "build_version": "0.3.11",
    }


def test_detect_v2_from_window_screenshot():
    screenshot = _wrap_window_frame(_render_v2_frame())
    decoded = watermark_detect.detect_watermark(
        screenshot,
        MASTER_KEY_HEX,
        build_ids=[BUILD_ID],
    )
    assert decoded is not None
    assert decoded["trace_id"] == "a1b2c3d4e5f6"
    assert decoded["build_id"] == BUILD_ID


def test_detect_v2_from_scaled_window_screenshot():
    screenshot = _scale_frame(_wrap_window_frame(_render_v2_frame()), 0.75)
    decoded = watermark_detect.detect_watermark(
        screenshot,
        MASTER_KEY_HEX,
        build_ids=[BUILD_ID],
    )
    assert decoded is not None
    assert decoded["trace_id"] == "a1b2c3d4e5f6"
    assert decoded["scale"] == pytest.approx(4.0 / 3.0, abs=0.05)


def test_detect_v2_from_half_scaled_window_screenshot():
    screenshot = _scale_frame(_wrap_window_frame(_render_v2_frame()), 0.5)
    decoded = watermark_detect.detect_watermark(
        screenshot,
        MASTER_KEY_HEX,
        build_ids=[BUILD_ID],
    )
    assert decoded is not None
    assert decoded["trace_id"] == "a1b2c3d4e5f6"
    assert decoded["scale"] == pytest.approx(2.0, abs=0.05)


def test_detect_v2_from_missing_origin_crop():
    screenshot = _render_v2_frame(crop_top=80, crop_left=120)
    decoded = watermark_detect.detect_watermark(
        screenshot,
        MASTER_KEY_HEX,
        build_ids=[BUILD_ID],
    )
    assert decoded is not None
    assert decoded["trace_id"] == "a1b2c3d4e5f6"


def test_detect_v2_from_occluded_crop():
    screenshot = _render_v2_frame(
        crop_top=80,
        crop_left=120,
        occlusion_rects=[(140, 90, 320, 220)],
    )
    decoded = watermark_detect.detect_watermark(
        screenshot,
        MASTER_KEY_HEX,
        build_ids=[BUILD_ID],
    )
    assert decoded is not None
    assert decoded["trace_id"] == "a1b2c3d4e5f6"


def test_detect_v2_after_brightness_contrast_adjustment():
    screenshot = _adjust_brightness_contrast(_wrap_window_frame(_render_v2_frame()))
    decoded = watermark_detect.detect_watermark(
        screenshot,
        MASTER_KEY_HEX,
        build_ids=[BUILD_ID],
    )
    assert decoded is not None
    assert decoded["trace_id"] == "a1b2c3d4e5f6"
