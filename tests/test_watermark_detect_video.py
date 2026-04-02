from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools import watermark_detect
from tests.test_watermark_detect import _render_v2_frame, MASTER_KEY_HEX, BUILD_ID


def test_accumulate_v2_video_frames_improves_decode():
    occlusion = [(140, 90, 320, 220)]
    frames = [
        _render_v2_frame(crop_top=80, crop_left=120, occlusion_rects=occlusion)
        for _ in range(5)
    ]

    decoded = watermark_detect.detect_watermark_sequence(
        frames,
        MASTER_KEY_HEX,
        build_ids=[BUILD_ID],
    )

    assert decoded is not None
    assert decoded["trace_id"] == "a1b2c3d4e5f6"
    assert decoded["build_id"] == BUILD_ID
