"""
Sarn Language CLI Driver  v0.3
"""

import os, sys, subprocess, json, shutil, tempfile, threading, argparse
from pathlib import Path

SARN_VER = "0.3"

R  = "\033[31m"; G  = "\033[32m"; Y  = "\033[33m"
C  = "\033[36m"; W  = "\033[37m"; GR = "\033[90m"
B  = "\033[1m";  X  = "\033[0m"

def ok  (m): print(f"{G}[OK]   {X}{m}")
def err (m): print(f"{R}[ERR]  {X}{m}", file=sys.stderr)
def info(m): print(f"{C}[*]    {X}{m}")
def warn(m): print(f"{Y}[WARN] {X}{m}")

def exe_dir() -> Path:
    return Path(sys.executable if getattr(sys, "frozen", False) else __file__).resolve().parent

def sarn_root() -> Path:
    p = exe_dir()
    for candidate in [p.parent.parent, p.parent, p]:
        if (candidate / "compiler" / "CMakeLists.txt").exists():
            return candidate
    return p

def find_llvm_bin() -> Path | None:
    for d in [r"C:\Program Files\LLVM\bin", r"C:\Program Files (x86)\LLVM\bin", r"C:\LLVM\bin"]:
        if Path(d).exists():
            return Path(d)
    return None

def find_tool(name: str, llvm_bin: Path | None = None) -> str | None:
    if llvm_bin:
        p = llvm_bin / name
        if p.exists():
            return str(p)
    return shutil.which(name)

def find_sarnc() -> str | None:
    root = sarn_root()
    for rel in ["build/compiler/Release/sarnc.exe", "build/compiler/sarnc.exe"]:
        p = root / rel
        if p.exists():
            return str(p)
    return shutil.which("sarnc.exe")

def find_sarn_lib() -> str | None:
    base = sarn_root() / "build" / "runtime"
    if base.exists():
        for sub in base.iterdir():
            for candidate in [sub / "sarn.lib", sub / "Release" / "sarn.lib"]:
                if candidate.exists():
                    return str(candidate)
    return None

def find_raylib() -> str | None:
    p = Path(r"C:\vcpkg\installed\x64-windows\lib\raylib.lib")
    return str(p) if p.exists() else None

def run(cmd: str, capture=False) -> tuple[int, str]:
    result = subprocess.run(cmd, shell=True, capture_output=capture, text=True)
    out = (result.stdout or "") + (result.stderr or "") if capture else ""
    return result.returncode, out

def pkg_root() -> Path:
    return sarn_root() / ".packages"

def compile_pkg_c(pkg_dir: Path, pkg_name: str) -> bool:
    lib_path = pkg_dir / f"{pkg_name}.lib"
    if lib_path.exists():
        return True
    json_path = pkg_dir / "pkg.json"
    if not json_path.exists():
        return True
    data = json.loads(json_path.read_text())
    srcs = data.get("c_sources", [])
    if not srcs:
        return True
    llvm = find_llvm_bin()
    clang = find_tool("clang.exe", llvm)
    if not clang:
        warn(f"clang.exe not found, cannot compile C sources for {pkg_name}")
        return False
    inc = sarn_root() / "runtime" / "include"
    objs = []
    for src in srcs:
        sp = pkg_dir / src
        if not sp.exists():
            continue
        op = sp.with_suffix(".obj")
        rc, _ = run(f'"{clang}" -c -O2 "{sp}" -I"{inc}" -o "{op}"')
        if rc == 0:
            objs.append(str(op))
    if not objs:
        return False
    llvm_lib = find_tool("llvm-lib.exe", llvm) or find_tool("lib.exe", llvm)
    if not llvm_lib:
        return False
    obj_str = " ".join(f'"{o}"' for o in objs)
    rc, _ = run(f'"{llvm_lib}" /nologo /OUT:"{lib_path}" {obj_str}')
    return rc == 0

def get_pkg_libs() -> list[str]:
    libs = []
    pr = pkg_root()
    if not pr.exists():
        return libs
    for sub in pr.iterdir():
        if not sub.is_dir():
            continue
        if not (sub / "__init__.sarn").exists() and not (sub / "__init__.slua").exists():
            continue
        name = sub.name
        if compile_pkg_c(sub, name):
            lib = sub / f"{name}.lib"
            if lib.exists():
                libs.append(str(lib))
    return libs

def do_build(src: str, out_exe: str = "", is_static=False, run_after=False) -> bool:
    llvm    = find_llvm_bin()
    sarnc   = find_sarnc()
    clang   = find_tool("clang.exe", llvm)
    sarnlib = find_sarn_lib()
    raylib  = find_raylib()

    if not sarnc:   err("sarnc.exe not found. Run cmake_configure.bat first."); return False
    if not clang:   err("clang.exe not found."); return False
    if not sarnlib: err("sarn.lib not found. Build the project first."); return False

    src_path = Path(src).resolve()
    if not src_path.exists():
        err(f"Source file not found: {src_path}"); return False

    stem = src_path.stem
    bin_dir = sarn_root() / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)

    ll_file  = str(bin_dir / f"{stem}.ll")
    obj_file = str(bin_dir / f"{stem}.obj")
    exe_file = out_exe if out_exe else str(bin_dir / f"{stem}.exe")

    info(f"Compiling  {src_path.name}")
    rc, _ = run(f'"{sarnc}" "{src_path}" -o "{ll_file}"')
    if rc != 0:
        err("Compilation failed."); return False

    info("IR → obj...")
    llc = find_tool("llc.exe", llvm)
    compiled_obj = False
    if llc:
        rc, _ = run(f'"{llc}" "{ll_file}" -o "{obj_file}"')
        compiled_obj = (rc == 0)
    if not compiled_obj:
        rc, out = run(f'"{clang}" -x ir -c "{ll_file}" -o "{obj_file}"', capture=True)
        if rc != 0:
            err("IR → obj failed."); print(out); return False

    info("Linking...")
    pkg_libs = get_pkg_libs()
    lib_str  = " ".join(f'"{l}"' for l in [sarnlib] + (([raylib] if raylib else [])) + pkg_libs)

    sys_libs = "-lOpenGL32 -lgdi32 -lwinmm -ladvapi32 -lUser32 -lShell32 -lGdi32 -lws2_32 -lwinhttp"
    nodef    = "-Wl,/NODEFAULTLIB:libcmt"

    if is_static:
        crt = "-Wl,/NODEFAULTLIB:msvcrt -Wl,/NODEFAULTLIB:msvcrtd -Wl,/NODEFAULTLIB:ucrt -Wl,/NODEFAULTLIB:vcruntime -lmt -libucrt -lvcruntime"
    else:
        crt = "-lmsvcrt -lucrt -lvcruntime"

    link_cmd = f'"{clang}" "{obj_file}" {lib_str} {sys_libs} {nodef} {crt} -o "{exe_file}"'
    rc, out = run(link_cmd, capture=True)
    for line in out.splitlines():
        if "error" in line:   print(f"{R}{line}{X}")
        elif "warning" in line: print(f"{Y}{line}{X}")

    if rc != 0 or not Path(exe_file).exists():
        err("Linking failed."); return False

    ok(f"Built: {exe_file}")
    if is_static:
        ok("Static build — no DLL redistribution required.")
    else:
        raylib_dll = Path(r"C:\vcpkg\installed\x64-windows\bin\raylib.dll")
        if raylib_dll.exists():
            dst = Path(exe_file).parent / "raylib.dll"
            if not dst.exists():
                shutil.copy2(raylib_dll, dst)
        warn("Dynamic build. Distribute raylib.dll and MSVC runtime DLLs alongside the exe.")

    if run_after:
        print()
        run(f'"{exe_file}"')
    return True

def cmd_install(pkg: str):
    pr = pkg_root(); pr.mkdir(parents=True, exist_ok=True)
    if pkg.startswith("http://") or pkg.startswith("https://"):
        name = Path(pkg).stem
        dest = pr / name
        rc, _ = run(f'git clone "{pkg}" "{dest}"')
        ok(f"Installed {name}") if rc == 0 else err("Clone failed.")
        return
    reg = sarn_root() / "packageReg.json"
    if not reg.exists():
        err("packageReg.json not found"); return
    data = json.loads(reg.read_text())
    if pkg not in data:
        err(f"Package '{pkg}' not in registry"); return
    url  = data[pkg]
    dest = pr / pkg
    if dest.exists():
        shutil.rmtree(dest)
    rc, _ = run(f'git clone "{url}" "{dest}"')
    if rc == 0:
        compile_pkg_c(dest, pkg)
        ok(f"Installed {pkg}")
    else:
        err("Clone failed.")

def cmd_remove(pkg: str):
    d = pkg_root() / pkg
    if d.exists():
        shutil.rmtree(d); ok(f"Removed {pkg}")
    else:
        err(f"'{pkg}' not installed")

def cmd_update(pkg: str):
    d = pkg_root() / pkg
    if not d.exists():
        err(f"'{pkg}' not installed"); return
    run(f'git -C "{d}" pull')
    compile_pkg_c(d, pkg)
    ok(f"Updated {pkg}")

def cmd_list():
    pr = pkg_root()
    if not pr.exists():
        print("No packages installed."); return
    print(f"{C}Installed packages:{X}")
    for sub in pr.iterdir():
        if sub.is_dir():
            print(f"  {W}{sub.name}{X}")

def cmd_clean():
    bin_dir = sarn_root() / "bin"
    if not bin_dir.exists():
        info("No bin directory to clean.")
        return
    removed = 0
    for ext in ["*.exe", "*.ll", "*.dll", "*.obj"]:
        for f in bin_dir.glob(ext):
            try:
                f.unlink()
                removed += 1
            except Exception as e:
                warn(f"Failed to delete {f.name}: {e}")
    if removed > 0:
        ok(f"Cleaned {removed} file(s) from bin/")
    else:
        info("No build artifacts found to clean.")

def cmd_newpkg(name: str):
    Path(name).mkdir(exist_ok=True)
    (Path(name) / "__init__.sarn").write_text(
        f"--!!type:strict\n\nexport function {name}.hello(): string\n    return \"Hello from {name}!\"\nend\n"
    )
    (Path(name) / "pkg.json").write_text(json.dumps({
        "name": name, "version": "1.0.0",
        "files": ["__init__.sarn"], "c_sources": [], "deps": []
    }, indent=4) + "\n")
    ok(f"Created package '{name}'")

def cmd_repl():
    try:
        import tkinter as tk
        from tkinter import scrolledtext
    except ImportError:
        err("tkinter not available. Install Python with tkinter support."); return 1

    root = tk.Tk()
    root.title("Sarn Editor  |  F5 = Run")
    root.configure(bg="#1e1e2e")
    root.geometry("960x660")

    font = ("Consolas", 12)
    editor = scrolledtext.ScrolledText(root, font=font, bg="#1e1e2e", fg="#cdd6f4",
                                       insertbackground="#cdd6f4", wrap=tk.NONE,
                                       undo=True, tabs=("1c",))
    editor.insert("1.0", '--!!type:strict\n\nfunction main(): int\n    print("Hello, Sarn!")\n    return 0\nend\n')
    editor.pack(fill=tk.BOTH, expand=True)

    def on_tab(e):
        editor.insert(tk.INSERT, "    ")
        return "break"
    editor.bind("<Tab>", on_tab)

    status = tk.Label(root, text="Ready", bg="#181825", fg="#89b4fa",
                      font=("Consolas", 10), anchor="w", padx=8)
    status.pack(fill=tk.X, side=tk.BOTTOM)

    compile_lock = threading.Lock()

    def run_code():
        def _run():
            with compile_lock:
                code = editor.get("1.0", tk.END)
                tmp = tempfile.NamedTemporaryFile(suffix=".sarn", delete=False, mode="w")
                tmp.write(code); tmp.close()
                status.config(text="Compiling...", fg="#f9e2af")
                root.update_idletasks()
                success = do_build(tmp.name, run_after=True)
                status.config(
                    text="Done." if success else "Build failed.",
                    fg="#a6e3a1" if success else "#f38ba8"
                )
                os.unlink(tmp.name)
        threading.Thread(target=_run, daemon=True).start()

    editor.bind("<F5>", lambda e: run_code())

    btn_frame = tk.Frame(root, bg="#181825")
    btn_frame.pack(fill=tk.X, side=tk.BOTTOM)
    tk.Button(btn_frame, text="▶ Run (F5)", command=run_code,
              bg="#89b4fa", fg="#1e1e2e", font=("Consolas", 10, "bold"),
              relief=tk.FLAT, padx=12, pady=4).pack(side=tk.LEFT, padx=4, pady=4)

    print(f"\n{C}{B}  Sarn REPL v{SARN_VER}{X}")
    print(f"  Editor open. Press F5 or click Run inside the window.\n")
    root.mainloop()
    return 0

def print_usage():
    print(f"{C}{B}Sarn CLI  v{SARN_VER}\n{X}")
    print(f"{W}Usage:{X} sarn <command> [options]\n")
    print(f"{C}Run / Build:{X}")
    print("  run    <file.sarn>                     Compile and run immediately")
    print("  build  <file.sarn> [-o out.exe]         Build dynamic exe")
    print("  build  <file.sarn> --static [-o out]    Build fully static exe")
    print("  clean                                  Remove build artifacts (.exe, .ll, .dll, .obj)\n")
    print(f"{C}Packages:{X}")
    print("  install <name|url>   Install from registry or git URL")
    print("  remove  <name>       Remove installed package")
    print("  update  <name>       Update package")
    print("  list                 List installed packages")
    print("  newpkg  <name>       Create package scaffold\n")
    print(f"{C}Other:{X}")
    print("  version              Print version")
    print("  help                 Show this message")
    print(f"\n{GR}(Run sarn with no args to open the REPL editor){X}")

def main():
    if sys.platform == "win32":
        os.system("")  # enable ANSI on Windows

    root = sarn_root()
    os.environ["SARN_ROOT"] = str(root)
    os.environ["SLUA_ROOT"]  = str(root)

    args = sys.argv[1:]

    if not args:
        sys.exit(cmd_repl())

    cmd = args[0]

    if cmd in ("version",):           print(f"Sarn v{SARN_VER}"); return
    if cmd in ("help", "--help", "-h"): print_usage(); return
    if cmd == "list":                  cmd_list(); return
    if cmd == "clean":                 cmd_clean(); return
    if cmd == "sarn":                  sys.exit(cmd_repl())

    if cmd == "install":
        if len(args) < 2: err("Usage: sarn install <pkg|url>"); sys.exit(1)
        cmd_install(args[1]); return

    if cmd == "remove":
        if len(args) < 2: err("Usage: sarn remove <pkg>"); sys.exit(1)
        cmd_remove(args[1]); return

    if cmd == "update":
        if len(args) < 2: err("Usage: sarn update <pkg>"); sys.exit(1)
        cmd_update(args[1]); return

    if cmd == "newpkg":
        if len(args) < 2: err("Usage: sarn newpkg <name>"); sys.exit(1)
        cmd_newpkg(args[1]); return

    if cmd == "run":
        if len(args) < 2: err("Usage: sarn run <file.sarn>"); sys.exit(1)
        sys.exit(0 if do_build(args[1], run_after=True) else 1)

    if cmd == "build":
        if len(args) < 2: err("Usage: sarn build <file.sarn> [--static] [-o out.exe]"); sys.exit(1)
        out_exe = ""; is_static = False
        i = 2
        while i < len(args):
            if args[i] == "--static":       is_static = True
            elif args[i] == "-o" and i+1 < len(args): out_exe = args[i+1]; i += 1
            i += 1
        sys.exit(0 if do_build(args[1], out_exe=out_exe, is_static=is_static) else 1)

    err(f"Unknown command: {cmd}"); print_usage(); sys.exit(1)

if __name__ == "__main__":
    main()