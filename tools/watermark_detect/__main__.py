"""GameTrace-WM v2 detector CLI."""
from __future__ import annotations

import argparse
import sys

from .detector import Detector
from .preprocess import _load_input_frames


def main() -> None:
    parser = argparse.ArgumentParser(description="GameTrace-WM v2 detector")
    parser.add_argument("image", help="Screenshot path, video-frame path, or a directory of frames")
    parser.add_argument("--master-key", required=True, help="128-bit master key (hex)")
    parser.add_argument("--build-id", action="append",
                        help="Candidate build id (decimal or 0xHEX). Repeat for more.")
    parser.add_argument("--search", action="store_true",
                        help="Use broader phase/scale search (slower)")
    args = parser.parse_args()

    frames = _load_input_frames(args.image)
    first_height, first_width = frames[0].shape

    print("Loading input...")
    if len(frames) == 1:
        print(f"  {first_width}x{first_height}")
    else:
        print(f"  {len(frames)} frames, first frame {first_width}x{first_height}")
    print("Searching GameTrace-WM v2...")

    detector = Detector(
        args.master_key,
        build_ids=args.build_id,
        exhaustive=args.search,
    )

    result = (
        detector.detect(frames[0])
        if len(frames) == 1
        else detector.detect_sequence(frames)
    )

    if result is None:
        print("No GameTrace-WM v2 watermark found.")
        sys.exit(1)

    print("\nGameTrace-WM v2 detected!")
    print(f"  Trace ID:     {result.trace_id}")
    print(f"  Trace source: {result.trace_source}")
    print(f"  Session time: {result.session_time}")
    print(f"  Build ID:     0x{result.build_id:04X}")
    print(f"  Build ver:    {result.build_version}")
    print(f"  Confidence:   {result.confidence:.4f}")
    print(f"  Crop:         {result.crop_name} {result.crop_bounds}")
    print(f"  Scale:        {result.scale:.4f}")
    print(f"  Preprocess:   {result.preprocess}")
    print(f"  Offset:       {result.offset}")
    print(f"  Tile phase:   {result.tile_phase}")
    print(f"  Tile origin:  {result.tile_origin}")
    print(f"  Pilot score:  {result.pilot_score:.4f}")


if __name__ == "__main__":
    main()
