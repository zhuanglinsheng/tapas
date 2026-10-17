#!/usr/bin/env python3
"""Collect a Clang PGO profile from hotspots, leaving algorithms for validation."""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from ab_script import binary_metadata
from compare_script import HERE, HOT_PATH_BENCHMARKS, benchmark_source


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tapas", type=Path, help="Clang instrumented Release executable")
    parser.add_argument("output", type=Path, help="output .profdata file")
    parser.add_argument("--llvm-profdata", help="matching Clang llvm-profdata executable")
    args = parser.parse_args()
    profiler = args.llvm_profdata or shutil.which("llvm-profdata")
    if profiler is None and shutil.which("xcrun"):
        profiler = subprocess.check_output(
            ["xcrun", "--find", "llvm-profdata"], text=True
        ).strip()
    if not profiler:
        parser.error("llvm-profdata was not found; pass --llvm-profdata")
    binary = args.tapas.resolve()
    metadata = binary_metadata(binary)
    sources = [HERE / benchmark_source(name).with_suffix(".tap")
               for name in HOT_PATH_BENCHMARKS]
    hashes = {str(source.relative_to(HERE)): sha256(source) for source in sources}
    checksums = {}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="tapas-pgo-") as directory:
        env = dict(os.environ, LLVM_PROFILE_FILE=str(Path(directory) / "%p.profraw"))
        for source in sources:
            result = subprocess.run([str(binary), str(source)], env=env,
                                    check=True, capture_output=True, text=True,
                                    timeout=120)
            lines = result.stdout.splitlines()
            if len(lines) != 2 or int(lines[1]) <= 0:
                raise RuntimeError(f"invalid training output: {source}: {result.stdout!r}")
            checksums[str(source.relative_to(HERE))] = int(lines[0])
        profiles = sorted(Path(directory).glob("*.profraw"))
        if not profiles:
            raise RuntimeError("no profiles produced; build with -fprofile-instr-generate")
        for source in sources:
            if sha256(source) != hashes[str(source.relative_to(HERE))]:
                raise RuntimeError(f"training source changed: {source}")
        if sha256(binary) != metadata["sha256"]:
            raise RuntimeError("instrumented binary changed during training")
        subprocess.run([profiler, "merge", "-o", str(args.output.resolve()),
                        *(str(path) for path in profiles)], check=True)
    manifest = {
        "training": "All hotspots; no complete algorithms.",
        "binary": metadata,
        "sources_sha256": hashes,
        "checksums": checksums,
        "profile_sha256": sha256(args.output),
        "llvm_profdata_version": subprocess.check_output(
            [profiler, "--version"], text=True).strip(),
    }
    args.output.with_suffix(".json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"trained on {len(sources)} hotspots; wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
