import ast
import os
import re
import sys
import itertools
from pathlib import Path
from typing import Sequence
from errors import CompileError, Loc
from ast_parser import parse

_bop_counter = itertools.count()

# --- Operator tables ---

_BIN_OPS: dict[type, str] = {
    ast.Add:      "estus__add",
    ast.Sub:      "estus__sub",
    ast.Mult:     "estus__mul",
    ast.Div:      "estus__div",
    ast.FloorDiv: "estus__floordiv",
    ast.Mod:      "estus__mod",
    ast.Pow:      "estus__pow",
    ast.BitAnd:   "estus__band",
    ast.BitOr:    "estus__bor",
    ast.BitXor:   "estus__bxor",
    ast.LShift:   "estus__lshift",
    ast.RShift:   "estus__rshift",
}

_CMP_OPS: dict[type, str] = {
    ast.Eq:    "estus__eq",
    ast.NotEq: "estus__noteq",
    ast.Lt:    "estus__lt",
    ast.LtE:   "estus__lte",
    ast.Gt:    "estus__gt",
    ast.GtE:   "estus__gte",
}

# --- Builtins and native pass-throughs ---

BUILTIN_MAP: dict[str, str] = {
    "print": "estus__println",
}

# Single-arg builtins that take a duck and return a duck, no arena needed
BUILTIN_CAST_MAP: dict[str, str] = {
    "int":   "estus__casti",
    "float": "estus__castf",
    "bool":  "estus__castb",
    "abs":   "estus__abs",
}

# C stdlib functions that must NOT receive the implicit (_reg, _arena) injection
C_NATIVE_FUNCS: set[str] = {
    "llabs", "fabs",
    "sqrt", "pow", "ceil", "floor", "round",
    "sin", "cos", "tan", "atan2",
    "exp", "log", "log2", "log10",
    "memcpy", "memset", "strlen",
}

# --- Expression emission ---

def _expr_str(node: ast.expr) -> str:
    match node:
        case ast.Constant(value=v):
            if isinstance(v, bool):  return f"estus__packb({'true' if v else 'false'})"
            if isinstance(v, int):   return f"estus__packi({v})"
            if isinstance(v, float): return f"estus__packf({v})"
            if v is None:            return "estus__packn()"
            if isinstance(v, str):
                escaped = (v.replace("\\", "\\\\").replace('"', '\\"')
                            .replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t"))
                return f'estus__str_new(&_estus_reg, _estus_arena, "{escaped}", {len(v)})'
            return f"/* TODO: constant {type(v).__name__} */ estus__packn()"

        case ast.Name(id=name):
            return name

        case ast.List(elts=elts):
            if not elts:
                return "estus__list_create(&_estus_reg, _estus_arena, NULL, 0)"
            items = ", ".join(_expr_str(e) for e in elts)
            return f"estus__list_create(&_estus_reg, _estus_arena, (estus__duck[]){{{items}}}, {len(elts)})"

        case ast.BinOp(left=l, op=op, right=r):
            c_op = _BIN_OPS.get(type(op))
            if c_op:
                if isinstance(op, (ast.Add, ast.Mult)):
                    return f"({c_op}(&_estus_reg, _estus_arena, {_expr_str(l)}, {_expr_str(r)}))"
                return f"({c_op}({_expr_str(l)}, {_expr_str(r)}))"
            raise CompileError(
                Loc(node.lineno, node.col_offset),
                f"unsupported binary operator: {type(op).__name__}"
            )

        case ast.UnaryOp(op=op, operand=o):
            match op:
                case ast.USub():
                    # Fold negation into literal to avoid an extra pack/unpack cycle
                    if isinstance(o, ast.Constant):
                        if isinstance(o.value, int) and not isinstance(o.value, bool):
                            return f"estus__packi({-o.value})"
                        if isinstance(o.value, float):
                            return f"estus__packf({-o.value})"
                    return f"estus__negatei({_expr_str(o)})"  # TODO: dispatch on float too
                case ast.Not():
                    return f"estus__packb(!estus__unpack_truthy({_expr_str(o)}))"
                case ast.Invert():
                    return f"estus__invert({_expr_str(o)})"
                case ast.UAdd():
                    return _expr_str(o)
                case _:
                    return f"/* TODO: UnaryOp {type(op).__name__} */"

        case ast.BoolOp(op=op, values=values):
            is_or = isinstance(op, ast.Or)
            result = _expr_str(values[-1])
            for v in reversed(values[:-1]):
                t = f"_bop{next(_bop_counter)}"
                inner = _expr_str(v)
                if is_or:
                    result = f"(__extension__({{ estus__duck {t} = ({inner}); estus__unpack_truthy({t}) ? {t} : ({result}); }}))"
                else:
                    result = f"(__extension__({{ estus__duck {t} = ({inner}); estus__unpack_truthy({t}) ? ({result}) : {t}; }}))"
            return result

        case ast.Compare(left=left, ops=ops, comparators=comparators):
            if len(ops) == 1:
                c_op = _CMP_OPS.get(type(ops[0]))
                if c_op:
                    return f"{c_op}({_expr_str(left)}, {_expr_str(comparators[0])})"
                if isinstance(ops[0], (ast.Is, ast.IsNot)):
                    # TODO: Add equality by reference comparison
                    c_op = "==" if isinstance(ops[0], ast.Is) else "!="
                    return f"estus__packb({_expr_str(left)} {c_op} {_expr_str(comparators[0])})"
            # TODO: chained comparisons (e.g. 1 < x < 10)
            return f"/* TODO: chained compare */"

        case ast.Call():
            return _call_expr_str(node)

        case ast.Attribute(value=obj, attr=attr):
            # TODO: class instances are pointers — may need -> instead of .
            return f"{_expr_str(obj)}.{attr}"

        case ast.Subscript(value=obj, slice=idx):
            return f"estus__index(&_estus_reg, {_expr_str(obj)}, {_expr_str(idx)})"

        case ast.List() | ast.Tuple():
            return f"/* TODO: {type(node).__name__} literal */"

        case ast.JoinedStr():
            return f"/* TODO: f-string */"

        case _:
            return f"/* TODO: expr {type(node).__name__} */"


def _call_expr_str(node: ast.Call) -> str:
    match node.func:
        case ast.Name(id=name) if name in BUILTIN_MAP:
            return _print_call_str(node)
        case ast.Name(id="len"):
            if len(node.args) != 1:
                raise CompileError(
                    Loc(node.lineno, node.col_offset),
                    f"len() takes exactly 1 argument ({len(node.args)} given)"
                )
            return f"estus__len(&_estus_reg, {_expr_str(node.args[0])})"
        case ast.Name(id="str"):
            if len(node.args) != 1:
                raise CompileError(
                    Loc(node.lineno, node.col_offset),
                    f"str() takes exactly 1 argument ({len(node.args)} given)"
                )
            return f"estus__unpack_str(&_estus_reg, _estus_arena, {_expr_str(node.args[0])})"
        case ast.Name(id=name) if name in BUILTIN_CAST_MAP:
            if len(node.args) != 1:
                raise CompileError(
                    Loc(node.lineno, node.col_offset),
                    f"{name}() takes exactly 1 argument ({len(node.args)} given)"
                )
            return f"{BUILTIN_CAST_MAP[name]}({_expr_str(node.args[0])})"
        case ast.Name(id=name) if name in C_NATIVE_FUNCS:
            args = ", ".join(_expr_str(a) for a in node.args)
            return f"{name}({args})"
        case _:
            callee = _expr_str(node.func)
            args = ["&_estus_reg", "_estus_arena"] + [_expr_str(a) for a in node.args]
            return f"{callee}({', '.join(args)})"


def _print_call_str(node: ast.Call) -> str:
    """Expand print() to estus__print per-argument calls followed by a newline."""
    parts: list[str] = []
    for arg in node.args:
        # String literals can go straight to printf — no duck packing needed
        if isinstance(arg, ast.Constant) and isinstance(arg.value, str):
            escaped = (arg.value
                       .replace("\\", "\\\\")
                       .replace('"',  '\\"')
                       .replace("\n", "\\n")
                       .replace("\r", "\\r")
                       .replace("\t", "\\t"))
            parts.append(f'printf("{escaped}")')
        else:
            parts.append(f"estus__print(&_estus_reg, _estus_arena, {_expr_str(arg)})")
    parts.append('printf("\\n")')
    return "(" + ", ".join(parts) + ")"


# --- Statement emission ---

def _compile_stmts(stmts: Sequence[ast.stmt], c_lines: list[str], defined_vars: set[str]) -> None:
    for stmt in stmts:
        match stmt:
            case ast.AnnAssign():  _ann_assign(stmt, c_lines, defined_vars)
            case ast.Assign():     _assign(stmt, c_lines, defined_vars)
            case ast.AugAssign():  _aug_assign(stmt, c_lines)
            case ast.If():         _if_stmt(stmt, c_lines, defined_vars)
            case ast.While():      _while_stmt(stmt, c_lines, defined_vars)
            case ast.For():        _for_stmt(stmt, c_lines, defined_vars)
            case ast.Expr():       c_lines.append(f"{_expr_str(stmt.value)};")
            case ast.Return():     _return_stmt(stmt, c_lines)
            case ast.Break():      c_lines.append("break;")
            case ast.Continue():   c_lines.append("continue;")
            case ast.FunctionDef(): _func_def(stmt, c_lines)
            case ast.ClassDef():   _class_def(stmt, c_lines)
            case _:
                c_lines.append(f"/* TODO: stmt {type(stmt).__name__} */")


def _ann_assign(node: ast.AnnAssign, c_lines: list[str], defined_vars: set[str]) -> None:
    name = _expr_str(node.target)  # type: ignore[arg-type]
    value = _expr_str(node.value) if node.value else "estus__packn()"
    c_lines.append(f"estus__duck {name} = {value};")
    defined_vars.add(name)


def _assign(node: ast.Assign, c_lines: list[str], defined_vars: set[str]) -> None:
    value = _expr_str(node.value)
    for target in node.targets:
        name = _expr_str(target)  # type: ignore[arg-type]
        if name not in defined_vars:
            c_lines.append(f"estus__duck {name} = {value};")
            defined_vars.add(name)
        else:
            c_lines.append(f"{name} = {value};")


def _aug_assign(node: ast.AugAssign, c_lines: list[str]) -> None:
    # Python linter should flag error if user tries to AugAssign before variable declared
    target = _expr_str(node.target)  # type: ignore[arg-type]
    func = _BIN_OPS.get(type(node.op))
    if not func:
        raise CompileError(
            Loc(node.lineno, node.col_offset),
            f"unsupported augmented assignment operator: {type(node.op).__name__}"
        )
    c_lines.append(f"{target} = {func}({target}, {_expr_str(node.value)});")


def _if_stmt(node: ast.If, c_lines: list[str], defined_vars: set[str]) -> None:
    c_lines.append(f"if (estus__unpack_truthy({_expr_str(node.test)})) {{")
    _compile_stmts(node.body, c_lines, defined_vars)
    c_lines.append("}")
    while node.orelse:
        if len(node.orelse) == 1 and isinstance(node.orelse[0], ast.If):
            node = node.orelse[0]
            c_lines.append(f"else if (estus__unpack_truthy({_expr_str(node.test)})) {{")
            _compile_stmts(node.body, c_lines, defined_vars)
            c_lines.append("}")
        else:
            c_lines.append("else {")
            _compile_stmts(node.orelse, c_lines, defined_vars)
            c_lines.append("}")
            break


def _while_stmt(node: ast.While, c_lines: list[str], defined_vars: set[str]) -> None:
    c_lines.append(f"while (estus__unpack_truthy({_expr_str(node.test)})) {{")
    _compile_stmts(node.body, c_lines, defined_vars)
    c_lines.append("}")


def _for_stmt(node: ast.For, c_lines: list[str], defined_vars: set[str]) -> None:
    # Only range() is supported for now
    if not (isinstance(node.iter, ast.Call)
            and isinstance(node.iter.func, ast.Name)
            and node.iter.func.id == "range"):
        c_lines.append(f"/* TODO: for-in over non-range iterables */")
        return

    args = node.iter.args
    if len(args) not in (1, 2, 3):
        c_lines.append("/* TODO: range() requires 1–3 arguments */")
        return

    var = _expr_str(node.target)  # type: ignore[arg-type]

    # Validate literal arg types at parse time; guard duck expressions at runtime
    _BAD_LITERALS = (ast.Constant,)  # only int Constants are fine
    needs_guard: list[bool] = []
    for arg in args:
        if isinstance(arg, ast.Constant):
            if not isinstance(arg.value, int) or isinstance(arg.value, bool):
                c_lines.append(f"/* TODO: range() argument must be an integer */")
                return
            needs_guard.append(False)
        else:
            needs_guard.append(True)

    if any(needs_guard):
        c_lines.append("{")
        for i, (arg, guard) in enumerate(zip(args, needs_guard)):
            if guard:
                tmp = f"_estus_{var}_arg{i}"
                c_lines.append(f"estus__duck {tmp} = {_expr_str(arg)};")
                c_lines.append(
                    f"if (_get_type_enum({tmp}) != INT && _get_type_enum({tmp}) != UINT) "
                    f'estus__panic_roll(ESTUS_ERR_TYPE, "range() argument must be an integer");'
                )

    def _bound(i: int) -> str:
        arg = args[i]
        if isinstance(arg, ast.Constant):
            return str(arg.value)
        return f"estus__unpacki(_estus_{var}_arg{i})"

    match len(args):
        case 1:
            c_lines.append(f"for (int64_t _estus_i = 0; _estus_i < {_bound(0)}; _estus_i++) {{")
        case 2:
            c_lines.append(f"for (int64_t _estus_i = {_bound(0)}; _estus_i < {_bound(1)}; _estus_i++) {{")
        case 3:
            c_lines.append(f"for (int64_t _estus_i = {_bound(0)}; _estus_i < {_bound(1)}; _estus_i += {_bound(2)}) {{")

    c_lines.append(f"estus__duck {var} = estus__packi(_estus_i);")
    defined_vars.add(var)
    _compile_stmts(node.body, c_lines, defined_vars)
    c_lines.append("}")

    if any(needs_guard):
        c_lines.append("}")


def _return_stmt(node: ast.Return, c_lines: list[str]) -> None:
    if node.value:
        c_lines.append(f"return {_expr_str(node.value)};")
    else:
        c_lines.append("return;")


def _func_sig(node: ast.FunctionDef, class_name: str | None = None, forward_dec: bool = False) -> str:
    ret = "void" if (node.returns is None or (
        isinstance(node.returns, ast.Constant) and node.returns.value is None
    )) else "estus__duck"

    params: list[str] = ["estus__registry *_reg", "estus__arena *_parent_arena"]
    if class_name:
        params.append(f"{class_name} *self")

    for arg in node.args.args:
        if arg.arg == "self":
            continue
        params.append(f"estus__duck {arg.arg}")

    c_name = f"{class_name}__{node.name}" if class_name else node.name
    sig = f"{ret} {c_name}({', '.join(params)})"
    return sig + ";" if forward_dec else sig


def _func_def(node: ast.FunctionDef, c_lines: list[str], class_name: str | None = None) -> None:
    # Parameters are pre-declared by the signature, so seed defined_vars with them
    func_vars: set[str] = {arg.arg for arg in node.args.args if arg.arg != "self"}
    c_lines.append(f"{_func_sig(node, class_name)} {{")
    _compile_stmts(node.body, c_lines, func_vars)
    c_lines.append("}")


def _class_def(node: ast.ClassDef, c_lines: list[str]) -> None:
    fields  = [s for s in node.body if isinstance(s, ast.AnnAssign)]
    methods = [s for s in node.body if isinstance(s, ast.FunctionDef)]

    # Emit typedef struct
    c_lines.append(f"typedef struct {node.name} {{")
    for f in fields:
        c_lines.append(f"estus__duck {_expr_str(f.target)};")  # type: ignore[arg-type]
    c_lines.append(f"}} {node.name};")
    c_lines.append("")

    for method in methods:
        _func_def(method, c_lines, class_name=node.name)
        c_lines.append("")


# --- Forward declarations ---

def _forward_dec(stmts: Sequence[ast.stmt], c_lines: list[str], class_name: str | None = None) -> None:
    for stmt in stmts:
        if isinstance(stmt, ast.FunctionDef):
            c_lines.append(_func_sig(stmt, class_name, forward_dec=True))
            _forward_dec(stmt.body, c_lines, class_name)
        elif isinstance(stmt, ast.ClassDef):
            c_lines.append(f"struct {stmt.name};")
            methods = [s for s in stmt.body if isinstance(s, ast.FunctionDef)]
            _forward_dec(methods, c_lines, stmt.name)


# --- Main compile ---

def compile(tree: ast.Module) -> str:
    c_lines: list[str] = []

    _here    = os.path.dirname(__file__)
    _runtime = os.path.normpath(os.path.join(_here, "..", "runtime")).replace("\\", "/")

    try:
        with open(os.path.join(_here, "c_header.txt"), "r") as f:
            header = f.read()
        for name in ("arena.h", "duckbox.h", "print.h", "error.h"):
            header = header.replace(f'"{name}"', f'"{_runtime}/{name}"')
        header = re.sub(r'"(objects/[^"]+)"', lambda m: f'"{_runtime}/{m.group(1)}"', header)
        c_lines.append(header)
    except FileNotFoundError:
        c_lines.append("/* error: c_header.txt not found */")

    _forward_dec(tree.body, c_lines)
    c_lines.append("")

    defs = [s for s in tree.body if isinstance(s, (ast.FunctionDef, ast.ClassDef))]
    body = [s for s in tree.body if not isinstance(s, (ast.FunctionDef, ast.ClassDef))]

    _compile_stmts(defs, c_lines, set())

    # Global registry and arena — v1 uses a single arena for all allocations.
    # TODO: per-call-frame arenas once the full calling convention lands.
    c_lines.append("estus__registry _estus_reg;")
    c_lines.append("estus__arena *_estus_arena;")
    c_lines.append("")

    c_lines.append("int main(void) {")
    c_lines.append("_estus_reg = estus__registry_create();")
    c_lines.append("estus__str_init_char_table(&_estus_reg);")
    c_lines.append("uint16_t _main_arena_id = estus__arena_create(&_estus_reg);")
    c_lines.append("_estus_arena = &_estus_reg.arenas[_main_arena_id];")
    _compile_stmts(body, c_lines, set())
    c_lines.append("estus__arena_free(&_estus_reg, _main_arena_id);")
    c_lines.append("estus__registry_free(_estus_reg);")
    c_lines.append("return 0;")
    c_lines.append("}")

    # Indent based on brace depth.
    # Only leading '}' characters (before any '{') determine the indent decrease —
    # expression-level braces like __extension__({ ... }) are balanced on one line
    # and must not affect indentation.
    script = "\n".join(c_lines).split("\n")
    depth = 0
    for i, line in enumerate(script):
        opens  = line.count("{")
        closes = line.count("}")
        leading_closes = len(line.lstrip()) - len(line.lstrip().lstrip("}"))
        script[i] = "\t" * max(depth - leading_closes, 0) + line
        depth += opens - closes

    return "\n".join(script)

# Compiler entry point:
def main(args: list[str]) -> None:

    if len(args) != 1:
        raise CompileError(None, ".py path must be one argument")
    
    py_path = Path(args[0])
    if py_path.suffix != ".py":
        raise CompileError(None, "Can only compile a .py file")
    if not py_path.exists():
        raise CompileError(None, "Specified path to Python file is invalid")
    
    with open(py_path, 'r') as f:
        script = compile(parse(f.read()))
    
    print(script)
    out_path = py_path.with_name(py_path.stem + "_compile.c")
    with open(out_path, 'w') as output:
        output.write(script)

if __name__ == "__main__":
    main(sys.argv[1:])