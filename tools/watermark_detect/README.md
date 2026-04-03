# GameTrace-WM v2 Detector

Offline watermark detector for BML Plus screenshots and video recordings.
Extracts embedded trace ID, session timestamp, and build version from
spread-spectrum watermarks produced by the BML watermark encoder.

## Requirements

- Python 3.11+
- NumPy
- Pillow (PIL)

## Quick Start

```bash
# Single screenshot
python -m tools.watermark_detect screenshot.png \
    --master-key 9E57B26D3FEB74A3DA46F009CDB11C88

# Video frames (directory of PNGs)
python -m tools.watermark_detect frames_dir/ \
    --master-key 9E57B26D3FEB74A3DA46F009CDB11C88

# Exhaustive search (slower, for hard cases)
python -m tools.watermark_detect screenshot.png \
    --master-key 9E57B26D3FEB74A3DA46F009CDB11C88 \
    --search

# Specify build ID explicitly
python -m tools.watermark_detect screenshot.png \
    --master-key 9E57B26D3FEB74A3DA46F009CDB11C88 \
    --build-id 0x00CB
```

## Python API

```python
from tools.watermark_detect import Detector

# Create detector (derives keys, precomputes templates once)
detector = Detector(
    master_key_hex="9E57B26D3FEB74A3DA46F009CDB11C88",
    build_ids=["0x00CB"],       # optional, auto-detected from Version.h
    exhaustive=False,           # True for broader search
)

# Single-frame detection
result = detector.detect("screenshot.png")  # or numpy array
if result:
    print(result.trace_id)       # e.g. "31086ea8522f"
    print(result.session_time)   # e.g. "2026-04-02 21:37:05 UTC"
    print(result.build_version)  # e.g. "0.3.11"
    print(result.confidence)     # e.g. 0.620

# Full diagnostic report (even if decode fails)
report = detector.analyze("screenshot.png")
print(report.decoded)        # DetectResult or None
print(report.best_sync)      # SyncInfo (best pilot alignment found)
print(report.best_evidence)  # EvidenceInfo (best message correlations)

# Multi-frame detection (video)
result = detector.detect_sequence(["frame001.png", "frame002.png", ...])
```

## Output Fields

| Field | Type | Description |
|-------|------|-------------|
| `trace_id` | str | 6-byte device fingerprint (hex) |
| `trace_source` | str | Identity source (e.g. "device_fallback") |
| `session_time` | str | Session start timestamp (UTC) |
| `build_id` | int | Packed build version (16-bit) |
| `build_version` | str | Decoded version string (e.g. "0.3.11") |
| `confidence` | float | Mean absolute correlation of message bits |
| `pilot_score` | float | Pilot block alignment quality |
| `scale` | float | Image scale factor used for detection |
| `offset` | tuple | Block grid offset (dx, dy) in pixels |
| `crop_name` | str | Crop method used ("full" or "content") |

## Package Structure

```
watermark_detect/
    __init__.py      Exports: Detector, DetectResult, AnalysisReport, ...
    __main__.py      CLI entry point
    detector.py      Detector class with cached template banks
    rs.py            GF(2^8) arithmetic + RS(30,14) encoder/decoder + CRC-16
    layout.py        Tile geometry, pilot/message indices, permutations, sign masks
    keys.py          SplitMix64, HKDF-SHA256, key derivation, build ID utilities
    pn.py            Vectorized PN template generation (numpy uint64 batch ops)
    preprocess.py    Image loading, crop/scale candidates, high-pass filter
    sync.py          Pilot-based synchronization search + tile class scoring
    evidence.py      Message evidence aggregation + soft-decision RS decode
    video.py         Multi-frame bit evidence accumulation
```

### Module Dependencies

```
detector
  |-- preprocess -- layout
  |-- sync ------- layout, preprocess
  |-- evidence --- layout, rs, keys
  |-- video ------ layout
  |-- keys
  +-- pn --------- keys, layout
```

`keys` is a leaf module (no internal deps). No circular dependencies.

## Detection Pipeline

```
Input Image
  |
  v
[Preprocess] --> crop candidates x scale candidates x preprocessing
  |
  v
[Sync Search] --> coarse grid (step=8) + refinement (radius=2)
  |                pilot correlation via np.abs (handles both polarities)
  v
[Evidence]    --> per-tile message block correlation + median aggregation
  |                inverse permutation + sign mask unflip
  v
[Soft Decode] --> hard decision + bit-flip search (16 candidates, up to 5 flips)
  |                RS(30,14) error correction (up to 8 byte errors)
  v
[Payload]     --> trace_id (6B) + session_time (4B) + build_id (2B) + CRC-16 (2B)
```

## Watermark Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| Block size | 32x32 px | PN spread-spectrum chip |
| Tile size | 512x512 px | 16x16 blocks (16 pilot + 240 message) |
| Pilot blocks | 16 per tile | Fixed positions for synchronization |
| Message blocks | 240 per tile | RS-coded payload bits |
| RS code | RS(30,14) | 14 data bytes, 16 parity, corrects 8 byte errors |
| Payload | 14 bytes | trace_id(6) + session(4) + build(2) + crc(2) |
| Tile classes | 4 (A/B/C/D) | 2x2 repeating pattern with distinct permutations |
| kMessageDelta | 3 | RGB additive perturbation for message blocks |
| kPilotDelta | 4 | RGB additive perturbation for pilot blocks |

## Encoding

The C++ encoder (`src/Watermark.cpp`) renders the watermark via CKRenderContext
with two-pass additive/subtractive blending:

- **Pass 1** (ADD): `output = background + delta` for positive PN pixels
- **Pass 2** (REVSUBTRACT): `output = background - delta` for negative PN pixels

Delta is encoded in the RGB channels (not alpha) with `SRCBLEND=ONE` so that
LINEAR texture filtering correctly interpolates the contribution at sub-pixel
boundaries. This produces uniform +/-delta perturbation regardless of
background luminance.

## Testing

```bash
# Run detector tests
python -m pytest tests/test_watermark_detect.py -v

# Run video accumulation tests
python -m pytest tests/test_watermark_detect_video.py -v
```
