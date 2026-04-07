import ast

# --- Statement handlers ---

def analyze_node(node: ast.stmt) -> None:
    match node:
        case ast.FunctionDef():       analyze_nodefunction_def(node)
        case ast.AsyncFunctionDef():  analyze_nodeasync_function_def(node)
        case ast.ClassDef():          analyze_nodeclass_def(node)
        case ast.Return():            analyze_nodereturn(node)
        case ast.Delete():            analyze_nodedelete(node)
        case ast.Assign():            analyze_nodeassign(node)
        case ast.AnnAssign():         analyze_nodeann_assign(node)
        case ast.AugAssign():         analyze_nodeaug_assign(node)
        case ast.Raise():             analyze_noderaise(node)
        case ast.Assert():            analyze_nodeassert(node)
        case ast.Import():            analyze_nodeimport(node)
        case ast.ImportFrom():        analyze_nodeimport_from(node)
        case ast.Global():            analyze_nodeglobal(node)
        case ast.Nonlocal():          analyze_nodenonlocal(node)
        case ast.Expr():              analyze_nodeexpr_stmt(node)
        case ast.Pass():              analyze_nodepass(node)
        case ast.Break():             analyze_nodebreak(node)
        case ast.Continue():          analyze_nodecontinue(node)
        case ast.If():                analyze_nodeif(node)
        case ast.For():               analyze_nodefor(node)
        case ast.AsyncFor():          analyze_nodeasync_for(node)
        case ast.While():             analyze_nodewhile(node)
        case ast.With():              analyze_nodewith(node)
        case ast.AsyncWith():         analyze_nodeasync_with(node)
        case ast.Match():             analyze_nodematch(node)
        case ast.Try():               analyze_nodetry(node)
        case ast.TryStar():           analyze_nodetry_star(node)
        case _:
            print(f"Unhandled statement: {type(node).__name__}")

def analyze_nodefunction_def(node: ast.FunctionDef) -> None:
    analyze_nodeannotation(node.returns)
    for arg in node.args.args:
        analyze_nodeannotation(arg.annotation)
    for stmt in node.body:
        analyze_node(stmt)

def analyze_nodeasync_function_def(_: ast.AsyncFunctionDef) -> None:
    pass

def analyze_nodeclass_def(node: ast.ClassDef) -> None:
    for stmt in node.body:
        analyze_node(stmt)

def analyze_nodereturn(node: ast.Return) -> None:
    if node.value:
        analyze_nodeexpression(node.value)

def analyze_nodedelete(_: ast.Delete) -> None:
    pass

def analyze_nodeassign(node: ast.Assign) -> None:
    analyze_nodeexpression(node.value)
    for target in node.targets:
        analyze_nodeexpression(target)

def analyze_nodeann_assign(node: ast.AnnAssign) -> None:
    analyze_nodeexpression(node.target)
    analyze_nodeannotation(node.annotation)
    if node.value:
        analyze_nodeexpression(node.value)

def analyze_nodeaug_assign(node: ast.AugAssign) -> None:
    analyze_nodeexpression(node.target)
    analyze_nodeexpression(node.value)

def analyze_noderaise(_: ast.Raise) -> None:
    pass

def analyze_nodeassert(_: ast.Assert) -> None:
    pass

def analyze_nodeimport(_: ast.Import) -> None:
    pass

def analyze_nodeimport_from(_: ast.ImportFrom) -> None:
    pass

def analyze_nodeglobal(_: ast.Global) -> None:
    pass

def analyze_nodenonlocal(_: ast.Nonlocal) -> None:
    pass

def analyze_nodeexpr_stmt(node: ast.Expr) -> None:
    analyze_nodeexpression(node.value)

def analyze_nodepass(_: ast.Pass) -> None:
    pass

def analyze_nodebreak(_: ast.Break) -> None:
    pass

def analyze_nodecontinue(_: ast.Continue) -> None:
    pass

def analyze_nodeif(node: ast.If) -> None:
    analyze_nodeexpression(node.test)
    for stmt in node.body:
        analyze_node(stmt)
    if node.orelse:
        if len(node.orelse) == 1 and isinstance(node.orelse[0], ast.If):
            analyze_nodeif(node.orelse[0])
        else:
            for stmt in node.orelse:
                analyze_node(stmt)

def analyze_nodefor(node: ast.For) -> None:
    analyze_nodeexpression(node.target)
    analyze_nodeexpression(node.iter)
    for stmt in node.body:
        analyze_node(stmt)

def analyze_nodeasync_for(_: ast.AsyncFor) -> None:
    pass

def analyze_nodewhile(node: ast.While) -> None:
    analyze_nodeexpression(node.test)
    for stmt in node.body:
        analyze_node(stmt)

def analyze_nodewith(_: ast.With) -> None:
    pass

def analyze_nodeasync_with(_: ast.AsyncWith) -> None:
    pass

def analyze_nodematch(_: ast.Match) -> None:
    pass

def analyze_nodetry(_: ast.Try) -> None:
    pass

def analyze_nodetry_star(_: ast.TryStar) -> None:
    pass

# --- Expression handlers (recursive) ---

def analyze_nodeexpression(node: ast.expr | None) -> None:
    match node:
        case ast.BoolOp():
            for v in node.values:
                analyze_nodeexpression(v)
        case ast.NamedExpr():
            pass
        case ast.BinOp():
            analyze_nodeexpression(node.left)
            analyze_nodeexpression(node.right)
        case ast.UnaryOp():
            analyze_nodeexpression(node.operand)
        case ast.Lambda():
            pass
        case ast.IfExp():
            analyze_nodeexpression(node.test)
            analyze_nodeexpression(node.body)
            analyze_nodeexpression(node.orelse)
        case ast.Dict():
            for k, v in zip(node.keys, node.values):
                if k is not None:
                    analyze_nodeexpression(k)
                analyze_nodeexpression(v)
        case ast.Set():
            for elt in node.elts:
                analyze_nodeexpression(elt)
        case ast.ListComp():
            pass
        case ast.SetComp():
            pass
        case ast.DictComp():
            pass
        case ast.GeneratorExp():
            pass
        case ast.Await():
            pass
        case ast.Yield():
            pass
        case ast.YieldFrom():
            pass
        case ast.Compare():
            analyze_nodeexpression(node.left)
            for comparator in node.comparators:
                analyze_nodeexpression(comparator)
        case ast.Call():
            analyze_nodeexpression(node.func)
            for arg in node.args:
                analyze_nodeexpression(arg)
        case ast.FormattedValue():
            analyze_nodeexpression(node.value)
        case ast.JoinedStr():
            for value in node.values:
                if isinstance(value, ast.FormattedValue):
                    analyze_nodeexpression(value.value)
        case ast.Constant():
            pass
        case ast.Attribute():
            analyze_nodeexpression(node.value)
        case ast.Subscript():
            analyze_nodeexpression(node.value)
            analyze_nodeexpression(node.slice)
        case ast.Starred():
            analyze_nodeexpression(node.value)
        case ast.Name():
            pass
        case ast.List():
            for elt in node.elts:
                analyze_nodeexpression(elt)
        case ast.Tuple():
            for elt in node.elts:
                analyze_nodeexpression(elt)
        case ast.Slice():
            if node.lower: analyze_nodeexpression(node.lower)
            if node.upper: analyze_nodeexpression(node.upper)
            if node.step:  analyze_nodeexpression(node.step)
        case _:
            print(f"Unhandled expression: {type(node).__name__}")

# --- Operator handlers ---

def analyze_nodeoperator(op: ast.operator) -> str:
    match op:
        case ast.Add():      return "+"
        case ast.Sub():      return "-"
        case ast.Mult():     return "*"
        case ast.Div():      return "/"
        case ast.Mod():      return "%"
        case ast.Pow():      return "**"
        case ast.FloorDiv(): return "//"
        case ast.BitAnd():   return "&"
        case ast.BitOr():    return "|"
        case ast.BitXor():   return "^"
        case ast.LShift():   return "<<"
        case ast.RShift():   return ">>"
        case ast.MatMult():  return "@"
        case _:              return f"<{type(op).__name__}>"

def analyze_nodeunary_operator(op: ast.unaryop) -> str:
    match op:
        case ast.USub():   return "-"
        case ast.UAdd():   return "+"
        case ast.Not():    return "not"
        case ast.Invert(): return "~"
        case _:            return f"<{type(op).__name__}>"

def analyze_nodebool_operator(op: ast.boolop) -> str:
    match op:
        case ast.And(): return "and"
        case ast.Or():  return "or"
        case _:         return f"<{type(op).__name__}>"

def analyze_nodecmp_operator(op: ast.cmpop) -> str:
    match op:
        case ast.Eq():    return "=="
        case ast.NotEq(): return "!="
        case ast.Lt():    return "<"
        case ast.LtE():   return "<="
        case ast.Gt():    return ">"
        case ast.GtE():   return ">="
        case ast.Is():    return "is"
        case ast.IsNot(): return "is not"
        case ast.In():    return "in"
        case ast.NotIn(): return "not in"
        case _:           return f"<{type(op).__name__}>"

def analyze_nodeannotation(node: ast.expr | None) -> str:
    match node:
        case ast.Name():
            return node.id
        case ast.Constant():
            return repr(node.value)
        case ast.Attribute():
            return f"{analyze_nodeannotation(node.value)}.{node.attr}"
        case ast.Subscript():
            obj = analyze_nodeannotation(node.value)
            index = analyze_nodeannotation(node.slice)
            return f"{obj}[{index}]"
        case ast.BinOp(op=ast.BitOr()):
            return f"{analyze_nodeannotation(node.left)} | {analyze_nodeannotation(node.right)}"
        case ast.Tuple():
            return ", ".join(analyze_nodeannotation(e) for e in node.elts)
        case None:
            return "none"
        case _:
            return f"<{type(node).__name__}>"

# --- Entry point ---

def parse(source: str) -> ast.Module:
    return ast.parse(source, "<string>", mode="exec")

def analyze(tree: ast.Module) -> None:
    for node in tree.body:
        analyze_node(node)

def dump(tree: ast.Module) -> str:
    return ast.dump(tree, indent=4)
