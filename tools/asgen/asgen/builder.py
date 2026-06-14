from __future__ import annotations

from typing import Any

from .model import FunctionBinding, ParamSpec
from .policy import FRAME_INDEPENDENT_FUNCTIONS, SurfacePolicy
from .type_mapper import TypeMapper


class BindingBuilder:
    def __init__(self, definitions: dict[str, Any], mapper: TypeMapper, report: list[tuple[str, str]], policy: SurfacePolicy | None = None) -> None:
        self.definitions = definitions
        self.mapper = mapper
        self.report = report
        self.policy = policy or SurfacePolicy()

    def build(self) -> list[FunctionBinding]:
        functions: list[FunctionBinding] = []
        seen_decls: set[str] = set()
        index = 0
        for _, overloads in sorted(self.definitions.items()):
            for item in overloads:
                fn = self.build_function(item, index)
                index += 1
                if not fn:
                    continue
                if fn.script_decl in seen_decls:
                    self.report.append((fn.wrapper_name, "duplicate AngelScript declaration"))
                    continue
                seen_decls.add(fn.script_decl)
                functions.append(fn)
        return functions

    def build_function(self, item: dict[str, Any], index: int) -> FunctionBinding | None:
        funcname = str(item.get("funcname", ""))
        cimguiname = str(item.get("cimguiname", ""))

        decision = self.policy.classify(item)
        if not decision.allowed:
            self.report.append((decision.symbol, decision.reason or "skipped"))
            return None

        ret = self.mapper.map_return(str(item.get("ret", "void")), funcname)
        if not ret:
            self.report.append((cimguiname, "unsupported return type"))
            return None
        cpp_ret, as_ret, ret_kind = ret

        args = list(item.get("argsT", []))
        defaults = dict(item.get("defaults", {}))
        while args and self.mapper.map_param(args[-1], defaults) is None and self.mapper.can_drop_trailing_arg(args[-1], defaults):
            args.pop()

        params: list[ParamSpec] = []
        call_args: list[str] = []
        char_buffer_name: str | None = None
        i = 0
        while i < len(args):
            arg = args[i]
            if self.mapper.is_char_ptr(str(arg["type"])):
                if i + 1 >= len(args) or self.mapper.base_type(str(args[i + 1]["type"])) != "size_t":
                    self.report.append((cimguiname, "unsupported mutable char buffer"))
                    return None
                name = self.mapper.safe_identifier(arg["name"])
                capacity_name = self.mapper.safe_identifier(args[i + 1]["name"])
                params.append(ParamSpec(name=name, cpp_type="std::string &", as_type="string &inout", call_expr=""))
                params.append(ParamSpec(name=capacity_name, cpp_type="size_t", as_type="uint", call_expr="", default="256"))
                char_buffer_name = name
                call_args.append(f"{name}Buffer.data()")
                call_args.append(f"{name}Buffer.size()")
                i += 2
                continue

            mapped = self.mapper.map_param(arg, defaults)
            if not mapped:
                self.report.append((cimguiname, f"unsupported parameter {arg['name']}:{arg['type']}"))
                return None
            params.append(mapped)
            call_args.append(mapped.call_expr)
            i += 1

        saw_required_after_default = False
        for p in reversed(params):
            if p.default is None:
                saw_required_after_default = True
            elif saw_required_after_default:
                self.report.append((cimguiname, f"removed non-trailing default for {p.name}"))
                p.default = None

        script_params = []
        for p in params:
            text = f"{p.as_type} {p.name}"
            if p.default is not None:
                text += f" = {p.default}"
            script_params.append(text)

        wrapper_name = f"BMLImGuiAS_{index}_{self.mapper.safe_identifier(cimguiname)}"
        script_decl = f"{as_ret} {funcname}({', '.join(script_params)})"
        needs_frame_scope = funcname not in FRAME_INDEPENDENT_FUNCTIONS
        return FunctionBinding(wrapper_name, script_decl, cpp_ret, ret_kind, params, call_args, funcname, char_buffer_name, needs_frame_scope)
