#!/usr/bin/env python3
"""C11 syntax/layout checks; these do not claim target runtime acceptance."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--source-root",required=True,type=Path)
    args=parser.parse_args()
    compiler=shutil.which("clang")
    if not compiler:
        print("BML_TEST_SKIPPED: Clang is required for the seven-target C layout check")
        return 0
    triples=("i686-pc-windows-msvc","x86_64-pc-windows-msvc","aarch64-pc-windows-msvc",
             "x86_64-unknown-linux-gnu","aarch64-unknown-linux-gnu",
             "x86_64-apple-macosx12","arm64-apple-macosx12")
    source=args.source_root/"tests/abi/NativeModAbiCompile.c"
    # Use Clang's target-specific stddef/stdint/float declarations. Foreign
    # libc SDKs are unnecessary for syntax/layout; the public headers also
    # include these two C library headers, so supply only their declarations.
    with tempfile.TemporaryDirectory(prefix="bml-c-layout-") as temporary:
        shims = Path(temporary)
        (shims / "stdlib.h").write_text("#include <stddef.h>\nvoid *malloc(size_t);\nvoid *realloc(void *, size_t);\nvoid free(void *);\n")
        (shims / "string.h").write_text("#include <stddef.h>\nvoid *memcpy(void *, const void *, size_t);\nvoid *memset(void *, int, size_t);\nint memcmp(const void *, const void *, size_t);\nsize_t strlen(const char *);\nint strcmp(const char *, const char *);\n")
        (shims / "assert.h").write_text("#define assert(expression) ((void)0)\n")
        check_targets(compiler, shims, triples, source, args.source_root)
    return 0


def check_targets(compiler, shims, triples, source, root):
    for triple in triples:
        command=[compiler,"-target",triple,"-x","c","-std=c11","-ffreestanding",
                 "-fms-extensions","-Werror","-fsyntax-only","-DBML_RUNTIME_SDL=1",
                 "-I",str(shims),"-I",str(root/"include"),str(source)]
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        if result.returncode:
            raise RuntimeError(f"{triple} C ABI check failed:\n{result.stdout}")
        print(f"C11 layouts and cdecl: {triple}")
    return 0


if __name__=="__main__":
    raise SystemExit(main())
