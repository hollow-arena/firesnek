from __future__ import annotations
from dataclasses import dataclass
from typing import Optional


@dataclass
class Loc:
    line: int
    col:  int

    def __repr__(self) -> str:
        return f"{self.line}:{self.col}"


class CompileError(Exception):
    def __init__(self, loc: Optional[Loc], message: str) -> None:
        if loc:
            err_report = f"Compile error at Line: {loc.line}, Col: {loc.col}. {message}"
        else:
            err_report = f"Compile error: {message}"

        super().__init__(err_report)