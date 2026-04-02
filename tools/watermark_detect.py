#!/usr/bin/env python3
"""GameTrace-WM v2 detector for screenshots and recording frames."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
from typing import Any

try:
    import numpy as np
except ImportError:
    print("Error: NumPy required. Install with: pip install numpy", file=sys.stderr)
    sys.exit(1)

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow required. Install with: pip install Pillow", file=sys.stderr)
    sys.exit(1)

try:
    from .watermark_detect_codec import (
        BLOCK_SIZE,
        DEFAULT_SOFT_BITS,
        DEFAULT_SOFT_FLIPS,
        MESSAGE_BLOCK_INDICES,
        NUM_MESSAGE_BLOCKS,
        NUM_PILOT_BLOCKS,
        PILOT_BLOCK_INDICES,
        RS_CODE,
        RS_DATA,
        RS_PARITY,
        TILE_CLASSES,
        TILE_CLASS_TO_INDEX,
        TILE_COLS,
        TILE_H,
        TILE_ROWS,
        TILE_W,
        correlations_to_coded_bytes,
        crc16_ccitt_false,
        decode_payload,
        derive_v2_keys,
        flip_message_bit,
        generate_pn_block_np,
        get_v2_inverse_message_permutation,
        get_v2_message_permutation,
        get_v2_message_sign_mask,
        get_v2_tile_class_for_tile,
        hkdf_sha256,
        key_bytes_to_seed,
        normalize_build_ids,
        pack_build_id,
        precompute_templates,
        precompute_v2_template_bank,
        rs_decode,
        rs_encode,
        splitmix64,
        try_decode,
        try_decode_soft,
        unpack_build_version,
    )
    from .watermark_detect_sync import (
        aggregate_v2_message_evidence,
        build_crop_candidates,
        build_preprocessed_candidates,
        build_scale_candidates,
        find_v2_sync_candidates,
        prepare_block_source,
        refine_sync_candidate_tile_origin,
        resize_gray,
    )
    from .watermark_detect_video import accumulate_bit_evidence
except ImportError:
    from watermark_detect_codec import (
        BLOCK_SIZE,
        DEFAULT_SOFT_BITS,
        DEFAULT_SOFT_FLIPS,
        MESSAGE_BLOCK_INDICES,
        NUM_MESSAGE_BLOCKS,
        NUM_PILOT_BLOCKS,
        PILOT_BLOCK_INDICES,
        RS_CODE,
        RS_DATA,
        RS_PARITY,
        TILE_CLASSES,
        TILE_CLASS_TO_INDEX,
        TILE_COLS,
        TILE_H,
        TILE_ROWS,
        TILE_W,
        correlations_to_coded_bytes,
        crc16_ccitt_false,
        decode_payload,
        derive_v2_keys,
        flip_message_bit,
        generate_pn_block_np,
        get_v2_inverse_message_permutation,
        get_v2_message_permutation,
        get_v2_message_sign_mask,
        get_v2_tile_class_for_tile,
        hkdf_sha256,
        key_bytes_to_seed,
        normalize_build_ids,
        pack_build_id,
        precompute_templates,
        precompute_v2_template_bank,
        rs_decode,
        rs_encode,
        splitmix64,
        try_decode,
        try_decode_soft,
        unpack_build_version,
    )
    from watermark_detect_sync import (
        aggregate_v2_message_evidence,
        build_crop_candidates,
        build_preprocessed_candidates,
        build_scale_candidates,
        find_v2_sync_candidates,
        prepare_block_source,
        refine_sync_candidate_tile_origin,
        resize_gray,
    )
    from watermark_detect_video import accumulate_bit_evidence


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


def _sync_candidate_to_dict(sync_candidate: Any, build_id: int, scale: float,
                            crop_candidate: dict[str, Any], preprocess_name: str) -> dict[str, Any]:
    return {
        "build_id": build_id,
        "offset": sync_candidate.offset,
        "pilot_score": sync_candidate.pilot_score,
        "pilot_confidence": sync_candidate.pilot_confidence,
        "tile_phase": sync_candidate.tile_phase,
        "scale": scale,
        "bounds": crop_candidate["bounds"],
        "crop_name": crop_candidate["name"],
        "preprocess": preprocess_name,
    }


def _merge_decoded_result(decoded: dict[str, Any], frame_result: dict[str, Any],
                          coded_bytes: list[int]) -> dict[str, Any]:
    merged = dict(decoded)
    merged.update({
        "confidence": frame_result["confidence"],
        "offset": frame_result["offset"],
        "scale": frame_result["scale"],
        "crop_name": frame_result["crop_name"],
        "crop_bounds": frame_result["crop_bounds"],
        "preprocess": frame_result["preprocess"],
        "pilot_score": frame_result["pilot_score"],
        "pilot_confidence": frame_result["pilot_confidence"],
        "tile_phase": frame_result["tile_phase"],
        "tile_origin": frame_result["tile_origin"],
        "coded_bytes": coded_bytes,
        "correlations": frame_result["correlations"],
        "reliability": frame_result["reliability"],
        "weights": frame_result["weights"],
        "counts": frame_result["counts"],
    })
    return merged


def analyze_watermark_frame(gray_input: np.ndarray | str | Path, key_hex: str, *,
                            exhaustive: bool = False,
                            build_ids: list[int | str] | None = None
                            ) -> tuple[dict[str, Any] | None, dict[str, Any] | None, dict[str, Any] | None]:
    normalized_build_ids = normalize_build_ids(build_ids)
    gray = _load_gray_array(gray_input)
    best_sync: dict[str, Any] | None = None
    best_frame_result: dict[str, Any] | None = None

    crop_candidates = build_crop_candidates(gray)
    crop_candidates.sort(key=lambda item: (0 if item["name"] == "content" else 1, item["priority"]))

    for build_id in normalized_build_ids:
        derived = derive_v2_keys(key_hex, build_id)
        pilot_bank = precompute_v2_template_bank(derived, "sync_seeds", NUM_PILOT_BLOCKS)
        message_bank = precompute_v2_template_bank(derived, "message_seeds", NUM_MESSAGE_BLOCKS)

        for crop_candidate in crop_candidates:
            scale_candidates = build_scale_candidates(exhaustive, crop_candidate["gray"].shape)
            for scale in scale_candidates:
                scaled_gray = resize_gray(crop_candidate["gray"], scale)
                if scaled_gray.shape[0] < BLOCK_SIZE or scaled_gray.shape[1] < BLOCK_SIZE:
                    continue

                prefer_highpass = scale >= 1.75
                for preprocess_name, prepared_gray in build_preprocessed_candidates(
                    scaled_gray,
                    prefer_highpass=prefer_highpass,
                ):
                    block_source = prepare_block_source(prepared_gray)
                    sync_candidates = find_v2_sync_candidates(
                        block_source,
                        pilot_bank,
                        exhaustive=exhaustive,
                    )

                    for sync_index, sync_candidate in enumerate(sync_candidates):
                        sync_summary = _sync_candidate_to_dict(
                            sync_candidate,
                            build_id,
                            scale,
                            crop_candidate,
                            preprocess_name,
                        )
                        if best_sync is None or sync_summary["pilot_score"] > best_sync["pilot_score"]:
                            best_sync = sync_summary

                        direct_evidence = aggregate_v2_message_evidence(sync_candidate, message_bank)
                        if np.any(direct_evidence["counts"] > 0):
                            frame_result = {
                                "build_id": build_id,
                                "confidence": direct_evidence["confidence"],
                                "offset": sync_candidate.offset,
                                "scale": scale,
                                "crop_name": crop_candidate["name"],
                                "crop_bounds": crop_candidate["bounds"],
                                "preprocess": preprocess_name,
                                "pilot_score": sync_candidate.pilot_score,
                                "pilot_confidence": sync_candidate.pilot_confidence,
                                "tile_phase": sync_candidate.tile_phase,
                                "tile_origin": sync_candidate.tile_origin,
                                "correlations": direct_evidence["correlations"],
                                "reliability": direct_evidence["reliability"],
                                "weights": direct_evidence["weights"],
                                "counts": direct_evidence["counts"],
                            }

                            if (
                                best_frame_result is None
                                or frame_result["pilot_score"] > best_frame_result["pilot_score"]
                                or (
                                    frame_result["pilot_score"] == best_frame_result["pilot_score"]
                                    and frame_result["confidence"] > best_frame_result["confidence"]
                                )
                            ):
                                best_frame_result = frame_result

                            decoded, coded_bytes = try_decode_soft(
                                direct_evidence["correlations"],
                                reliabilities=direct_evidence["reliability"],
                            )
                            if decoded is not None:
                                return _merge_decoded_result(decoded, frame_result, coded_bytes), best_sync, frame_result

                        if sync_index >= 8 or sync_candidate.tile_origin != (0, 0):
                            continue

                        for variant in refine_sync_candidate_tile_origin(
                            sync_candidate,
                            pilot_bank,
                            limit=4,
                        ):
                            if variant.tile_origin == (0, 0):
                                continue

                            evidence = aggregate_v2_message_evidence(variant, message_bank)
                            if not np.any(evidence["counts"] > 0):
                                continue

                            frame_result = {
                                "build_id": build_id,
                                "confidence": evidence["confidence"],
                                "offset": variant.offset,
                                "scale": scale,
                                "crop_name": crop_candidate["name"],
                                "crop_bounds": crop_candidate["bounds"],
                                "preprocess": preprocess_name,
                                "pilot_score": variant.pilot_score,
                                "pilot_confidence": variant.pilot_confidence,
                                "tile_phase": variant.tile_phase,
                                "tile_origin": variant.tile_origin,
                                "correlations": evidence["correlations"],
                                "reliability": evidence["reliability"],
                                "weights": evidence["weights"],
                                "counts": evidence["counts"],
                            }

                            if (
                                best_frame_result is None
                                or frame_result["pilot_score"] > best_frame_result["pilot_score"]
                                or (
                                    frame_result["pilot_score"] == best_frame_result["pilot_score"]
                                    and frame_result["confidence"] > best_frame_result["confidence"]
                                )
                            ):
                                best_frame_result = frame_result

                            decoded, coded_bytes = try_decode_soft(
                                evidence["correlations"],
                                reliabilities=evidence["reliability"],
                            )
                            if decoded is not None:
                                return _merge_decoded_result(decoded, frame_result, coded_bytes), best_sync, frame_result

    return None, best_sync, best_frame_result


def find_watermark(gray_input: np.ndarray | str | Path, key_hex: str, *,
                   exhaustive: bool = False,
                   build_ids: list[int | str] | None = None) -> tuple[dict[str, Any] | None, dict[str, Any] | None]:
    result, best_sync, _ = analyze_watermark_frame(
        gray_input,
        key_hex,
        exhaustive=exhaustive,
        build_ids=build_ids,
    )
    return result, best_sync


def detect_watermark(gray_input: np.ndarray | str | Path, key_hex: str, *,
                     exhaustive: bool = False,
                     build_ids: list[int | str] | None = None) -> dict[str, Any] | None:
    result, _, _ = analyze_watermark_frame(
        gray_input,
        key_hex,
        exhaustive=exhaustive,
        build_ids=build_ids,
    )
    return result


def detect_watermark_sequence(frames: list[np.ndarray | str | Path], key_hex: str, *,
                              exhaustive: bool = False,
                              build_ids: list[int | str] | None = None) -> dict[str, Any] | None:
    frame_results = []
    decoded_results = []
    best_sync = None

    for frame in frames:
        decoded, frame_best_sync, frame_result = analyze_watermark_frame(
            frame,
            key_hex,
            exhaustive=exhaustive,
            build_ids=build_ids,
        )
        if frame_best_sync is not None and (best_sync is None or frame_best_sync["pilot_score"] > best_sync["pilot_score"]):
            best_sync = frame_best_sync
        if frame_result is not None:
            frame_results.append(frame_result)
        if decoded is not None:
            decoded_results.append(decoded)

    if frame_results:
        accumulated = accumulate_bit_evidence(frame_results)
        decoded, coded_bytes = try_decode_soft(
            accumulated["correlations"],
            reliabilities=accumulated["reliability"],
        )
        if decoded is not None:
            best_frame_result = max(
                frame_results,
                key=lambda item: (item["pilot_score"], item["confidence"]),
            )
            merged_result = dict(best_frame_result)
            merged_result.update(accumulated)
            return _merge_decoded_result(decoded, merged_result, coded_bytes)

    if decoded_results:
        return max(decoded_results, key=lambda item: (item["pilot_score"], item["confidence"]))
    return None


def main() -> None:
    parser = argparse.ArgumentParser(description="GameTrace-WM v2 detector")
    parser.add_argument("image", help="Screenshot path, video-frame path, or a directory of frames")
    parser.add_argument("--master-key", required=True, help="128-bit master key (hex)")
    parser.add_argument("--build-id", action="append",
                        help="Candidate build id (decimal or 0xHEX). Repeat to add more candidates.")
    parser.add_argument("--search", action="store_true",
                        help="Use broader phase/scale search (slower, for hard cases)")
    args = parser.parse_args()

    frames = _load_input_frames(args.image)
    first_height, first_width = frames[0].shape

    print("Loading input...")
    if len(frames) == 1:
        print(f"  {first_width}x{first_height}")
    else:
        print(f"  {len(frames)} frames, first frame {first_width}x{first_height}")
    print("Searching GameTrace-WM v2...")

    build_ids = normalize_build_ids(args.build_id)
    result = (
        detect_watermark(frames[0], args.master_key, exhaustive=args.search, build_ids=build_ids)
        if len(frames) == 1
        else detect_watermark_sequence(frames, args.master_key, exhaustive=args.search, build_ids=build_ids)
    )

    if result is None:
        print("No GameTrace-WM v2 watermark found.")
        sys.exit(1)

    print("\nGameTrace-WM v2 detected!")
    print(f"  Trace ID:     {result['trace_id']}")
    print(f"  Trace source: {result['trace_source']}")
    print(f"  Session time: {result['session_time']}")
    print(f"  Build ID:     0x{result['build_id']:04X}")
    print(f"  Build ver:    {result['build_version']}")
    print(f"  Confidence:   {result['confidence']:.4f}")
    print(f"  Crop:         {result['crop_name']} {result['crop_bounds']}")
    print(f"  Scale:        {result['scale']:.4f}")
    print(f"  Preprocess:   {result['preprocess']}")
    print(f"  Offset:       {result['offset']}")
    print(f"  Tile phase:   {result['tile_phase']}")
    print(f"  Tile origin:  {result['tile_origin']}")
    print(f"  Pilot score:  {result['pilot_score']:.4f}")


__all__ = [
    "BLOCK_SIZE",
    "DEFAULT_SOFT_BITS",
    "DEFAULT_SOFT_FLIPS",
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
    "analyze_watermark_frame",
    "correlations_to_coded_bytes",
    "crc16_ccitt_false",
    "decode_payload",
    "detect_watermark",
    "detect_watermark_sequence",
    "derive_v2_keys",
    "find_watermark",
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
    "rs_decode",
    "rs_encode",
    "splitmix64",
    "try_decode",
    "try_decode_soft",
    "unpack_build_version",
]


if __name__ == "__main__":
    main()
