"""Detector class: unified watermark detection pipeline."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np

from .keys import derive_v2_keys, normalize_build_ids
from .layout import (
    BLOCK_SIZE,
    MESSAGE_SEED_OFFSETS,
    NUM_MESSAGE_BLOCKS,
    NUM_PILOT_BLOCKS,
    SYNC_SEED_OFFSETS,
    TILE_CLASSES,
)
from .pn import precompute_v2_template_bank
from .preprocess import (
    build_crop_candidates,
    build_preprocessed_candidates,
    build_scale_candidates,
    prepare_block_source,
    resize_gray,
    _load_gray_array,
)
from .sync import find_v2_sync_candidates, refine_sync_candidate_tile_origin
from .evidence import aggregate_v2_message_evidence, try_decode_soft
from .video import accumulate_bit_evidence


@dataclass(frozen=True)
class DetectResult:
    trace_id: str
    trace_source: str
    session_time: str
    build_id: int
    build_version: str
    confidence: float
    pilot_score: float
    pilot_confidence: float
    scale: float
    offset: tuple[int, int]
    tile_phase: tuple[int, int]
    tile_origin: tuple[int, int]
    crop_name: str
    crop_bounds: tuple[int, int, int, int]
    preprocess: str
    coded_bytes: list[int]


@dataclass
class SyncInfo:
    build_id: int
    offset: tuple[int, int]
    pilot_score: float
    pilot_confidence: float
    tile_phase: tuple[int, int]
    scale: float
    crop_name: str
    crop_bounds: tuple[int, int, int, int]
    preprocess: str


@dataclass
class EvidenceInfo:
    sync: SyncInfo
    correlations: np.ndarray
    reliability: np.ndarray
    weights: np.ndarray
    counts: np.ndarray
    confidence: float
    tile_origin: tuple[int, int]


@dataclass
class AnalysisReport:
    decoded: DetectResult | None
    best_sync: SyncInfo | None
    best_evidence: EvidenceInfo | None


def _make_sync_info(sync_candidate, build_id: int, scale: float,
                    crop_candidate: dict, preprocess_name: str) -> SyncInfo:
    return SyncInfo(
        build_id=build_id,
        offset=sync_candidate.offset,
        pilot_score=sync_candidate.pilot_score,
        pilot_confidence=sync_candidate.pilot_confidence,
        tile_phase=sync_candidate.tile_phase,
        scale=scale,
        crop_name=crop_candidate["name"],
        crop_bounds=crop_candidate["bounds"],
        preprocess=preprocess_name,
    )


def _make_evidence_info(sync_info: SyncInfo, evidence: dict, tile_origin: tuple[int, int]) -> EvidenceInfo:
    return EvidenceInfo(
        sync=sync_info,
        correlations=evidence["correlations"],
        reliability=evidence["reliability"],
        weights=evidence["weights"],
        counts=evidence["counts"],
        confidence=evidence["confidence"],
        tile_origin=tile_origin,
    )


def _make_detect_result(decoded: dict, evidence_info: EvidenceInfo, coded_bytes: list[int]) -> DetectResult:
    return DetectResult(
        trace_id=decoded["trace_id"],
        trace_source=decoded["trace_source"],
        session_time=decoded["session_time"],
        build_id=decoded["build_id"],
        build_version=decoded["build_version"],
        confidence=evidence_info.confidence,
        pilot_score=evidence_info.sync.pilot_score,
        pilot_confidence=evidence_info.sync.pilot_confidence,
        scale=evidence_info.sync.scale,
        offset=evidence_info.sync.offset,
        tile_phase=evidence_info.sync.tile_phase,
        tile_origin=evidence_info.tile_origin,
        crop_name=evidence_info.sync.crop_name,
        crop_bounds=evidence_info.sync.crop_bounds,
        preprocess=evidence_info.sync.preprocess,
        coded_bytes=coded_bytes,
    )


def _is_better_evidence(candidate: EvidenceInfo, current: EvidenceInfo | None) -> bool:
    if current is None:
        return True
    if candidate.sync.pilot_score > current.sync.pilot_score:
        return True
    if (candidate.sync.pilot_score == current.sync.pilot_score
            and candidate.confidence > current.confidence):
        return True
    return False


class Detector:
    def __init__(self, master_key_hex: str, *,
                 build_ids: list[int | str] | None = None,
                 exhaustive: bool = False):
        self._master_key_hex = master_key_hex
        self._exhaustive = exhaustive
        self._build_ids = normalize_build_ids(build_ids)
        self._derived: dict[int, dict] = {}
        self._pilot_banks: dict[int, dict] = {}
        self._message_banks: dict[int, dict] = {}

        for bid in self._build_ids:
            derived = derive_v2_keys(master_key_hex, bid, TILE_CLASSES,
                                     MESSAGE_SEED_OFFSETS, SYNC_SEED_OFFSETS)
            self._derived[bid] = derived
            self._pilot_banks[bid] = precompute_v2_template_bank(derived, "sync_seeds", NUM_PILOT_BLOCKS)
            self._message_banks[bid] = precompute_v2_template_bank(derived, "message_seeds", NUM_MESSAGE_BLOCKS)

    def detect(self, image: np.ndarray | str | Path) -> DetectResult | None:
        return self.analyze(image).decoded

    def detect_sequence(self, frames: list[np.ndarray | str | Path]) -> DetectResult | None:
        return self.analyze_sequence(frames).decoded

    def analyze(self, image: np.ndarray | str | Path) -> AnalysisReport:
        gray = _load_gray_array(image)
        best_sync: SyncInfo | None = None
        best_evidence: EvidenceInfo | None = None

        crop_candidates = build_crop_candidates(gray)
        crop_candidates.sort(key=lambda item: (0 if item["name"] == "content" else 1, item["priority"]))

        for build_id in self._build_ids:
            pilot_bank = self._pilot_banks[build_id]
            message_bank = self._message_banks[build_id]

            for crop_candidate in crop_candidates:
                scale_candidates = build_scale_candidates(self._exhaustive, crop_candidate["gray"].shape)
                for scale in scale_candidates:
                    scaled_gray = resize_gray(crop_candidate["gray"], scale)
                    if scaled_gray.shape[0] < BLOCK_SIZE or scaled_gray.shape[1] < BLOCK_SIZE:
                        continue

                    prefer_highpass = scale >= 1.75
                    for preprocess_name, prepared_gray in build_preprocessed_candidates(
                        scaled_gray, prefer_highpass=prefer_highpass,
                    ):
                        block_source = prepare_block_source(prepared_gray)
                        sync_candidates = find_v2_sync_candidates(
                            block_source, pilot_bank, exhaustive=self._exhaustive,
                        )

                        if sync_candidates and sync_candidates[0].pilot_score < 0.1:
                            continue

                        for sync_index, sync_candidate in enumerate(sync_candidates):
                            sync_info = _make_sync_info(
                                sync_candidate, build_id, scale, crop_candidate, preprocess_name,
                            )
                            if best_sync is None or sync_info.pilot_score > best_sync.pilot_score:
                                best_sync = sync_info

                            direct_evidence = aggregate_v2_message_evidence(sync_candidate, message_bank)
                            if np.any(direct_evidence["counts"] > 0):
                                ev_info = _make_evidence_info(sync_info, direct_evidence, sync_candidate.tile_origin)
                                if _is_better_evidence(ev_info, best_evidence):
                                    best_evidence = ev_info

                                decoded, coded_bytes = try_decode_soft(
                                    direct_evidence["correlations"],
                                    reliabilities=direct_evidence["reliability"],
                                )
                                if decoded is not None:
                                    result = _make_detect_result(decoded, ev_info, coded_bytes)
                                    return AnalysisReport(decoded=result, best_sync=best_sync, best_evidence=best_evidence)

                            if sync_index >= 8 or sync_candidate.tile_origin != (0, 0):
                                continue

                            for variant in refine_sync_candidate_tile_origin(
                                sync_candidate, pilot_bank, limit=4,
                            ):
                                if variant.tile_origin == (0, 0):
                                    continue

                                evidence = aggregate_v2_message_evidence(variant, message_bank)
                                if not np.any(evidence["counts"] > 0):
                                    continue

                                var_sync = _make_sync_info(
                                    variant, build_id, scale, crop_candidate, preprocess_name,
                                )
                                var_ev = _make_evidence_info(var_sync, evidence, variant.tile_origin)
                                if _is_better_evidence(var_ev, best_evidence):
                                    best_evidence = var_ev

                                decoded, coded_bytes = try_decode_soft(
                                    evidence["correlations"],
                                    reliabilities=evidence["reliability"],
                                )
                                if decoded is not None:
                                    result = _make_detect_result(decoded, var_ev, coded_bytes)
                                    return AnalysisReport(decoded=result, best_sync=best_sync, best_evidence=best_evidence)

        return AnalysisReport(decoded=None, best_sync=best_sync, best_evidence=best_evidence)

    def analyze_sequence(self, frames: list[np.ndarray | str | Path]) -> AnalysisReport:
        frame_evidences: list[EvidenceInfo] = []
        decoded_results: list[DetectResult] = []
        best_sync: SyncInfo | None = None

        for frame in frames:
            report = self.analyze(frame)
            if report.best_sync is not None:
                if best_sync is None or report.best_sync.pilot_score > best_sync.pilot_score:
                    best_sync = report.best_sync
            if report.best_evidence is not None:
                frame_evidences.append(report.best_evidence)
            if report.decoded is not None:
                decoded_results.append(report.decoded)

        if frame_evidences:
            evidence_dicts = [
                {
                    "correlations": ev.correlations,
                    "reliability": ev.reliability,
                    "weights": ev.weights,
                    "counts": ev.counts,
                    "confidence": ev.confidence,
                    "pilot_score": ev.sync.pilot_score,
                }
                for ev in frame_evidences
            ]
            accumulated = accumulate_bit_evidence(evidence_dicts)
            decoded, coded_bytes = try_decode_soft(
                accumulated["correlations"],
                reliabilities=accumulated["reliability"],
            )
            if decoded is not None:
                best_ev = max(frame_evidences, key=lambda e: (e.sync.pilot_score, e.confidence))
                result = _make_detect_result(decoded, best_ev, coded_bytes)
                return AnalysisReport(decoded=result, best_sync=best_sync, best_evidence=best_ev)

        if decoded_results:
            best = max(decoded_results, key=lambda r: (r.pilot_score, r.confidence))
            return AnalysisReport(decoded=best, best_sync=best_sync, best_evidence=None)

        best_ev = max(frame_evidences, key=lambda e: (e.sync.pilot_score, e.confidence)) if frame_evidences else None
        return AnalysisReport(decoded=None, best_sync=best_sync, best_evidence=best_ev)
