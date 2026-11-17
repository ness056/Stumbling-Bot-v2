#!/usr/bin/env python3
"""
Compare a UCI chess engine's `divide <depth>` against Stockfish's `go perft <depth>`.

Usage:
    python perft_compare.py /path/to/stockfish /path/to/my_engine
    python perft_compare.py stockfish.exe my_engine.exe --timeout 600
    python perft_compare.py stockfish.exe my_engine.exe --only "Start position" "Kiwipete"

No Python packages are required.
"""

from __future__ import annotations

import argparse
import queue
import re
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class TestPosition:
    name: str
    fen: str
    depth: int


# Six canonical Chess Programming Wiki positions, plus four edge-case positions
# from Peter Ellis Jones' public "Chess Perft test positions" gist.
# Depths are selected to keep each test at or below approximately 1 billion nodes.
POSITIONS = [
    TestPosition(
        "Start position",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        6,
    ),
    TestPosition(
        "Kiwipete",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        5,
    ),
    TestPosition(
        "Position 3 (endgame / en passant)",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        6,
    ),
    TestPosition(
        "Position 4 (promotions / checks)",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        6,
    ),
    TestPosition(
        "Position 5 (promotion-heavy middlegame)",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        5,
    ),
    TestPosition(
        "Position 6",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        5,
    ),
    TestPosition(
        "En-passant capture / discovered check",
        "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1",
        6,
    ),
    TestPosition(
        "Castling rights (kingside)",
        "5k2/8/8/8/8/8/8/4K2R w K - 0 1",
        6,
    ),
    TestPosition(
        "Promotion with rook check",
        "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1",
        6,
    ),
    TestPosition(
        "Pawn promotion endgame",
        "4k3/1P6/8/8/8/8/K7/8 w - - 0 1",
        6,
    ),
]


MOVE_LINE = re.compile(r"^\s*([a-h][1-8][a-h][1-8][qrbnQRBN]?)\s*:\s*(\d+)\s*$")
TOTAL_LINE = re.compile(r"^\s*(?:Nodes searched|Total nodes|Total)\s*:\s*(\d+)\s*$", re.I)


class UCIProcess:
    def __init__(self, executable: str, label: str, timeout: float):
        self.label = label
        self.timeout = timeout
        self.process = subprocess.Popen(
            [executable],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=None,
            text=True,
            bufsize=1,
        )
        self.lines: queue.Queue[Optional[str]] = queue.Queue()
        self.reader = threading.Thread(target=self._read_stdout, daemon=True)
        self.reader.start()
        self._send("uci")
        self._wait_for(lambda line: line.strip() == "uciok", "uciok")
        self._send("isready")
        self._wait_for(lambda line: line.strip() == "readyok", "readyok")

    def _read_stdout(self):
        assert self.process.stdout is not None
        for line in self.process.stdout:
            self.lines.put(line.rstrip("\r\n"))
        self.lines.put(None)

    def _send(self, command: str):
        if self.process.poll() is not None:
            raise RuntimeError(f"{self.label} exited with code {self.process.returncode}")
        assert self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def _wait_for(self, predicate, expected: str) -> list[str]:
        collected = []
        deadline = time.monotonic() + self.timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"{self.label}: timed out waiting for {expected}")
            try:
                line = self.lines.get(timeout=remaining)
            except queue.Empty:
                raise TimeoutError(f"{self.label}: timed out waiting for {expected}")
            if line is None:
                raise RuntimeError(f"{self.label} closed stdout while waiting for {expected}")
            collected.append(line)
            if predicate(line):
                return collected

    def divide(self, fen: str, moves: list[str], depth: int, command: str) -> tuple[dict[str, int], int]:
        position = "position fen " + fen
        if moves:
            position += " moves " + " ".join(moves)
        self._send(position)
        self._send(command.format(depth=depth))

        result: dict[str, int] = {}
        deadline = time.monotonic() + self.timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(
                    f"{self.label}: timed out running depth {depth} "
                    f"after moves {' '.join(moves) or '(root)'}"
                )
            try:
                line = self.lines.get(timeout=remaining)
            except queue.Empty:
                raise TimeoutError(f"{self.label}: timed out running perft")
            if line is None:
                raise RuntimeError(f"{self.label} closed stdout during perft")

            match = MOVE_LINE.match(line)
            if match:
                move, count = match.groups()
                result[move.lower()] = int(count)
                continue

            total_match = TOTAL_LINE.match(line)
            if total_match:
                return result, int(total_match.group(1))

    def close(self):
        if self.process.poll() is None:
            try:
                self._send("quit")
                self.process.wait(timeout=3)
            except Exception:
                self.process.kill()
                self.process.wait()


def show_moves(moves: list[str]) -> str:
    return " ".join(moves) if moves else "(none)"


def diagnose(
    stockfish: UCIProcess,
    mine: UCIProcess,
    fen: str,
    path: list[str],
    depth: int,
    indent: int = 0,
) -> bool:
    """Return True if this subtree contains a mismatch."""
    stock, stock_total = stockfish.divide(fen, path, depth, "go perft {depth}")
    ours, our_total = mine.divide(fen, path, depth, "divide {depth}")

    prefix = " " * indent
    if stock_total != our_total:
        print(f"{prefix}Total differs at depth {depth}: Stockfish={stock_total:,}, yours={our_total:,}")

    all_moves = sorted(set(stock) | set(ours))
    mismatches = [
        move for move in all_moves
        if stock.get(move) != ours.get(move)
    ]

    if not mismatches and stock_total == our_total:
        print(f"{prefix}OK depth {depth}: {stock_total:,} nodes")
        return False

    print(f"{prefix}Mismatch at depth {depth}, after moves: {show_moves(path)}")
    print(f"{prefix}  Stockfish total: {stock_total:,}")
    print(f"{prefix}  Your total:      {our_total:,}")

    for move in mismatches:
        sf_count = stock.get(move)
        my_count = ours.get(move)
        print(
            f"{prefix}  {move}: Stockfish={sf_count if sf_count is not None else 'ILLEGAL'}, "
            f"yours={my_count if my_count is not None else 'ILLEGAL'}"
        )

    # A move present in only one divide output is already the exact illegal
    # move. Its preceding path identifies the position where it was generated.
    # Stop immediately rather than checking other mismatching branches.
    for move in mismatches:
        if move not in stock or move not in ours:
            print(f"{prefix}FIRST DIFFERING POSITION FOUND.")
            print(f"{prefix}  Illegal/missing move: {move}")
            print(f"{prefix}  FEN: {fen}")
            print(f"{prefix}  Moves from test root: {show_moves(path)}")
            print(f"{prefix}  Command: position fen {fen} moves {show_moves(path)}")
            print(f"{prefix}  Full sequence: {show_moves(path + [move])}")
            return True

        # Both engines consider this move legal, but its subtree count differs.
        # Follow this branch and stop as soon as its first discrepancy is found.
        if depth > 1 and diagnose(
            stockfish, mine, fen, path + [move], depth - 1, indent + 4
        ):
            return True

    # At depth 1, all child counts should be 1. If only the total differs,
    # or a total differs despite identical divide entries, report the position.
    print(f"{prefix}FIRST DIFFERING POSITION FOUND.")
    print(f"{prefix}  FEN: {fen}")
    print(f"{prefix}  Moves from test root: {show_moves(path)}")
    if depth <= 1:
        print(f"{prefix}  The legal-move lists differ at this position.")
    else:
        print(f"{prefix}  Totals differ but no individual branch could be localized.")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Compare your UCI divide command against Stockfish perft."
    )
    parser.add_argument("stockfish", help="Path to Stockfish executable")
    parser.add_argument("engine", help="Path to your engine executable")
    parser.add_argument("--timeout", type=float, default=600.0,
                        help="Timeout in seconds for each perft call (default: 600)")
    parser.add_argument("--only", nargs="+", metavar="NAME",
                        help="Run only positions whose names contain one of these strings")
    args = parser.parse_args()

    selected = POSITIONS
    if args.only:
        needles = [s.casefold() for s in args.only]
        selected = [p for p in POSITIONS if any(n in p.name.casefold() for n in needles)]
        if not selected:
            parser.error("No test positions matched --only")

    sf = mine = None
    failures = 0
    try:
        print("Starting engines...")
        sf = UCIProcess(args.stockfish, "Stockfish", args.timeout)
        mine = UCIProcess(args.engine, "Your engine", args.timeout)

        print(f"\nRunning {len(selected)} perft tests.\n")
        for index, test in enumerate(selected, 1):
            print(f"[{index}/{len(selected)}] {test.name} (depth {test.depth})")
            print(f"  FEN: {test.fen}")
            try:
                mismatch = diagnose(sf, mine, test.fen, [], test.depth)
                if mismatch:
                    failures += 1
                    print("  RESULT: FAIL\n")
                else:
                    print("  RESULT: PASS\n")
            except (TimeoutError, RuntimeError) as exc:
                failures += 1
                print(f"  ERROR: {exc}\n")

    except (OSError, RuntimeError, TimeoutError) as exc:
        print(f"Fatal error: {exc}", file=sys.stderr)
        return 2
    finally:
        if mine:
            mine.close()
        if sf:
            sf.close()

    print(f"Finished: {len(selected) - failures}/{len(selected)} passed.")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
