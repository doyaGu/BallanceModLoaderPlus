#!/usr/bin/env python3
"""Audit reconstructed IVP public member methods against available evidence.

The nearby IVP tree is only a declaration/algorithm reference.  This tool does
not declare it compatible.  It parses declarations physically owned by its
explicitly public headers with an x86 MSVC target, excludes declarations inside
the source's editorial ``INTERN_START``/``INTERN_END`` fences, and compares the
remaining public methods with BML's reconstructed headers.  Independent output
columns then record retained retail bodies, adapter callsites, owner-layout
evidence, nearby definitions, deterministic host tests, and real Player tests.
Curated evidence is strict opt-in: absent ledger rows remain unclassified.
Declarations from internal headers pulled in only to make the aggregate compile
are excluded.
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import re
import shutil
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Collection, Iterable


METHOD_KINDS = {
    "CXXMethodDecl",
    "CXXConstructorDecl",
    "CXXDestructorDecl",
}

# Public IVP utility headers also expose the historical P_String/P_List and
# IVV_* helper families.  Limiting the denominator to IVP_/IVE_ silently hid
# those source-level APIs even though the header ownership filter already
# excludes unrelated dependency declarations.
PUBLIC_OWNER_PREFIXES = ("IVP_", "IVE_", "IVV_", "P_")


def is_public_owner_name(value: str) -> bool:
    return value.startswith(PUBLIC_OWNER_PREFIXES)

# These headers contain platform-selected implementations or lack include
# guards.  Their declarations are already present through ivp_physics.hxx.
REFERENCE_AGGREGATE_EXCLUSIONS = {
    "ivp_physics.hxx",
    "ivu_linear_double.hxx",
    "ivu_linear_macros.hxx",
    "ivu_linear_software.hxx",
    "ivu_linear_willamette.hxx",
    "ivu_string.hxx",
}

# This is tagged public in the nearby SDK because it supports its standalone
# demos, but it is a graphics/example bridge rather than physics_RT's IVP API.
REFERENCE_PRODUCT_EXCLUSIONS = {
    "ive_graphics.hxx",
}

RETAIL_MANIFEST_PATH = Path("src/IVP/generated/IvpSymbols.inc")
RETAIL_ADDRESS_PATH = Path("include/BML/IVP/detail/AddressEntries.inc")


@dataclass(frozen=True)
class Method:
    owner: str
    name: str
    signature: str
    mangled: str
    has_body: bool
    pure_virtual: bool
    virtual: bool
    override: bool = False
    deleted: bool = False
    retail_invoke: bool = False
    retail_address_ids: tuple[str, ...] = ()

    @property
    def key(self) -> tuple[str, str, str]:
        return self.owner, self.name, self.signature


@dataclass(frozen=True)
class DirectBase:
    owner: str
    arguments: tuple[str, ...] = ()


EVIDENCE_KINDS = {
    "layout-retail",
    "layout-ida-only",
    "layout-not-applicable",
    "nearby-source-basis",
    "host-test",
    "ballance-player",
    "retail-body-variant",
    "retail-inline-body",
    "retail-complete-body",
    "retail-constructor-reconstruction",
    "retail-destructor-thunk",
    "retail-destructor-reconstruction",
    "retail-indirect-callsite",
    "ida-virtual-variant",
    "retail-virtual-variant",
    "retail-vtable-variant",
    "retail-inheritance-variant",
    "retail-signature-variant",
    "retail-interface-absent",
    "retail-structure-absent",
}


@dataclass(frozen=True)
class CuratedEvidence:
    owner: str
    method: str
    mangled: str
    kind: str
    source: str
    note: str


@dataclass(frozen=True)
class EvidenceColumns:
    retail_body: str
    retail_callsite: str
    owner_layout: str
    nearby_header_definition: bool
    nearby_source_basis: bool
    host_behavior_test: bool
    ballance_player_test: bool
    sources: str


def normalize_signature(value: str) -> str:
    value = re.sub(r"\s+", " ", value.strip())
    # Clang may retain an elaborated type specifier from one declaration but
    # omit it from another. `class T *`, `struct T *`, and `T *` denote the
    # same C++ parameter type and cannot be an ABI/signature difference.
    value = re.sub(r"\b(?:class|struct|enum)\s+", "", value)
    value = re.sub(r"\s*([*&(),])\s*", r"\1", value)
    # Defaulted C++17 constructors acquire an implicit noexcept that is not
    # part of the older source declaration or the x86 decorated name.
    return re.sub(r"\s*noexcept$", "", value)


def normalize_path(path: Path | str) -> str:
    return str(Path(path).resolve()).replace("\\", "/").casefold()


def spelling_location(location: dict) -> dict:
    while isinstance(location, dict):
        nested = location.get("spellingLoc")
        if not isinstance(nested, dict):
            break
        location = nested
    return location


INTERNAL_SECTION_MARKER = re.compile(rb"//\s*INTERN_(START|END)")
PUBLIC_EXPORT_MARKER = re.compile(r"//\s*IVP_EXPORT_PUBLIC\b")


def source_offset(location: dict, contents: bytes) -> int | None:
    offset = location.get("offset")
    if isinstance(offset, int):
        return offset

    line = location.get("line")
    column = location.get("col")
    if not isinstance(line, int) or line < 1:
        return None
    lines = contents.splitlines(keepends=True)
    if line > len(lines):
        return None
    return sum(len(value) for value in lines[: line - 1]) + max(
        0, int(column or 1) - 1
    )


def location_is_internal(location: dict, contents: bytes) -> bool:
    """Honor the nearby SDK's editorial INTERN_START/END export fences."""
    offset = source_offset(location, contents)
    if offset is None:
        return False

    depth = 0
    for marker in INTERNAL_SECTION_MARKER.finditer(contents):
        if marker.start() >= offset:
            break
        if marker.group(1) == b"START":
            depth += 1
        elif depth > 0:
            depth -= 1
    return depth > 0


def owned_header(
    node: dict,
    allowed_headers: dict[str, tuple[Path, bytes]],
) -> tuple[Path, bytes] | None:
    """Return whether a record definition is physically in an allowed header.

    Clang's JSON AST omits a repeated ``loc.file`` value.  Falling back to the
    includer is incorrect: it would make internal dependency declarations look
    public.  For omitted filenames, match Clang's byte offset, line, and token
    against the actual selected header bytes instead.
    """

    location = spelling_location(node.get("loc", {}))
    explicit_file = location.get("file")
    if explicit_file:
        evidence = allowed_headers.get(normalize_path(explicit_file))
        if evidence is None or location_is_internal(location, evidence[1]):
            return None
        return evidence

    offset = location.get("offset")
    line = location.get("line")
    token_length = location.get("tokLen")
    owner = node.get("name", "").encode("latin-1", errors="strict")
    if offset is None or token_length != len(owner):
        return False

    matches = []
    for normalized, (path, contents) in allowed_headers.items():
        if contents[offset : offset + token_length] != owner:
            continue
        if line is not None and contents.count(b"\n", 0, offset) + 1 != line:
            continue
        if location_is_internal(location, contents):
            continue
        matches.append((normalized, path))

    if len(matches) > 1:
        candidates = ", ".join(str(path) for _, path in matches)
        raise RuntimeError(
            f"ambiguous source location for {node.get('name')}: {candidates}"
        )
    if not matches:
        return None
    return allowed_headers[matches[0][0]]


def record_is_owned_by(
    node: dict,
    allowed_headers: dict[str, tuple[Path, bytes]],
) -> bool:
    return owned_header(node, allowed_headers) is not None


def clang_ast(
    clang: Path,
    source: str,
    language_standard: str,
    include_directories: list[Path],
    extra_definitions: list[str] | None = None,
) -> dict:
    command = [
        str(clang),
        "--target=i686-pc-windows-msvc",
        "-x",
        "c++",
        f"-std={language_standard}",
        "-fms-extensions",
        # The Windows target otherwise enables MSVC-style delayed template
        # parsing. Its JSON AST replaces dependent template bodies with empty
        # placeholder nodes, making real in-header implementations look like
        # declarations. Parse them now so callability is auditable.
        "-fno-delayed-template-parsing",
        "-Wno-everything",
        "-ferror-limit=0",
        "-DWIN32",
        "-DNDEBUG",
        "-Xclang",
        "-ast-dump=json",
        "-fsyntax-only",
        "-",
    ]
    for definition in extra_definitions or []:
        command.insert(-5, f"-D{definition}")
    for directory in include_directories:
        command.insert(-5, f"-I{directory}")

    try:
        completed = subprocess.run(
            command,
            input=source,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            timeout=150,
        )
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(
            "clang AST extraction exceeded 150 seconds; the child process "
            "was terminated instead of leaving an audit hung"
        ) from error
    try:
        return json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        diagnostic = completed.stderr[-4000:]
        raise RuntimeError(
            f"clang did not produce a usable AST (exit {completed.returncode}):\n"
            f"{diagnostic}"
        ) from error


def extract_public_surface(
    ast: dict,
    declaration_headers: list[Path] | None = None,
) -> dict[str, set[Method]]:
    allowed_headers = None
    if declaration_headers is not None:
        allowed_headers = {
            normalize_path(path): (path, path.read_bytes())
            for path in declaration_headers
        }

    classes: dict[str, set[Method]] = defaultdict(set)
    stack = [ast]
    while stack:
        node = stack.pop()
        stack.extend(node.get("inner", []))
        if node.get("kind") != "CXXRecordDecl" or not node.get(
            "completeDefinition"
        ):
            continue

        owner = node.get("name", "")
        if not is_public_owner_name(owner):
            continue
        owner_header = None
        if allowed_headers is not None:
            owner_header = owned_header(node, allowed_headers)
            if owner_header is None:
                continue

        access = "private" if node.get("tagUsed") == "class" else "public"
        classes.setdefault(owner, set())
        for child in node.get("inner", []):
            if child.get("kind") == "AccessSpecDecl":
                access = child.get("access", access)
                continue
            if (
                access != "public"
                or child.get("kind") not in METHOD_KINDS
                or child.get("isImplicit")
            ):
                continue
            if owner_header is not None and location_is_internal(
                spelling_location(child.get("loc", {})), owner_header[1]
            ):
                continue

            methods = child.get("inner", [])
            method_name = child.get("name", "")
            if child.get("kind") == "CXXConstructorDecl":
                method_name = owner
            elif child.get("kind") == "CXXDestructorDecl":
                method_name = f"~{owner}"
            classes[owner].add(
                Method(
                    owner=owner,
                    name=method_name,
                    signature=normalize_signature(
                        child.get("type", {}).get("qualType", "")
                    ),
                    mangled=child.get("mangledName", ""),
                    has_body=(
                        child.get("explicitlyDefaulted") == "default"
                        or any(
                            nested.get("kind") == "CompoundStmt"
                            for nested in methods
                        )
                    ),
                    pure_virtual=bool(child.get("pure")),
                    # Clang's JSON marks an initially-declared virtual with
                    # ``virtual`` but commonly represents an overriding
                    # declaration only with an OverrideAttr child.  Both
                    # occupy a vtable slot and therefore have identical ABI
                    # significance here.
                    virtual=(
                        bool(child.get("virtual"))
                        or any(
                            nested.get("kind") == "OverrideAttr"
                            for nested in methods
                        )
                    ),
                    override=any(
                        nested.get("kind") == "OverrideAttr"
                        for nested in methods
                    ),
                    deleted=bool(child.get("explicitlyDeleted")),
                    retail_invoke=invokes_retail_abi(child),
                    retail_address_ids=retail_abi_address_ids(child),
                )
            )
    return classes


def walk_ast_nodes(root: dict) -> Iterable[dict]:
    stack = [root]
    while stack:
        node = stack.pop()
        yield node
        stack.extend(node.get("inner", []))


def invokes_retail_abi(root: dict) -> bool:
    names = {"Invoke", "InvokeThis", "InvokeThisOr"}
    return any(
        node.get("name") in names
        or node.get("referencedDecl", {}).get("name") in names
        or node.get("foundReferencedDecl", {}).get("name") in names
        for node in walk_ast_nodes(root)
    )


def retail_abi_address_ids(root: dict) -> tuple[str, ...]:
    """Return the concrete ABI::Address enumerators referenced by a body.

    Merely finding Invoke/InvokeThis is too broad: constructors commonly call
    a retail allocator or a retained base/helper while deliberately rebuilding
    their own stripped/incompatible complete-constructor layer. Keep the exact
    address identities so the audit can distinguish those cases from a call to
    the method's own retail body.
    """
    return tuple(sorted({
        node.get("referencedDecl", {}).get("name", "")
        for node in walk_ast_nodes(root)
        if node.get("kind") == "DeclRefExpr"
        and node.get("type", {}).get("qualType")
        == "BML::IVP::ABI::Address"
        and node.get("referencedDecl", {}).get("name")
    }))


def defined_method_implementations(
    ast: dict,
) -> dict[str, tuple[bool, tuple[str, ...]]]:
    """Return methods whose definition may live outside the class body.

    Clang leaves the declaration nested in ``CXXRecordDecl`` body-less when an
    inline definition appears later in the same aggregate header.  The
    top-level definition carries the same mangled name and a ``CompoundStmt``;
    joining those two views avoids misclassifying real SDK wrappers as
    declaration-only.
    """
    result: dict[str, bool] = {}
    stack = [ast]
    while stack:
        node = stack.pop()
        nested = node.get("inner", [])
        stack.extend(nested)
        mangled = node.get("mangledName", "")
        if (
            node.get("kind") in METHOD_KINDS
            and mangled
            and any(child.get("kind") == "CompoundStmt" for child in nested)
        ):
            result[mangled] = (
                invokes_retail_abi(node),
                retail_abi_address_ids(node),
            )
    return result


def apply_out_of_class_definitions(
    surface: dict[str, set[Method]], ast: dict
) -> dict[str, set[Method]]:
    definitions = defined_method_implementations(ast)
    return {
        owner: {
            replace(
                method,
                has_body=True,
                retail_invoke=definitions[method.mangled][0],
                retail_address_ids=definitions[method.mangled][1],
            )
            if method.mangled in definitions
            else method
            for method in methods
        }
        for owner, methods in surface.items()
    }


def extract_declared_methods(
    ast: dict,
    declaration_headers: list[Path] | None = None,
) -> dict[str, list[Method]]:
    """Return explicit methods in declaration order, regardless of access.

    Public API completeness is access-sensitive, but vtable slot assignment is
    not. In particular, Ballance's IVP_Real_Object inserts a protected virtual
    before a public one. Keeping this second ordered view prevents a set-based
    method audit from missing that ABI distinction.
    """
    allowed_headers = None
    if declaration_headers is not None:
        allowed_headers = {
            normalize_path(path): (path, path.read_bytes())
            for path in declaration_headers
        }

    classes: dict[str, list[Method]] = {}
    stack = [ast]
    while stack:
        node = stack.pop()
        stack.extend(reversed(node.get("inner", [])))
        if node.get("kind") != "CXXRecordDecl" or not node.get(
            "completeDefinition"
        ):
            continue
        owner = node.get("name", "")
        if not is_public_owner_name(owner):
            continue
        owner_header = None
        if allowed_headers is not None:
            owner_header = owned_header(node, allowed_headers)
            if owner_header is None:
                continue

        declared: list[Method] = []
        for child in node.get("inner", []):
            if child.get("kind") not in METHOD_KINDS or child.get("isImplicit"):
                continue
            if owner_header is not None and location_is_internal(
                spelling_location(child.get("loc", {})), owner_header[1]
            ):
                continue
            nested = child.get("inner", [])
            method_name = child.get("name", "")
            if child.get("kind") == "CXXConstructorDecl":
                method_name = owner
            elif child.get("kind") == "CXXDestructorDecl":
                method_name = f"~{owner}"
            declared.append(
                Method(
                    owner=owner,
                    name=method_name,
                    signature=normalize_signature(
                        child.get("type", {}).get("qualType", "")
                    ),
                    mangled=child.get("mangledName", ""),
                    has_body=(
                        child.get("explicitlyDefaulted") == "default"
                        or any(
                            value.get("kind") == "CompoundStmt"
                            for value in nested
                        )
                    ),
                    pure_virtual=bool(child.get("pure")),
                    virtual=(
                        bool(child.get("virtual"))
                        or any(
                            value.get("kind") == "OverrideAttr"
                            for value in nested
                        )
                    ),
                    override=any(
                        value.get("kind") == "OverrideAttr"
                        for value in nested
                    ),
                    deleted=bool(child.get("explicitlyDeleted")),
                )
            )

        previous = classes.get(owner)
        if previous is not None and previous != declared:
            raise RuntimeError(
                f"multiple incompatible definitions found for {owner}"
            )
        classes[owner] = declared
    return classes


def normalize_base_owner(value: str) -> str:
    value = re.sub(r"\b(?:class|struct)\s+", "", value).strip()
    # The audit groups class-template declarations by their public template
    # name, so an instantiated base must be mapped back to that owner.
    return value.split("<", 1)[0].strip()


def split_template_arguments(value: str) -> tuple[str, ...]:
    start = value.find("<")
    if start < 0 or not value.rstrip().endswith(">"):
        return ()
    arguments: list[str] = []
    depth = 0
    begin = start + 1
    for offset, character in enumerate(value[begin:], begin):
        if character == "<":
            depth += 1
        elif character == ">":
            if depth == 0:
                tail = value[begin:offset].strip()
                if tail:
                    arguments.append(tail)
                break
            depth -= 1
        elif character == "," and depth == 0:
            arguments.append(value[begin:offset].strip())
            begin = offset + 1
    return tuple(arguments)


def extract_template_parameters(ast: dict) -> dict[str, tuple[str, ...]]:
    result: dict[str, tuple[str, ...]] = {}
    stack = [ast]
    while stack:
        node = stack.pop()
        stack.extend(node.get("inner", []))
        if node.get("kind") != "ClassTemplateDecl":
            continue
        owner = node.get("name", "")
        if not is_public_owner_name(owner):
            continue
        parameters = tuple(
            child.get("name", "")
            for child in node.get("inner", [])
            if child.get("kind") in {
                "TemplateTypeParmDecl",
                "NonTypeTemplateParmDecl",
            }
            and child.get("name")
        )
        if parameters:
            result[owner] = parameters
    return result


def extract_direct_bases(
    ast: dict,
    declaration_headers: list[Path] | None = None,
) -> dict[str, tuple[DirectBase, ...]]:
    allowed_headers = None
    if declaration_headers is not None:
        allowed_headers = {
            normalize_path(path): (path, path.read_bytes())
            for path in declaration_headers
        }

    result: dict[str, tuple[DirectBase, ...]] = {}
    stack = [ast]
    while stack:
        node = stack.pop()
        stack.extend(node.get("inner", []))
        if node.get("kind") != "CXXRecordDecl" or not node.get(
            "completeDefinition"
        ):
            continue
        owner = node.get("name", "")
        if not is_public_owner_name(owner):
            continue
        if allowed_headers is not None and owned_header(
            node, allowed_headers
        ) is None:
            continue
        direct = []
        for base in node.get("bases", []):
            value = (
                base.get("type", {}).get("desugaredQualType")
                or base.get("type", {}).get("qualType", "")
            )
            direct.append(
                DirectBase(
                    normalize_base_owner(value), split_template_arguments(value)
                )
            )
        result[owner] = tuple(base for base in direct if base.owner)
    return result


def extract_public_bases(
    ast: dict,
    declaration_headers: list[Path] | None = None,
) -> dict[str, tuple[str, ...]]:
    return {
        owner: tuple(base.owner for base in bases)
        for owner, bases in extract_direct_bases(
            ast, declaration_headers
        ).items()
    }


def specialize_text(value: str, substitutions: dict[str, str]) -> str:
    for parameter, argument in substitutions.items():
        value = re.sub(rf"\b{re.escape(parameter)}\b", argument, value)
    return normalize_signature(value)


def direct_base_substitutions(
    owner: str,
    direct_bases: dict[str, tuple[DirectBase, ...]],
    template_parameters: dict[str, tuple[str, ...]],
    substitutions: dict[str, str] | None = None,
) -> Iterable[tuple[DirectBase, dict[str, str]]]:
    substitutions = substitutions or {}
    for base in direct_bases.get(owner, ()):
        arguments = tuple(
            specialize_text(argument, substitutions)
            for argument in base.arguments
        )
        yield base, dict(
            zip(template_parameters.get(base.owner, ()), arguments)
        )


def apply_effective_virtuals(
    surface: dict[str, set[Method]],
    bases: dict[str, tuple[str, ...]],
    direct_bases: dict[str, tuple[DirectBase, ...]] | None = None,
    template_parameters: dict[str, tuple[str, ...]] | None = None,
) -> dict[str, set[Method]]:
    """Infer virtual overrides omitted by old source spelling.

    The nearby IVP headers predate C++11 and usually omit ``virtual`` on an
    override.  Clang's JSON does not mark those declarations as virtual, so
    walk the public base graph and inherit virtuality by matching the function
    type.  A derived destructor is virtual whenever a base destructor is.
    """
    direct_bases = direct_bases or {
        owner: tuple(DirectBase(base) for base in values)
        for owner, values in bases.items()
    }
    template_parameters = template_parameters or {}
    cache: dict[tuple[str, str, str], bool] = {}
    visiting: set[tuple[str, str, str]] = set()

    def inherited_slot_is_virtual(
        owner: str,
        method: Method,
        seen_owners: set[tuple[str, tuple[tuple[str, str], ...]]],
        substitutions: dict[str, str] | None = None,
    ) -> bool:
        substitutions = substitutions or {}
        owner_key = (owner, tuple(sorted(substitutions.items())))
        if owner_key in seen_owners:
            return False
        seen_owners.add(owner_key)
        destructor = method.name == f"~{method.owner}"
        for candidate in surface.get(owner, ()):
            same_slot = (
                candidate.name == f"~{owner}"
                if destructor
                else candidate.name == method.name
                and specialize_text(
                    candidate.signature, substitutions
                ) == method.signature
            )
            if same_slot and is_effectively_virtual(candidate):
                return True
        return any(
            inherited_slot_is_virtual(
                base.owner, method, seen_owners, base_substitutions
            )
            for base, base_substitutions in direct_base_substitutions(
                owner,
                direct_bases,
                template_parameters,
                substitutions,
            )
        )

    def is_effectively_virtual(method: Method) -> bool:
        key = method.key
        if key in cache:
            return cache[key]
        if method.virtual:
            cache[key] = True
            return True
        if key in visiting:
            return False
        visiting.add(key)
        for base, substitutions in direct_base_substitutions(
            method.owner, direct_bases, template_parameters
        ):
            if inherited_slot_is_virtual(
                base.owner, method, set(), substitutions
            ):
                visiting.discard(key)
                cache[key] = True
                return True
        visiting.discard(key)
        cache[key] = False
        return False

    return {
        owner: {
            replace(method, virtual=is_effectively_virtual(method))
            for method in methods
        }
        for owner, methods in surface.items()
    }


def apply_effective_virtuals_ordered(
    surface: dict[str, list[Method]],
    bases: dict[str, tuple[str, ...]],
    direct_bases: dict[str, tuple[DirectBase, ...]] | None = None,
    template_parameters: dict[str, tuple[str, ...]] | None = None,
) -> dict[str, list[Method]]:
    """Ordered equivalent of apply_effective_virtuals for vtable analysis."""
    as_sets = {owner: set(methods) for owner, methods in surface.items()}
    resolved = apply_effective_virtuals(
        as_sets, bases, direct_bases, template_parameters
    )
    return {
        owner: [
            next(value for value in resolved[owner] if value.key == method.key)
            for method in methods
        ]
        for owner, methods in surface.items()
    }


def apply_effective_virtuals_with_support(
    candidate_surface: dict[str, set[Method]],
    support_surface: dict[str, set[Method]],
    bases: dict[str, tuple[str, ...]],
    direct_bases: dict[str, tuple[DirectBase, ...]] | None = None,
    template_parameters: dict[str, tuple[str, ...]] | None = None,
) -> dict[str, set[Method]]:
    """Resolve candidate virtuality against transitive support declarations.

    Product-marker filtering defines the public API denominator, but a marked
    derived header may inherit from an unmarked dependency header.  The latter
    must participate in override resolution without becoming a public API
    candidate itself.
    """
    resolved_support = apply_effective_virtuals(
        support_surface, bases, direct_bases, template_parameters
    )
    support_virtuals: dict[tuple[str, str, str], bool] = {}
    for methods in resolved_support.values():
        for method in methods:
            support_virtuals[method.key] = (
                support_virtuals.get(method.key, False) or method.virtual
            )
    return {
        owner: {
            replace(
                method,
                virtual=support_virtuals.get(method.key, method.virtual),
            )
            for method in methods
        }
        for owner, methods in candidate_surface.items()
    }


def apply_effective_virtuals_ordered_with_support(
    candidate_surface: dict[str, list[Method]],
    support_surface: dict[str, list[Method]],
    bases: dict[str, tuple[str, ...]],
    direct_bases: dict[str, tuple[DirectBase, ...]] | None = None,
    template_parameters: dict[str, tuple[str, ...]] | None = None,
) -> dict[str, list[Method]]:
    """Ordered projection of transitive effective-virtual information."""
    resolved = apply_effective_virtuals_with_support(
        {owner: set(methods) for owner, methods in candidate_surface.items()},
        {owner: set(methods) for owner, methods in support_surface.items()},
        bases,
        direct_bases,
        template_parameters,
    )
    virtuals: dict[tuple[str, str, str], bool] = {}
    for methods in resolved.values():
        for method in methods:
            virtuals[method.key] = (
                virtuals.get(method.key, False) or method.virtual
            )
    return {
        owner: [
            replace(method, virtual=virtuals.get(method.key, method.virtual))
            for method in methods
        ]
        for owner, methods in candidate_surface.items()
    }


def virtual_slot_identity(method: Method) -> tuple[str, str]:
    if method.name == f"~{method.owner}":
        return "~", "void()"
    return method.name, method.signature


def direct_new_virtual_slots(
    surface: dict[str, list[Method]],
    bases: dict[str, tuple[str, ...]],
    direct_bases: dict[str, tuple[DirectBase, ...]] | None = None,
    template_parameters: dict[str, tuple[str, ...]] | None = None,
) -> dict[str, tuple[tuple[str, str], ...]]:
    """Return each owner's newly appended virtual slots in source order.

    Overrides reuse an inherited slot and therefore do not participate in the
    append order. Multiple bases are all considered when deciding whether a
    declaration overrides an existing slot; their primary/secondary table
    order is separately protected by the direct-base order audit.
    """
    direct_bases = direct_bases or {
        owner: tuple(DirectBase(base) for base in values)
        for owner, values in bases.items()
    }
    template_parameters = template_parameters or {}
    inherited_cache: dict[
        tuple[str, tuple[tuple[str, str], ...]],
        set[tuple[str, str]],
    ] = {}

    def all_slots(
        owner: str,
        substitutions: dict[str, str],
        visiting: set[tuple[str, tuple[tuple[str, str], ...]]],
    ) -> set[tuple[str, str]]:
        owner_key = (owner, tuple(sorted(substitutions.items())))
        if owner_key in inherited_cache:
            return inherited_cache[owner_key]
        if owner_key in visiting:
            return set()
        visiting.add(owner_key)
        result: set[tuple[str, str]] = set()
        for base, base_substitutions in direct_base_substitutions(
            owner, direct_bases, template_parameters, substitutions
        ):
            result.update(
                all_slots(base.owner, base_substitutions, visiting)
            )
        for method in surface.get(owner, ()):
            if method.virtual:
                name, signature = virtual_slot_identity(method)
                result.add(
                    (name, specialize_text(signature, substitutions))
                )
        visiting.remove(owner_key)
        inherited_cache[owner_key] = result
        return result

    output = {}
    for owner, methods in surface.items():
        inherited: set[tuple[str, str]] = set()
        for base, substitutions in direct_base_substitutions(
            owner, direct_bases, template_parameters
        ):
            inherited.update(all_slots(base.owner, substitutions, set()))
        appended: list[tuple[str, str]] = []
        known = set(inherited)
        for method in methods:
            if not method.virtual:
                continue
            identity = virtual_slot_identity(method)
            if not method.override and identity not in known:
                appended.append(identity)
                known.add(identity)
        output[owner] = tuple(appended)
    return output


def find_reference_headers(reference_root: Path) -> list[Path]:
    headers = []
    for pattern in ("*.hxx", "*.h"):
        for path in reference_root.rglob(pattern):
            try:
                if (
                    path.name not in REFERENCE_PRODUCT_EXCLUSIONS
                    and PUBLIC_EXPORT_MARKER.search(
                        path.read_text(encoding="latin-1")
                    )
                ):
                    headers.append(path)
            except OSError as error:
                raise RuntimeError(f"cannot read reference header {path}: {error}")
    return sorted(set(headers))


def reference_source(headers: list[Path]) -> str:
    lines = [
        "#include <ivp_physics.hxx>",
        # Public headers in this source snapshot assume these internal
        # dependency headers were included by the product build.
        "#include <ivp_time_event.hxx>",
        "#include <ivp_car_system.hxx>",
        # Parse this multiple-inheritance declaration before malformed optional
        # Havok headers can put Clang into recovery mode. Otherwise the nearby
        # IVP_Listener_Set_Active<IVP_Core> base is silently dropped from the
        # JSON AST while the second base survives.
        "#include <ivp_forcefield.hxx>",
        # Likewise parse the public vector-derived Halfspacesoup before the
        # optional Havok/MOPP headers can put Clang into error recovery.  Its
        # methods survived the aggregate parse, but its template base did not.
        "#include <ivp_halfspacesoup.hxx>",
    ]
    for header in headers:
        if header.name not in REFERENCE_AGGREGATE_EXCLUSIONS:
            lines.append(f"#include <{header.name}>")
    return "\n".join(lines) + "\n"


def load_retail_manifest(path: Path) -> dict[str, int]:
    text = path.read_text(encoding="utf-8")
    symbols = {}
    pattern = re.compile(
        r'^\s*\{"((?:\\.|[^"\\])*)",\s*0x([0-9A-Fa-f]+)u?', re.M
    )
    for match in pattern.finditer(text):
        raw = match.group(1)
        name = raw.replace(r'\"', '"').replace(r"\\", "\\")
        symbols[name] = int(match.group(2), 16)
    if not symbols:
        raise RuntimeError(f"no symbols found in retail manifest {path}")
    return symbols


def load_retail_addresses(path: Path) -> dict[int, set[str]]:
    text = path.read_text(encoding="utf-8")
    result: dict[int, set[str]] = defaultdict(set)
    pattern = re.compile(
        r"^\s*([A-Za-z_]\w*)\s*=\s*0x([0-9A-Fa-f]+)u,", re.M
    )
    for match in pattern.finditer(text):
        result[int(match.group(2), 16)].add(match.group(1))
    if not result:
        raise RuntimeError(f"no retail addresses found in {path}")
    return result


def resolved_retail_symbol(
    reference: Method, retail_symbols: Collection[str] | dict[str, int]
) -> tuple[str, int | None] | None:
    """Resolve Clang's MS ABI destructor pseudo-name to a retail symbol.

    Clang's JSON AST emits ``??_D...@@QAEXXZ`` for destructor declarations.
    The Ballance MSVC image names complete destructors ``??1...@@QAE@XZ`` or
    ``??1...@@UAE@XZ``.  Restrict the fallback to the same owner and zero-arg
    destructor shape; never use it for deleting destructors or other methods.
    """
    if reference.mangled in retail_symbols:
        value = (
            retail_symbols[reference.mangled]
            if isinstance(retail_symbols, dict)
            else None
        )
        return reference.mangled, value
    if reference.name != f"~{reference.owner}":
        return None
    candidates = [
        f"??1{reference.owner}@@{access}AE@XZ"
        for access in ("Q", "U", "I")
        if f"??1{reference.owner}@@{access}AE@XZ" in retail_symbols
    ]
    if len(candidates) != 1:
        return None
    name = candidates[0]
    value = retail_symbols[name] if isinstance(retail_symbols, dict) else None
    return name, value


def invokes_exact_retail_body(
    reference: Method,
    current: Method | None,
    retail_symbols: dict[str, int],
    retail_addresses: dict[int, set[str]],
) -> bool:
    if current is None or not reference.mangled:
        return False
    resolved = resolved_retail_symbol(reference, retail_symbols)
    if resolved is None or resolved[1] is None:
        return False
    rva = resolved[1]
    expected_ids = retail_addresses.get(rva, set())
    return bool(expected_ids.intersection(current.retail_address_ids))


def exact_retail_body_route(
    reference: Method,
    current: Method | None,
    retail_symbols: dict[str, int],
    retail_addresses: dict[int, set[str]],
    entries: Iterable[CuratedEvidence],
) -> str:
    resolved = resolved_retail_symbol(reference, retail_symbols)
    if not reference.mangled or resolved is None:
        return "not-applicable"
    if invokes_exact_retail_body(
        reference, current, retail_symbols, retail_addresses
    ):
        return "retail-address"
    if any(
        entry.kind == "retail-constructor-reconstruction"
        for entry in matching_evidence(reference, entries)
    ):
        return "layered-constructor-reconstruction"
    if any(
        entry.kind == "retail-destructor-thunk"
        for entry in matching_evidence(reference, entries)
    ):
        return "retail-destructor-thunk"
    if any(
        entry.kind == "retail-destructor-reconstruction"
        for entry in matching_evidence(reference, entries)
    ):
        return "layered-destructor-reconstruction"
    return "missing-retail-route"


def load_retail_dependency_rvas(path: Path) -> set[int]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        rows = csv.DictReader(stream, delimiter="\t")
        if rows.fieldnames is None or "rva" not in rows.fieldnames:
            raise RuntimeError(f"invalid retail dependency table {path}")
        result = {
            int(row["rva"], 16)
            for row in rows
            if row.get("rva") and row.get("disposition") != "pending"
        }
    if not result:
        raise RuntimeError(f"no RVAs found in retail dependency table {path}")
    return result


def load_curated_evidence(path: Path) -> list[CuratedEvidence]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        rows = csv.DictReader(stream, delimiter="\t")
        required = {"owner", "method", "mangled", "evidence", "source", "note"}
        if rows.fieldnames is None or not required.issubset(rows.fieldnames):
            raise RuntimeError(f"invalid public evidence ledger {path}")
        result = []
        for line, row in enumerate(rows, start=2):
            kind = row["evidence"].strip()
            if kind not in EVIDENCE_KINDS:
                raise RuntimeError(
                    f"unknown evidence kind {kind!r} at {path}:{line}"
                )
            result.append(
                CuratedEvidence(
                    owner=row["owner"].strip(),
                    method=row["method"].strip(),
                    mangled=row["mangled"].strip(),
                    kind=kind,
                    source=row["source"].strip(),
                    note=row["note"].strip(),
                )
            )
    return result


def validate_curated_evidence(
    entries: Iterable[CuratedEvidence],
    reference_surface: dict[str, set[Method]],
) -> None:
    seen = set()
    layout_kinds: dict[str, str] = {}
    for entry in entries:
        key = (entry.owner, entry.method, entry.mangled, entry.kind)
        if key in seen:
            raise RuntimeError(
                f"duplicate evidence {entry.kind} for "
                f"{entry.owner}::{entry.method}"
            )
        seen.add(key)
        if entry.owner not in reference_surface:
            raise RuntimeError(f"evidence refers to unknown public owner {entry.owner}")
        if entry.kind.startswith("layout-"):
            if entry.method != "*" or entry.mangled:
                raise RuntimeError(
                    f"layout evidence for {entry.owner} must use method '*'"
                )
            previous = layout_kinds.setdefault(entry.owner, entry.kind)
            if previous != entry.kind:
                raise RuntimeError(
                    f"conflicting layout evidence for {entry.owner}: "
                    f"{previous} and {entry.kind}"
                )
            continue
        if entry.kind in {
            "retail-vtable-variant",
            "retail-inheritance-variant",
        }:
            if entry.method != "*" or entry.mangled:
                raise RuntimeError(
                    f"class ABI evidence for {entry.owner} must use method '*'"
                )
            continue
        if entry.method == "*":
            raise RuntimeError(
                f"method evidence {entry.kind} for {entry.owner} cannot use '*'"
            )
        matches = [
            method
            for method in reference_surface[entry.owner]
            if method.name == entry.method
            and (not entry.mangled or method.mangled == entry.mangled)
        ]
        if len(matches) != 1:
            raise RuntimeError(
                f"evidence {entry.kind} for {entry.owner}::{entry.method} "
                f"matches {len(matches)} public methods; provide an exact mangled name"
            )


def matching_evidence(
    reference: Method,
    entries: Iterable[CuratedEvidence],
) -> list[CuratedEvidence]:
    return [
        entry
        for entry in entries
        if entry.owner == reference.owner
        and (
            entry.method == "*"
            or (
                entry.method == reference.name
                and (not entry.mangled or entry.mangled == reference.mangled)
            )
        )
    ]


def evidence_columns(
    reference: Method,
    retail_symbols: dict[str, int],
    retail_dependency_rvas: set[int],
    entries: Iterable[CuratedEvidence],
) -> EvidenceColumns:
    matched = matching_evidence(reference, entries)
    kinds = {entry.kind for entry in matched}
    resolved = resolved_retail_symbol(reference, retail_symbols)
    retail_rva = resolved[1] if resolved is not None else None
    if "retail-body-variant" in kinds:
        retail_body = "binary-confirmed-variant"
    elif "retail-inline-body" in kinds:
        retail_body = "binary-confirmed-inline"
    elif retail_rva is not None:
        retail_body = "idb-exact-name"
    else:
        retail_body = "none"
    if retail_rva in retail_dependency_rvas:
        retail_callsite = "direct-target"
    elif "retail-indirect-callsite" in kinds:
        retail_callsite = "indirect-vtable"
    else:
        retail_callsite = "none"

    if "layout-retail" in kinds:
        owner_layout = "retail-confirmed"
    elif "layout-ida-only" in kinds:
        owner_layout = "ida-import-only"
    elif "layout-not-applicable" in kinds:
        owner_layout = "not-applicable"
    else:
        owner_layout = "unclassified"

    sources = ";".join(sorted({entry.source for entry in matched if entry.source}))
    return EvidenceColumns(
        retail_body=retail_body,
        retail_callsite=retail_callsite,
        owner_layout=owner_layout,
        nearby_header_definition=reference.has_body,
        nearby_source_basis="nearby-source-basis" in kinds,
        host_behavior_test="host-test" in kinds,
        ballance_player_test="ballance-player" in kinds,
        sources=sources,
    )


def classify(
    reference: Method,
    current_exact: set[tuple[str, str, str]],
    current_names: set[tuple[str, str]],
    reference_names_with_exact_overload: set[tuple[str, str]],
    retail_names: Collection[str],
    entries: Iterable[CuratedEvidence] = (),
) -> str:
    retail_absent = any(
        entry.kind == "retail-interface-absent"
        for entry in matching_evidence(reference, entries)
    )
    if retail_absent:
        return (
            "retail-interface-absence-violation"
            if reference.key in current_exact
            else "retail-omitted-variant"
        )
    if reference.key in current_exact:
        return "current-exact"
    name = (reference.owner, reference.name)
    if any(
        entry.kind == "retail-signature-variant"
        for entry in matching_evidence(reference, entries)
    ):
        return (
            "retail-confirmed-signature-variant"
            if name in current_names
            else "unresolved-missing"
        )
    if name in current_names and name not in reference_names_with_exact_overload:
        return "current-signature-diff"
    if reference.mangled and resolved_retail_symbol(reference, retail_names):
        return "retail-body-missing"
    if reference.pure_virtual:
        return "pure-virtual-missing"
    if reference.has_body:
        return "inline-candidate-missing"
    return "unresolved-missing"


def current_method_for(
    reference: Method,
    current_surface: dict[str, set[Method]],
) -> Method | None:
    """Return the unique current declaration with the same C++ signature."""
    matches = [
        method
        for method in current_surface.get(reference.owner, ())
        if method.key == reference.key
    ]
    if len(matches) > 1:
        raise RuntimeError(
            f"duplicate current declarations for "
            f"{reference.owner}::{reference.name} {reference.signature}"
        )
    return matches[0] if matches else None


def callable_disposition(current: Method | None) -> str:
    """Classify what a consumer can actually emit from the current headers.

    A matching declaration is not by itself callable: this SDK does not link
    an IVP import library.  Header definitions can emit code directly, pure
    virtual declarations are intentional interface contracts, and declaration-
    only virtuals can dispatch through a retail-owned object's vtable.  A
    non-virtual declaration without a body would instead become an unresolved
    external and is therefore the actionable closure gap.
    """
    if current is None:
        return "not-current-exact"
    if current.deleted:
        return "retail-unavailable"
    if current.has_body:
        return "header-definition"
    if current.pure_virtual:
        return "pure-virtual-contract"
    if current.virtual:
        return "retail-vtable-dispatch"
    return "declaration-only"


def virtual_abi_disposition(
    reference: Method,
    current_surface: dict[str, set[Method]],
    entries: Iterable[CuratedEvidence] = (),
) -> str:
    """Compare whether an exact declaration occupies a virtual ABI slot.

    Matching spelling and parameter types are insufficient for ABI safety: a
    missing ``virtual`` changes both dispatch and every following vtable slot.
    Constructors and methods absent from the current surface are not classed as
    virtual mismatches because their declaration disposition already reports
    the missing API.
    """
    current = current_method_for(reference, current_surface)
    if current is None:
        return "not-current-exact"
    if current.virtual == reference.virtual:
        return "match"
    if "retail-virtual-variant" in {
        entry.kind for entry in matching_evidence(reference, entries)
    }:
        return "retail-confirmed-variant"
    if "ida-virtual-variant" in {
        entry.kind for entry in matching_evidence(reference, entries)
    }:
        return "ida-import-variant"
    return "mismatch"


def inheritance_abi_disposition(
    owner: str,
    reference_bases: dict[str, tuple[str, ...]],
    current_bases: dict[str, tuple[str, ...]],
    entries: Iterable[CuratedEvidence] = (),
) -> str:
    if owner not in current_bases:
        return "not-current"
    if current_bases[owner] == reference_bases.get(owner, ()):
        return "match"
    if any(
        entry.owner == owner
        and entry.method == "*"
        and entry.kind == "retail-inheritance-variant"
        for entry in entries
    ):
        return "retail-confirmed-variant"
    return "mismatch"


def vtable_order_abi_disposition(
    owner: str,
    reference_slots: dict[str, tuple[tuple[str, str], ...]],
    current_slots: dict[str, tuple[tuple[str, str], ...]],
    current_bases: dict[str, tuple[str, ...]],
    entries: Iterable[CuratedEvidence] = (),
) -> str:
    if owner not in current_bases:
        return "not-current"
    owner_entries = [entry for entry in entries if entry.owner == owner]
    if any(
        entry.method == "*" and entry.kind == "retail-vtable-variant"
        for entry in owner_entries
    ):
        return "retail-confirmed-variant"

    ignored_names = {
        entry.method
        for entry in owner_entries
        if entry.kind in {
            "retail-virtual-variant",
            "ida-virtual-variant",
            "retail-interface-absent",
        }
    }
    nearby = tuple(
        name
        for name, _ in reference_slots.get(owner, ())
        if name not in ignored_names
    )
    current = tuple(name for name, _ in current_slots.get(owner, ()))
    return "match" if current == nearby else "mismatch"


def write_method_rows(
    reference_surface: dict[str, set[Method]],
    current_surface: dict[str, set[Method]],
    retail_symbols: dict[str, int],
    retail_addresses: dict[int, set[str]],
    retail_dependency_rvas: set[int],
    curated_evidence: list[CuratedEvidence],
) -> None:
    current_exact = {
        method.key for methods in current_surface.values() for method in methods
    }
    current_names = {
        (method.owner, method.name)
        for methods in current_surface.values()
        for method in methods
    }
    reference_names_with_exact_overload = {
        (method.owner, method.name)
        for methods in reference_surface.values()
        for method in methods
        if method.key in current_exact
    }
    writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    writer.writerow(
        [
            "class",
            "method",
            "signature",
            "disposition",
            "nearby_has_body",
            "nearby_pure_virtual",
            "nearby_virtual",
            "current_has_body",
            "current_pure_virtual",
            "current_virtual",
            "current_retail_invoke",
            "current_retail_address_ids",
            "current_exact_retail_invoke",
            "current_exact_retail_body_route",
            "callable_disposition",
            "virtual_abi",
            "retail_exact_name",
            "retail_body_evidence",
            "retail_callsite_evidence",
            "owner_layout_evidence",
            "nearby_header_definition",
            "nearby_source_basis",
            "host_behavior_test",
            "ballance_player_test",
            "curated_evidence_sources",
            "nearby_mangled_name",
        ]
    )
    for owner in sorted(reference_surface):
        for method in sorted(
            reference_surface[owner], key=lambda item: (item.name, item.signature)
        ):
            disposition = classify(
                method, current_exact, current_names,
                reference_names_with_exact_overload, retail_symbols,
                curated_evidence,
            )
            evidence = evidence_columns(
                method,
                retail_symbols,
                retail_dependency_rvas,
                curated_evidence,
            )
            current = current_method_for(method, current_surface)
            writer.writerow(
                [
                    owner,
                    method.name,
                    method.signature,
                    disposition,
                    str(method.has_body).lower(),
                    str(method.pure_virtual).lower(),
                    str(method.virtual).lower(),
                    (
                        str(current.has_body).lower()
                        if current is not None
                        else ""
                    ),
                    (
                        str(current.pure_virtual).lower()
                        if current is not None
                        else ""
                    ),
                    (
                        str(current.virtual).lower()
                        if current is not None
                        else ""
                    ),
                    (
                        str(current.retail_invoke).lower()
                        if current is not None
                        else ""
                    ),
                    (
                        ";".join(current.retail_address_ids)
                        if current is not None
                        else ""
                    ),
                    str(invokes_exact_retail_body(
                        method, current, retail_symbols, retail_addresses
                    )).lower(),
                    exact_retail_body_route(
                        method,
                        current,
                        retail_symbols,
                        retail_addresses,
                        curated_evidence,
                    ),
                    callable_disposition(current),
                    virtual_abi_disposition(
                        method, current_surface, curated_evidence
                    ),
                    str(bool(method.mangled and resolved_retail_symbol(
                        method, retail_symbols
                    ))).lower(),
                    evidence.retail_body,
                    evidence.retail_callsite,
                    evidence.owner_layout,
                    str(evidence.nearby_header_definition).lower(),
                    str(evidence.nearby_source_basis).lower(),
                    str(evidence.host_behavior_test).lower(),
                    str(evidence.ballance_player_test).lower(),
                    evidence.sources,
                    (
                        resolved_retail_symbol(method, retail_symbols)[0]
                        if resolved_retail_symbol(method, retail_symbols)
                        else method.mangled
                    ),
                ]
            )


def write_class_rows(
    reference_surface: dict[str, set[Method]],
    current_surface: dict[str, set[Method]],
    retail_symbols: dict[str, int],
    retail_dependency_rvas: set[int],
    curated_evidence: list[CuratedEvidence],
    reference_bases: dict[str, tuple[str, ...]],
    current_bases: dict[str, tuple[str, ...]],
    reference_virtual_slots: dict[str, tuple[tuple[str, str], ...]],
    current_virtual_slots: dict[str, tuple[tuple[str, str], ...]],
) -> None:
    current_exact = {
        method.key for methods in current_surface.values() for method in methods
    }
    current_names = {
        (method.owner, method.name)
        for methods in current_surface.values()
        for method in methods
    }
    reference_names_with_exact_overload = {
        (method.owner, method.name)
        for methods in reference_surface.values()
        for method in methods
        if method.key in current_exact
    }
    writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    dispositions = [
        "current-exact",
        "retail-confirmed-signature-variant",
        "current-signature-diff",
        "retail-omitted-variant",
        "retail-interface-absence-violation",
        "retail-body-missing",
        "inline-candidate-missing",
        "pure-virtual-missing",
        "unresolved-missing",
    ]
    writer.writerow(
        [
            "class",
            "nearby_public_methods",
            *dispositions,
            "idb_exact_named_retail_bodies",
            "binary_confirmed_body_variants",
            "binary_confirmed_inline_bodies",
            "retail_direct_callsites",
            "retail_indirect_callsites",
            "nearby_header_definitions",
            "nearby_source_basis_methods",
            "host_tested_methods",
            "ballance_player_tested_methods",
            "owner_layout_evidence",
            "nearby_direct_bases",
            "current_direct_bases",
            "inheritance_abi",
            "nearby_new_virtual_slots",
            "current_new_virtual_slots",
            "vtable_slot_order_abi",
        ]
    )
    for owner in sorted(reference_surface):
        counts = defaultdict(int)
        evidence_counts = defaultdict(int)
        owner_layout = "unclassified"
        for method in reference_surface[owner]:
            counts[
                classify(
                    method, current_exact, current_names,
                    reference_names_with_exact_overload, retail_symbols,
                    curated_evidence,
                )
            ] += 1
            evidence = evidence_columns(
                method,
                retail_symbols,
                retail_dependency_rvas,
                curated_evidence,
            )
            evidence_counts[evidence.retail_body] += int(
                evidence.retail_body != "none"
            )
            evidence_counts[evidence.retail_callsite] += 1
            evidence_counts["nearby-header-definition"] += int(
                evidence.nearby_header_definition
            )
            evidence_counts["nearby-source-basis"] += int(
                evidence.nearby_source_basis
            )
            evidence_counts["host-test"] += int(evidence.host_behavior_test)
            evidence_counts["ballance-player"] += int(
                evidence.ballance_player_test
            )
            if evidence.owner_layout == "retail-confirmed":
                owner_layout = "retail-confirmed"
            elif (
                evidence.owner_layout == "ida-import-only"
                and owner_layout == "unclassified"
            ):
                owner_layout = "ida-import-only"
            elif (
                evidence.owner_layout == "not-applicable"
                and owner_layout == "unclassified"
            ):
                owner_layout = "not-applicable"
        writer.writerow(
            [
                owner,
                len(reference_surface[owner]),
                *(counts[disposition] for disposition in dispositions),
                evidence_counts["idb-exact-name"],
                evidence_counts["binary-confirmed-variant"],
                evidence_counts["binary-confirmed-inline"],
                evidence_counts["direct-target"],
                evidence_counts["indirect-vtable"],
                evidence_counts["nearby-header-definition"],
                evidence_counts["nearby-source-basis"],
                evidence_counts["host-test"],
                evidence_counts["ballance-player"],
                owner_layout,
                ";".join(reference_bases.get(owner, ())),
                ";".join(current_bases.get(owner, ())),
                inheritance_abi_disposition(
                    owner,
                    reference_bases,
                    current_bases,
                    curated_evidence,
                ),
                ";".join(
                    name for name, _ in reference_virtual_slots.get(owner, ())
                ),
                ";".join(
                    name for name, _ in current_virtual_slots.get(owner, ())
                ),
                vtable_order_abi_disposition(
                    owner,
                    reference_virtual_slots,
                    current_virtual_slots,
                    current_bases,
                    curated_evidence,
                ),
            ]
        )


def write_summary_rows(
    reference_surface: dict[str, set[Method]],
    current_surface: dict[str, set[Method]],
    retail_symbols: dict[str, int],
    retail_addresses: dict[int, set[str]],
    retail_dependency_rvas: set[int],
    curated_evidence: list[CuratedEvidence],
    reference_virtual_slots: dict[str, tuple[tuple[str, str], ...]],
    current_virtual_slots: dict[str, tuple[tuple[str, str], ...]],
    current_bases: dict[str, tuple[str, ...]],
) -> None:
    methods = [
        method for values in reference_surface.values() for method in values
    ]
    current_exact = {
        method.key for values in current_surface.values() for method in values
    }
    current_names = {
        (method.owner, method.name)
        for values in current_surface.values()
        for method in values
    }
    exact_overloads = {
        (method.owner, method.name)
        for method in methods
        if method.key in current_exact
    }
    dispositions = [
        classify(
            method,
            current_exact,
            current_names,
            exact_overloads,
            retail_symbols,
            curated_evidence,
        )
        for method in methods
    ]
    evidence = [
        evidence_columns(
            method,
            retail_symbols,
            retail_dependency_rvas,
            curated_evidence,
        )
        for method in methods
    ]
    class_layout = {}
    for owner in reference_surface:
        kinds = {
            entry.kind
            for entry in curated_evidence
            if entry.owner == owner and entry.method == "*"
        }
        class_layout[owner] = (
            "retail-confirmed"
            if "layout-retail" in kinds
            else "ida-import-only"
            if "layout-ida-only" in kinds
            else "not-applicable"
            if "layout-not-applicable" in kinds
            else "unclassified"
        )

    exact_retail_methods = [
        method
        for method in methods
        if method.mangled and resolved_retail_symbol(method, retail_symbols)
    ]

    rows = [
        ("candidate_methods", len(methods), len(methods)),
        ("current_exact_signature", dispositions.count("current-exact"), len(methods)),
        (
            "retail_confirmed_signature_variant",
            dispositions.count("retail-confirmed-signature-variant"),
            len(methods),
        ),
        (
            "current_signature_difference",
            dispositions.count("current-signature-diff"),
            len(methods),
        ),
        (
            "retail_omitted_interface_variant",
            dispositions.count("retail-omitted-variant"),
            len(methods),
        ),
        (
            "retail_interface_absence_violation",
            dispositions.count("retail-interface-absence-violation"),
            len(methods),
        ),
        (
            "current_header_definition",
            sum(
                callable_disposition(current_method_for(method, current_surface))
                == "header-definition"
                for method in methods
            ),
            len(methods),
        ),
        (
            "current_any_retail_invoke",
            sum(
                bool(current_method_for(method, current_surface).retail_invoke)
                for method in methods
                if current_method_for(method, current_surface) is not None
            ),
            len(methods),
        ),
        (
            "current_exact_retail_body_invoke",
            sum(
                invokes_exact_retail_body(
                    method,
                    current_method_for(method, current_surface),
                    retail_symbols,
                    retail_addresses,
                )
                for method in exact_retail_methods
            ),
            len(exact_retail_methods),
        ),
        (
            "current_exact_retail_body_accounted",
            sum(
                exact_retail_body_route(
                    method,
                    current_method_for(method, current_surface),
                    retail_symbols,
                    retail_addresses,
                    curated_evidence,
                ) != "missing-retail-route"
                for method in exact_retail_methods
            ),
            len(exact_retail_methods),
        ),
        (
            "current_pure_virtual_contract",
            sum(
                callable_disposition(current_method_for(method, current_surface))
                == "pure-virtual-contract"
                for method in methods
            ),
            len(methods),
        ),
        (
            "current_retail_vtable_dispatch",
            sum(
                callable_disposition(current_method_for(method, current_surface))
                == "retail-vtable-dispatch"
                for method in methods
            ),
            len(methods),
        ),
        (
            "current_declaration_only",
            sum(
                callable_disposition(current_method_for(method, current_surface))
                == "declaration-only"
                for method in methods
            ),
            len(methods),
        ),
        (
            "current_explicitly_unavailable",
            sum(
                callable_disposition(current_method_for(method, current_surface))
                == "retail-unavailable"
                for method in methods
            ),
            len(methods),
        ),
        (
            "current_virtual_abi_match",
            sum(
                virtual_abi_disposition(
                    method, current_surface, curated_evidence
                ) == "match"
                for method in methods
            ),
            len(methods),
        ),
        (
            "current_virtual_abi_mismatch",
            sum(
                virtual_abi_disposition(
                    method, current_surface, curated_evidence
                ) == "mismatch"
                for method in methods
            ),
            len(methods),
        ),
        (
            "current_virtual_abi_retail_variant",
            sum(
                virtual_abi_disposition(
                    method, current_surface, curated_evidence
                ) == "retail-confirmed-variant"
                for method in methods
            ),
            len(methods),
        ),
        (
            "current_virtual_abi_ida_variant",
            sum(
                virtual_abi_disposition(
                    method, current_surface, curated_evidence
                ) == "ida-import-variant"
                for method in methods
            ),
            len(methods),
        ),
        (
            "current_vtable_slot_order_match",
            sum(
                vtable_order_abi_disposition(
                    owner,
                    reference_virtual_slots,
                    current_virtual_slots,
                    current_bases,
                    curated_evidence,
                ) == "match"
                for owner in reference_surface
            ),
            len(reference_surface),
        ),
        (
            "current_vtable_slot_order_mismatch",
            sum(
                vtable_order_abi_disposition(
                    owner,
                    reference_virtual_slots,
                    current_virtual_slots,
                    current_bases,
                    curated_evidence,
                ) == "mismatch"
                for owner in reference_surface
            ),
            len(reference_surface),
        ),
        (
            "current_vtable_slot_order_retail_variant",
            sum(
                vtable_order_abi_disposition(
                    owner,
                    reference_virtual_slots,
                    current_virtual_slots,
                    current_bases,
                    curated_evidence,
                ) == "retail-confirmed-variant"
                for owner in reference_surface
            ),
            len(reference_surface),
        ),
        (
            "idb_exact_named_retail_body",
            sum(item.retail_body == "idb-exact-name" for item in evidence),
            len(methods),
        ),
        (
            "binary_confirmed_body_variant",
            sum(
                item.retail_body == "binary-confirmed-variant"
                for item in evidence
            ),
            len(methods),
        ),
        (
            "binary_confirmed_inline_body",
            sum(
                item.retail_body == "binary-confirmed-inline"
                for item in evidence
            ),
            len(methods),
        ),
        (
            "retail_direct_callsite",
            sum(item.retail_callsite == "direct-target" for item in evidence),
            len(methods),
        ),
        (
            "retail_indirect_callsite",
            sum(item.retail_callsite == "indirect-vtable" for item in evidence),
            len(methods),
        ),
        (
            "nearby_header_definition",
            sum(item.nearby_header_definition for item in evidence),
            len(methods),
        ),
        (
            "nearby_source_basis",
            sum(item.nearby_source_basis for item in evidence),
            len(methods),
        ),
        (
            "host_behavior_tested_method",
            sum(item.host_behavior_test for item in evidence),
            len(methods),
        ),
        (
            "ballance_player_tested_method",
            sum(item.ballance_player_test for item in evidence),
            len(methods),
        ),
        (
            "retail_confirmed_layout_owner",
            sum(value == "retail-confirmed" for value in class_layout.values()),
            len(class_layout),
        ),
        (
            "ida_only_layout_owner",
            sum(value == "ida-import-only" for value in class_layout.values()),
            len(class_layout),
        ),
        (
            "layout_not_applicable_owner",
            sum(value == "not-applicable" for value in class_layout.values()),
            len(class_layout),
        ),
    ]
    writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    writer.writerow(["metric", "count", "denominator"])
    writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).parents[2])
    parser.add_argument("--reference-root", type=Path, required=True)
    parser.add_argument("--clang", type=Path, default=Path(shutil.which("clang++") or "clang++"))
    parser.add_argument("--evidence-ledger", type=Path)
    parser.add_argument(
        "--include-core-internal",
        action="store_true",
        help="legacy no-op: normal auditing now accepts spaced public markers",
    )
    parser.add_argument(
        "--owner",
        help="limit emitted rows and summary metrics to one reference owner",
    )
    parser.add_argument(
        "--format",
        choices=("classes-tsv", "methods-tsv", "summary-tsv"),
        default="classes-tsv",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    contract_check = subprocess.run(
        [
            sys.executable,
            str(repo_root / "tools/ivp/sync_retail_contract.py"),
            "--check",
        ],
        cwd=repo_root,
        check=False,
        capture_output=True,
        text=True,
    )
    if contract_check.returncode != 0:
        detail = contract_check.stderr.strip() or contract_check.stdout.strip()
        raise RuntimeError(f"IVP Retail Contract views are stale: {detail}")
    reference_root = args.reference_root.resolve()
    clang = args.clang.resolve()
    if not clang.is_file():
        raise RuntimeError(f"clang not found: {clang}")

    headers = find_reference_headers(reference_root)
    if args.include_core_internal:
        core_header = reference_root / "ivp_physics" / "ivp_core.hxx"
        if not core_header.is_file():
            raise RuntimeError(f"core header not found: {core_header}")
        headers.append(core_header)
        headers = sorted(set(headers))
    reference_includes = sorted({header.parent for header in headers})
    # Some public headers assume internal dependencies are on the include path.
    for directory_name in (
        "ivp_physics",
        "ivp_utility",
        "ivp_collision",
        "ivp_controller",
        "ivp_compact_builder",
    ):
        path = reference_root / directory_name
        if path.is_dir() and path not in reference_includes:
            reference_includes.append(path)

    reference_ast = clang_ast(
        clang,
        reference_source(headers),
        "c++14",
        reference_includes,
    )
    current_ast = clang_ast(
        clang,
        '#include "BML/IVP/IVP.h"\n',
        "c++17",
        [repo_root / "include"],
        ["_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH"],
    )
    reference_direct_bases = extract_direct_bases(reference_ast, headers)
    reference_support_direct_bases = extract_direct_bases(reference_ast)
    current_direct_bases = extract_direct_bases(current_ast)
    reference_bases = {
        owner: tuple(base.owner for base in bases)
        for owner, bases in reference_direct_bases.items()
    }
    current_bases = {
        owner: tuple(base.owner for base in bases)
        for owner, bases in current_direct_bases.items()
    }
    reference_template_parameters = extract_template_parameters(reference_ast)
    current_template_parameters = extract_template_parameters(current_ast)
    reference_support_bases = {
        owner: tuple(base.owner for base in bases)
        for owner, bases in reference_support_direct_bases.items()
    }
    reference_support_declared = extract_declared_methods(reference_ast)
    current_declared_support = extract_declared_methods(current_ast)
    reference_surface = apply_out_of_class_definitions(
        apply_effective_virtuals_with_support(
            extract_public_surface(reference_ast, headers),
            {
                owner: set(methods)
                for owner, methods in reference_support_declared.items()
            },
            reference_support_bases,
            reference_support_direct_bases,
            reference_template_parameters,
        ),
        reference_ast,
    )
    current_surface = apply_out_of_class_definitions(
        apply_effective_virtuals_with_support(
            extract_public_surface(current_ast),
            {
                owner: set(methods)
                for owner, methods in current_declared_support.items()
            },
            current_bases,
            current_direct_bases,
            current_template_parameters,
        ),
        current_ast,
    )
    reference_declared = apply_effective_virtuals_ordered_with_support(
        extract_declared_methods(reference_ast, headers),
        reference_support_declared,
        reference_support_bases,
        reference_support_direct_bases,
        reference_template_parameters,
    )
    current_declared = apply_effective_virtuals_ordered_with_support(
        current_declared_support,
        current_declared_support,
        current_bases,
        current_direct_bases,
        current_template_parameters,
    )
    reference_support_declared_resolved = apply_effective_virtuals_ordered(
        reference_support_declared,
        reference_support_bases,
        reference_support_direct_bases,
        reference_template_parameters,
    )
    reference_support_virtual_slots = direct_new_virtual_slots(
        reference_support_declared_resolved,
        reference_support_bases,
        reference_support_direct_bases,
        reference_template_parameters,
    )
    reference_virtual_slots = {
        owner: reference_support_virtual_slots.get(owner, ())
        for owner in reference_declared
    }
    current_virtual_slots = direct_new_virtual_slots(
        current_declared,
        current_bases,
        current_direct_bases,
        current_template_parameters,
    )
    retail_symbols = load_retail_manifest(repo_root / RETAIL_MANIFEST_PATH)
    retail_addresses = load_retail_addresses(repo_root / RETAIL_ADDRESS_PATH)
    retail_dependency_rvas = load_retail_dependency_rvas(
        repo_root / "tools" / "ivp" / "physics-rt-api-coverage.tsv"
    )
    evidence_ledger = (
        args.evidence_ledger.resolve()
        if args.evidence_ledger
        else repo_root / "tools" / "ivp" / "public-api-evidence.tsv"
    )
    curated_evidence = load_curated_evidence(evidence_ledger)
    validate_curated_evidence(curated_evidence, reference_surface)

    if args.owner:
        if args.owner not in reference_surface:
            raise RuntimeError(f"reference owner not found: {args.owner}")
        reference_surface = {
            args.owner: reference_surface[args.owner]
        }
        current_surface = {
            args.owner: current_surface.get(args.owner, set())
        }
        reference_bases = {
            args.owner: reference_bases.get(args.owner, ())
        }
        current_bases = {
            args.owner: current_bases.get(args.owner, ())
        }
        reference_virtual_slots = {
            args.owner: reference_virtual_slots.get(args.owner, ())
        }
        current_virtual_slots = {
            args.owner: current_virtual_slots.get(args.owner, ())
        }

    if args.format == "methods-tsv":
        write_method_rows(
            reference_surface,
            current_surface,
            retail_symbols,
            retail_addresses,
            retail_dependency_rvas,
            curated_evidence,
        )
    elif args.format == "classes-tsv":
        write_class_rows(
            reference_surface,
            current_surface,
            retail_symbols,
            retail_dependency_rvas,
            curated_evidence,
            reference_bases,
            current_bases,
            reference_virtual_slots,
            current_virtual_slots,
        )
    else:
        write_summary_rows(
            reference_surface,
            current_surface,
            retail_symbols,
            retail_addresses,
            retail_dependency_rvas,
            curated_evidence,
            reference_virtual_slots,
            current_virtual_slots,
            current_bases,
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
