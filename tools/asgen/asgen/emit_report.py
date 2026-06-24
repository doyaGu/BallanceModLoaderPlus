from __future__ import annotations

from .handwritten import hand_written_global_count, hand_written_object_method_count
from .model import FunctionBinding
from .policy import HEADER


def skip_category(reason: str) -> str:
    if reason in {
        "non-Imgui namespace",
        "imgui_internal",
        "context/frame lifecycle is owned by BML",
        "platform lifecycle is owned by BML",
        "debug tooling is not part of BML ImGui script surface",
        "not available in BML's compiled ImGui surface",
        "templated",
    }:
        return "deliberate omit"
    if reason.startswith("removed non-trailing default") or reason.startswith("unsupported mutable char buffer"):
        return "needs wrapper"
    if reason.startswith("unsupported parameter") or reason == "unsupported return type" or reason == "varargs":
        return "unsupported raw ABI"
    return "other"


def emit_report(ctx, functions: list[FunctionBinding]) -> str:
    out = [HEADER, "# BML ImGui AngelScript Generated Binding Report\n\n"]
    skipped = [(name, reason) for name, reason in ctx.report if reason != "hand-written overload"]
    handwritten = [(name, reason) for name, reason in ctx.report if reason == "hand-written overload"]
    total_seen = len(functions) + len(skipped) + len(handwritten)
    registered_total = len(functions) + hand_written_global_count()
    coverage = (registered_total / total_seen * 100.0) if total_seen else 0.0
    out.append(f"- Registered generated functions: {len(functions)}\n")
    out.append(f"- Registered hand-written overloads: {hand_written_global_count()}\n")
    out.append(f"- Registered hand-written object methods: {hand_written_object_method_count()}\n")
    out.append(f"- Registered global functions total: {registered_total}\n")
    out.append(f"- Hand-written source symbols: {len(handwritten)}\n")
    out.append(f"- Skipped symbols: {len(skipped)}\n")
    out.append(f"- Source symbols scanned: {total_seen}\n")
    out.append(f"- Script global function coverage estimate: {coverage:.1f}%\n")
    out.append(f"- Enum types referenced by registered functions: {len(ctx.used_enums)}\n\n")
    out.append("## Hand-Written Symbols\n\n")
    for name, _ in sorted(handwritten):
        out.append(f"- `{name}`: hand-written overload\n")
    out.append("\n")
    out.append("## Skip Reason Summary\n\n")
    summary: dict[str, int] = {}
    for _, reason in skipped:
        summary[reason] = summary.get(reason, 0) + 1
    for reason, count in sorted(summary.items(), key=lambda item: (-item[1], item[0])):
        out.append(f"- {count}: {reason}\n")
    out.append("\n")
    out.append("## Skip Categories\n\n")
    category_summary: dict[str, int] = {}
    for _, reason in skipped:
        category = skip_category(reason)
        category_summary[category] = category_summary.get(category, 0) + 1
    for category in ["deliberate omit", "needs wrapper", "unsupported raw ABI", "other"]:
        out.append(f"- {category}: {category_summary.get(category, 0)}\n")
    out.append("\n")
    out.append("## Needs Script-Friendly Wrapper\n\n")
    wrapper_gaps = [(name, reason) for name, reason in skipped if skip_category(reason) == "needs wrapper"]
    if wrapper_gaps:
        for name, reason in wrapper_gaps:
            out.append(f"- `{name}`: {reason}\n")
    else:
        out.append("- None\n")
    out.append("\n")
    out.append("## Unsupported Raw ABI\n\n")
    gap_reasons = ("unsupported parameter ", "unsupported return type", "removed non-trailing default", "unsupported mutable char buffer")
    gaps = [(name, reason) for name, reason in skipped if skip_category(reason) == "unsupported raw ABI" or reason.startswith(gap_reasons)]
    if gaps:
        for name, reason in gaps:
            out.append(f"- `{name}`: {reason}\n")
    else:
        out.append("- None\n")
    out.append("\n")
    out.append("## Skipped Symbols\n\n")
    for name, reason in skipped:
        out.append(f"- `{name}`: {reason}\n")
    return "".join(out)
