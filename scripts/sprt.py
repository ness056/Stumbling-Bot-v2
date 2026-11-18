#!/usr/bin/env python3
"""
Compare two commits of Stumbling-Bot-v2 with an SPRT using cutechess-cli.

Pipeline:
  1. clone/fetch the repo into a local cache (bare clone)
  2. resolve both commit names (branch, tag, SHA, HEAD~3, ...) to full SHAs
  3. export each commit's sources, build them with CMake (cached per SHA + flags)
  4. run cutechess-cli with an SPRT: NEW engine vs BASE engine

Example:
  python sprt.py main~1 main \
      --cutechess C:/tools/cutechess/cutechess-cli.exe \
      --book books/UHO_Lichess_4852_v1.epd \
      --tc 8+0.08 --concurrency 8 --elo0 0 --elo1 5

The first positional argument is the BASE (reference) and the second is the NEW
version. H1 ("NEW is stronger than BASE by at least elo1") is accepted when the
NEW engine wins the test, so for a regression test use e.g. --elo0 -5 --elo1 0.
"""

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

DEFAULT_REPO = "https://github.com/ness056/Stumbling-Bot-v2.git"
DEFAULT_TARGET = "stumbling_bot_v2"  # executable target in CMakeLists.txt

IS_WIN = os.name == "nt"
EXE = ".exe" if IS_WIN else ""


# --------------------------------------------------------------------------- #
# helpers
# --------------------------------------------------------------------------- #
def die(msg: str):
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(1)


def run(cmd, check=True, **kwargs):
    cmd = [str(c) for c in cmd]
    print("$ " + " ".join(cmd), flush=True)
    result = subprocess.run(cmd, **kwargs)
    if check and result.returncode != 0:
        die(f"command failed with exit code {result.returncode}: {cmd[0]}")
    return result


def git(repo_dir: Path, *args, capture=False):
    cmd = ["git", "-C", str(repo_dir), *args]
    if capture:
        return subprocess.run(cmd, capture_output=True, text=True)
    return run(cmd)


# --------------------------------------------------------------------------- #
# 1-2. download / resolve
# --------------------------------------------------------------------------- #
def sync_repo(url: str, repo_dir: Path):
    if not repo_dir.exists():
        repo_dir.parent.mkdir(parents=True, exist_ok=True)
        run(["git", "clone", "--bare", url, repo_dir])
    git(
        repo_dir, "fetch", "--prune", "--force", "origin",
        "+refs/heads/*:refs/heads/*", "+refs/tags/*:refs/tags/*",
    )


def resolve(repo_dir: Path, name: str) -> str:
    r = git(repo_dir, "rev-parse", "--verify", "--quiet", f"{name}^{{commit}}", capture=True)
    if r.returncode != 0:
        die(f"cannot resolve '{name}' to a commit (is it pushed to the remote?)")
    return r.stdout.strip()


def export_sources(repo_dir: Path, sha: str, dest: Path):
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    proc = subprocess.Popen(
        ["git", "-C", str(repo_dir), "archive", "--format=tar", sha],
        stdout=subprocess.PIPE,
    )
    with tarfile.open(fileobj=proc.stdout, mode="r|") as tar:
        if hasattr(tarfile, "data_filter"):
            tar.extractall(dest, filter="data")
        else:
            tar.extractall(dest)
    if proc.wait() != 0:
        die(f"git archive failed for {sha}")


# --------------------------------------------------------------------------- #
# 3. build
# --------------------------------------------------------------------------- #
def find_executable(build_dir: Path, target: str) -> Path | None:
    candidates = [
        p for p in build_dir.rglob(target + EXE)
        if p.is_file() and "CMakeFiles" not in p.parts
    ]
    if not candidates:
        return None
    return max(candidates, key=lambda p: p.stat().st_mtime)


def build_commit(sha: str, repo_dir: Path, work: Path, args) -> Path:
    build_type = args.build_type or ("RelWithDebInfo" if IS_WIN else "Release")
    flags_key = hashlib.sha1(
        repr((build_type, args.target, sorted(args.cmake_arg))).encode()
    ).hexdigest()[:6]
    short = sha[:10]
    out_exe = work / "bin" / f"{args.target}-{short}-{flags_key}{EXE}"

    if out_exe.exists() and not args.rebuild:
        print(f"[build] {short}: using cached binary {out_exe}")
        return out_exe

    src_dir = work / "src" / short
    build_dir = work / "build" / f"{short}-{flags_key}"
    print(f"[build] {short}: exporting sources")
    export_sources(repo_dir, sha, src_dir)

    if build_dir.exists():
        shutil.rmtree(build_dir)

    run([
        "cmake", "-S", src_dir, "-B", build_dir,
        f"-DCMAKE_BUILD_TYPE={build_type}", *args.cmake_arg,
    ])
    run([
        "cmake", "--build", build_dir,
        "--config", build_type,
        "--target", args.target,
        "--parallel", str(os.cpu_count() or 1),
    ])

    exe = find_executable(build_dir, args.target)
    if exe is None:
        die(f"build succeeded but '{args.target}{EXE}' was not found in {build_dir}")

    out_exe.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(exe, out_exe)
    out_exe.chmod(out_exe.stat().st_mode | 0o111)
    return out_exe


# --------------------------------------------------------------------------- #
# 4. SPRT with cutechess-cli
# --------------------------------------------------------------------------- #
def sanitize(s: str) -> str:
    return re.sub(r"[^A-Za-z0-9._-]", "_", s)


def run_sprt(new_exe: Path, base_exe: Path, new_label: str, base_label: str,
             cutechess: str, work: Path, args) -> int:
    book_args = []
    if args.book:
        book = Path(args.book).resolve()
        if not book.is_file():
            die(f"opening book not found: {book}")
        ext = book.suffix.lower()
        if ext not in (".epd", ".pgn"):
            die("opening book must be .epd or .pgn")
        book_args = [
            "-openings", f"file={book}", f"format={ext[1:]}", "order=random",
        ]
    else:
        print("WARNING: no --book given. Every game starts from the initial position; "
              "with deterministic engines this gives very repetitive games and a "
              "poor SPRT. Use an opening book (EPD/PGN).")

    pgn_dir = work / "games"
    pgn_dir.mkdir(parents=True, exist_ok=True)
    pgn = pgn_dir / f"{sanitize(new_label)}_vs_{sanitize(base_label)}.pgn"

    engine_opts = [f"option.{o}" for o in args.option]

    cmd = [
        cutechess,
        # NEW is the first engine: H1 of the SPRT = "first engine is stronger".
        "-engine", f"name={sanitize(new_label)}", f"cmd={new_exe}", "proto=uci",
        "-engine", f"name={sanitize(base_label)}", f"cmd={base_exe}", "proto=uci",
        "-each", f"tc={args.tc}", *engine_opts,
        *book_args,
        "-games", "2", "-repeat",           # each opening is played with both colours
        "-rounds", str(args.max_games // 2),
        "-concurrency", str(args.concurrency),
        "-sprt", f"elo0={args.elo0}", f"elo1={args.elo1}",
        f"alpha={args.alpha}", f"beta={args.beta}",
        "-draw", "movenumber=40", "movecount=8", "score=10",
        "-resign", "movecount=3", "score=600", "twosided=true",
        "-ratinginterval", str(args.rating_interval),
        "-pgnout", str(pgn),
        "-recover",
    ]
    print("$ " + " ".join(str(c) for c in cmd), flush=True)

    score_line = elo_line = ptnml_line = sprt_line = ""
    proc = subprocess.Popen(
        [str(c) for c in cmd],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1, errors="replace",
    )
    try:
        for line in proc.stdout:
            print(line, end="", flush=True)
            s = line.strip()
            if s.startswith("Score of"):
                score_line = s
            elif s.startswith("Elo difference"):
                elo_line = s
            elif s.startswith("Ptnml"):
                ptnml_line = s
            elif s.startswith("SPRT:"):
                sprt_line = s
        proc.wait()
    except KeyboardInterrupt:
        print("\nInterrupted, stopping cutechess-cli...")
        proc.terminate()
        proc.wait()

    print("\n" + "=" * 70)
    print(f"NEW : {new_label}")
    print(f"BASE: {base_label}")
    for l in (score_line, elo_line, ptnml_line, sprt_line):
        if l:
            print(l)
    low = sprt_line.lower()
    if "h1 was accepted" in low:
        print(f"-> H1 accepted: NEW is stronger than BASE (elo >= {args.elo1}).")
    elif "h0 was accepted" in low:
        print(f"-> H0 accepted: NEW is not stronger than BASE (elo <= {args.elo0}).")
    else:
        print("-> SPRT did not conclude (stopped early or hit the max game count).")
    print(f"PGN: {pgn}")
    print("=" * 70)
    return proc.returncode or 0


# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser(
        description="Build two commits and compare them with an SPRT (cutechess-cli).",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument("base", help="BASE commit/branch/tag (reference version)")
    ap.add_argument("new", help="NEW commit/branch/tag (version being tested)")
    ap.add_argument("--repo", default=DEFAULT_REPO, help="git URL or local path of the repository")
    ap.add_argument("--workdir", default="sprt_work", help="cache/output directory")
    ap.add_argument("--target", default=DEFAULT_TARGET, help="CMake executable target")
    ap.add_argument("--build-type", default=None,
                    help="CMake build type (default: RelWithDebInfo on Windows, Release elsewhere)")
    ap.add_argument("--cmake-arg", action="append", default=[],
                    help="extra argument for the cmake configure step (repeatable), "
                         "e.g. --cmake-arg=-DCMAKE_CXX_FLAGS=-march=native")
    ap.add_argument("--rebuild", action="store_true", help="ignore cached binaries")

    ap.add_argument("--cutechess", default=None, help="path to cutechess-cli (default: search PATH)")
    ap.add_argument("--book", default=None, help="opening book (.epd or .pgn)")
    ap.add_argument("--tc", default="8+0.08", help="time control, cutechess syntax")
    ap.add_argument("--option", action="append", default=[],
                    help="UCI option for both engines (repeatable), e.g. --option Hash=16")
    ap.add_argument("--concurrency", type=int, default=max(1, (os.cpu_count() or 2) // 2),
                    help="number of games played in parallel")
    ap.add_argument("--max-games", type=int, default=20000, help="hard cap on number of games")
    ap.add_argument("--rating-interval", type=int, default=10, help="print stats every N games")

    ap.add_argument("--elo0", type=float, default=0.0, help="SPRT H0 elo")
    ap.add_argument("--elo1", type=float, default=5.0, help="SPRT H1 elo")
    ap.add_argument("--alpha", type=float, default=0.05)
    ap.add_argument("--beta", type=float, default=0.05)
    args = ap.parse_args()

    for tool in ("git", "cmake"):
        if shutil.which(tool) is None:
            die(f"'{tool}' not found in PATH")
    cutechess = args.cutechess or shutil.which("cutechess-cli")
    if not cutechess or not Path(cutechess).exists() and shutil.which(cutechess) is None:
        die("cutechess-cli not found: pass --cutechess /path/to/cutechess-cli")

    work = Path(args.workdir).resolve()
    repo_dir = work / "repo.git"

    sync_repo(args.repo, repo_dir)
    base_sha = resolve(repo_dir, args.base)
    new_sha = resolve(repo_dir, args.new)
    if base_sha == new_sha:
        die(f"'{args.base}' and '{args.new}' resolve to the same commit ({base_sha[:10]})")
    print(f"BASE {args.base} -> {base_sha}")
    print(f"NEW  {args.new} -> {new_sha}")

    base_exe = build_commit(base_sha, repo_dir, work, args)
    new_exe = build_commit(new_sha, repo_dir, work, args)

    rc = run_sprt(
        new_exe, base_exe,
        f"new-{new_sha[:8]}", f"base-{base_sha[:8]}",
        cutechess, work, args,
    )
    sys.exit(rc)


if __name__ == "__main__":
    main()
