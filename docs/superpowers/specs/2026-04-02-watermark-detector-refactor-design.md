# Watermark Detector Refactor Design

**Status:** Implemented. This document reflects the final state after implementation.

## Problem

The Python offline detector (`tools/watermark_detect*.py`) was 4 flat files totaling ~1800 lines with tangled responsibilities, no state management, untyped dict return values, and several correctness bugs in the sync search and RS decoder.

## Solution

### Package Structure

```
tools/watermark_detect/
├── __init__.py          # Exports: Detector, DetectResult, AnalysisReport, SyncInfo, EvidenceInfo
├── __main__.py          # CLI entry point (python -m tools.watermark_detect)
├── rs.py                # GF(2^8) tables + RS(30,14) encode/decode + CRC-16
├── layout.py            # Tile constants, permutations, sign masks, VISIBLE_BLOCK_THRESHOLD
├── keys.py              # HKDF-SHA256, SplitMix64/mix64, key derivation, build_id utils
├── pn.py                # Vectorized PN template generation + template bank precomputation
├── preprocess.py        # Image loading, crop candidates, scale candidates, high-pass filter
├── sync.py              # Pilot sync search, tile class scoring, tile origin refinement
├── evidence.py          # Message evidence aggregation, soft-decision decode, payload parsing
├── video.py             # Multi-frame bit evidence accumulation
└── detector.py          # Detector class: unified detection pipeline
```

### Module Dependency Graph

```
detector.py
├── preprocess.py ─── layout.py
├── sync.py ────────── layout.py, preprocess.py
├── evidence.py ────── layout.py, rs.py, keys.py
├── video.py ───────── layout.py
├── keys.py
└── pn.py ──────────── keys.py, layout.py
```

`keys.py` is a leaf module. `layout.py` depends on `keys.py` (for `mix64`). No circular dependencies.

### Detector Class

```python
class Detector:
    def __init__(self, master_key_hex: str, *,
                 build_ids: list[int | str] | None = None,
                 exhaustive: bool = False):
        """Derive keys and precompute template banks once."""

    def detect(self, image) -> DetectResult | None:
        """Single-frame detection."""
        return self.analyze(image).decoded

    def detect_sequence(self, frames) -> DetectResult | None:
        """Multi-frame detection with evidence accumulation."""
        return self.analyze_sequence(frames).decoded

    def analyze(self, image) -> AnalysisReport:
        """Full analysis with diagnostics, even if decode fails."""

    def analyze_sequence(self, frames) -> AnalysisReport:
        """Multi-frame analysis with diagnostics."""
```

Results use frozen dataclasses: `DetectResult`, `SyncInfo`, `EvidenceInfo`, `AnalysisReport`. `EvidenceInfo` contains a `sync: SyncInfo` field to avoid field duplication.

### Key Design Decisions

**`derive_v2_keys` parameterized signature.** To keep `keys.py` as a leaf module, `derive_v2_keys` accepts `tile_classes`, `message_seed_offsets`, `sync_seed_offsets` as parameters. These constants live in `layout.py` and are passed by the `Detector` class.

**`VISIBLE_BLOCK_THRESHOLD` in `layout.py`.** Shared constant (0.25) used by both `sync.py` and `evidence.py`. Single source of truth.

**Pilot correlation uses `np.abs`.** `_compute_pilot_correlation_maps` uses `np.abs(correlations)` instead of `np.maximum(correlations, 0.0)`. Pilot blocks (codedBit=0) produce anti-correlated patterns (negative correlation). Clipping negatives discarded the pilot signal entirely.

**Soft decode parameters.** `DEFAULT_SOFT_BITS=16`, `DEFAULT_SOFT_FLIPS=5` (increased from 12/3 for better error recovery).

**Early termination.** Sync candidates with `pilot_score < 0.1` are skipped to avoid wasting time on invalid scale/crop combinations.

### CLI

```
python -m tools.watermark_detect image.png --master-key HEX [--build-id ID] [--search]
```

### Tests

Tests import from submodules directly:
```python
from tools.watermark_detect import Detector, DetectResult
from tools.watermark_detect.rs import rs_encode, rs_decode
from tools.watermark_detect.layout import BLOCK_SIZE, TILE_CLASSES, ...
```
Detection tests use `Detector(key).detect(image)`.
