/*
 * compiler/src/driver/main.cpp  -  sarnc  (unified compiler + CLI driver)
 *
 * Usage:
 *   sarnc                        Open REPL editor window (F5 to run)
 *   sarnc  <file.sarn>           Compile + link next to source, then run
 *   sarnc  <file.sarn> -o <out>  Compile + link to <out>.exe  (no auto-run)
 *   sarnc  <file.sarn> --emit-ast | --emit-tokens   (debug)
 *   sarnc  install <pkg|url>     Install package
 *   sarnc  remove  <pkg>         Remove package
 *   sarnc  list                  List installed packages
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlwapi.h>
#include <commctrl.h>

#ifdef ERROR
#undef ERROR
#endif
#ifdef CONST
#undef CONST
#endif
#ifdef VOID
#undef VOID
#endif

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "kernel32.lib")

/* Compiler headers */
#include "sarn/Lexer.h"
#include "sarn/AST.h"
#include "sarn/Diagnostics.h"
#include "sarn/SemanticConfig.h"
#include "sarn/Parser.h"
#include "sarn/Resolver.h"
#include "sarn/TypeChecker.h"
#ifdef SARN_HAS_LLVM
#include "sarn/IREmitter.h"
#endif

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using std::string;
using std::vector;

static const char* SARN_VER = "0.3";

/* ─── ANSI / logging ─────────────────────────────────────────────────────── */

static void ansi_on() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  m = 0;
    if (GetConsoleMode(h, &m))
        SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

#define C0  "\033[0m"
#define CG  "\033[32m"
#define CR  "\033[31m"
#define CY  "\033[33m"
#define CC  "\033[36m"
#define CW  "\033[37m"
#define CGR "\033[90m"
#define CBL "\033[1m"

static void log_ok  (const char* f,...){va_list a;va_start(a,f);printf(CG "[OK]   " C0);vprintf(f,a);puts("");va_end(a);}
static void log_err (const char* f,...){va_list a;va_start(a,f);fprintf(stderr,CR "[ERR]  " C0);vfprintf(stderr,f,a);fputs("\n",stderr);va_end(a);}
static void log_info(const char* f,...){va_list a;va_start(a,f);printf(CC "[*]    " C0);vprintf(f,a);puts("");va_end(a);}
static void log_warn(const char* f,...){va_list a;va_start(a,f);printf(CY "[WARN] " C0);vprintf(f,a);puts("");va_end(a);}

/* ─── Path helpers ───────────────────────────────────────────────────────── */

static fs::path exe_dir() {
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path();
}

static fs::path sarn_root() {
    auto p = exe_dir();
    for (auto c : { p.parent_path().parent_path(), p.parent_path(), p })
        if (fs::exists(c / "compiler" / "CMakeLists.txt")) return c;
    return p;
}

static fs::path pkg_root_dir() { return sarn_root() / ".packages"; }

static vector<string> package_search_roots(const string& base_file) {
    vector<string> roots;
    auto push_unique = [&](const fs::path& p) {
        auto s = fs::weakly_canonical(p).string();
        if (std::find(roots.begin(), roots.end(), s) == roots.end())
            roots.push_back(s);
    };

    fs::path base = fs::absolute(base_file).parent_path();
    for (auto p = base; ; p = p.parent_path()) {
        push_unique(p);
        if (p == p.parent_path()) break;
    }

    push_unique(fs::current_path());
    push_unique(sarn_root());
    if (const char* root_env = getenv("SARN_ROOT"))
        push_unique(fs::path(root_env));

    return roots;
}

static bool resolve_package_file(const string& module_name,
                                const string& base_file,
                                fs::path& out_path) {
    vector<string> candidates;
    for (auto& root : package_search_roots(base_file)) {
        fs::path root_path(root);
        candidates.push_back((root_path / ".packages" / module_name / "__init__.sarn").string());
        candidates.push_back((root_path / ".packages" / module_name / "main.sarn").string());
        candidates.push_back((root_path / ".packages" / module_name / (module_name + ".sarn")).string());
        candidates.push_back((root_path / module_name / "__init__.sarn").string());
        candidates.push_back((root_path / module_name / "main.sarn").string());
        candidates.push_back((root_path / module_name / (module_name + ".sarn")).string());
        candidates.push_back((root_path / (module_name + ".sarn")).string());
    }

    for (auto& c : candidates) {
        fs::path p(c);
        if (fs::exists(p)) {
            out_path = p;
            return true;
        }
    }
    return false;
}

struct SarnConfig {
    string llvm_bin;
    string runtime_lib;
    string raylib_lib;
    string raylib_dll;
    string runtime_bin;
    string raylib_static_lib;
};

static SarnConfig load_config() {
    SarnConfig cfg;
    std::ifstream file(sarn_root() / "sarn.config");
    string line;
    while (std::getline(file, line)) {
        auto first = line.find_first_not_of(" \t");
        if (first == string::npos || line[first] == '#') continue;
        auto eq = line.find('=', first);
        if (eq == string::npos) continue;
        string key = line.substr(first, eq - first);
        string value = line.substr(eq + 1);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.pop_back();
        if (key == "llvm_bin") cfg.llvm_bin = value;
        else if (key == "runtime_lib") cfg.runtime_lib = value;
        else if (key == "raylib_lib") cfg.raylib_lib = value;
        else if (key == "raylib_dll") cfg.raylib_dll = value;
        else if (key == "runtime_bin") cfg.runtime_bin = value;
        else if (key == "raylib_static_lib") cfg.raylib_static_lib = value;
    }
    return cfg;
}

/* ─── Tool finding ───────────────────────────────────────────────────────── */

static string find_llvm_bin(const string& configured = "") {
    if (!configured.empty() && fs::exists(configured)) return configured;
    for (const char* d : { "C:\\Program Files\\LLVM\\bin",
                            "C:\\Program Files (x86)\\LLVM\\bin",
                            "C:\\LLVM\\bin" })
        if (fs::exists(d)) return d;
    return "";
}

static string find_in_path(const string& name) {
    char buf[MAX_PATH];
    if (SearchPathA(nullptr, name.c_str(), nullptr, MAX_PATH, buf, nullptr)) return buf;
    return "";
}

static string find_tool(const string& llvm_bin, const string& name) {
    if (!llvm_bin.empty()) {
        string p = llvm_bin + "\\" + name;
        if (fs::exists(p)) return p;
    }
    return find_in_path(name);
}

static string find_sarn_lib(const string& configured = "") {
    if (!configured.empty() && fs::exists(configured)) return configured;
    auto base = sarn_root() / "build" / "runtime";
    if (!fs::exists(base)) return "";
    auto direct = base / "sarn.lib";
    if (fs::exists(direct)) return direct.string();
    for (auto& e : fs::directory_iterator(base)) {
        for (const char* rel : { "sarn.lib", "Release\\sarn.lib" }) {
            auto p = e.path() / rel;
            if (fs::exists(p)) return p.string();
        }
    }
    return "";
}

static string find_raylib_lib(const string& configured = "") {
    if (!configured.empty() && fs::exists(configured)) return configured;
    string p = "C:\\vcpkg\\installed\\x64-windows\\lib\\raylib.lib";
    return fs::exists(p) ? p : "";
}

/* ─── Process helpers ────────────────────────────────────────────────────── */

static int run_cmd(const string& cmd, string* cap = nullptr) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE hR = nullptr, hW = nullptr;
    bool do_cap = cap != nullptr;
    if (do_cap) {
        CreatePipe(&hR, &hW, &sa, 0);
        SetHandleInformation(hR, HANDLE_FLAG_INHERIT, 0);
    }
    STARTUPINFOA si{sizeof(si)};
    if (do_cap) {
        si.dwFlags    = STARTF_USESTDHANDLES;
        si.hStdOutput = hW;
        si.hStdError  = hW;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    }
    PROCESS_INFORMATION pi{};
    string mut = cmd;
    bool launched = !!CreateProcessA(nullptr, mut.data(),
        nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
    if (do_cap) CloseHandle(hW);
    if (!launched) { if (do_cap) CloseHandle(hR); return -1; }
    if (do_cap) {
        char buf[4096]; DWORD n;
        while (ReadFile(hR, buf, sizeof(buf)-1, &n, nullptr) && n) { buf[n]='\0'; *cap += buf; }
        CloseHandle(hR);
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return (int)code;
}

/*
 * Run an exe with its own directory as the working directory.
 * This fixes relative asset paths for 3D/UI demos (fonts, textures, models).
 */
static int run_exe(const string& exe_path) {
    fs::path exe  = fs::absolute(exe_path);
    string   cwd  = exe.parent_path().string();   /* <-- key fix */
    string   cmd  = "\"" + exe.string() + "\"";

    STARTUPINFOA    si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    bool ok = !!CreateProcessA(
        nullptr, cmd.data(),
        nullptr, nullptr, FALSE, 0,
        nullptr, cwd.c_str(),        /* working directory = exe dir */
        &si, &pi);

    if (!ok) { log_err("Failed to launch: %s", exe.string().c_str()); return -1; }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return (int)code;
}

/* ─── Compiler pipeline (called internally) ──────────────────────────────── */

static string read_file(const string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { fprintf(stderr, "sarnc: cannot open '%s'\n", path.c_str()); exit(1); }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

/*
 * compile_to_ll  –  run full compiler pipeline on <input_file>, write IR to
 * <output_ll>.  Returns 0 on success, 1 on error.
 */
static int compile_to_ll(const string& input_file,
                          const string& output_ll,
                          bool override_strict   = false,
                          bool override_nonstrict= false) {
    string source = read_file(input_file);

    sarn::Directives directives = sarn::detect_directives(source, input_file);
    sarn::CompileMode mode = directives.type;
    if (override_strict)     mode = sarn::CompileMode::STRICT;
    if (override_nonstrict)  mode = sarn::CompileMode::NONSTRICT;

    sarn::DiagEngine     diag(mode);
    sarn::SemanticConfig cfg = sarn::SemanticConfig::for_mode(mode);
    cfg.mem_mode = directives.mem;

    sarn::Lexer  lexer(source, input_file, mode);
    sarn::Parser parser(lexer, diag, mode);
    auto mod = parser.parse_module(input_file);

    /* resolve imports */
    {
        std::function<void(sarn::Module&, const string&)> resolve_imports;
        resolve_imports = [&](sarn::Module& m, const string& base_file) {
            string base_dir = base_file;
            auto slash = base_dir.find_last_of("/\\");
            if (slash != string::npos) base_dir = base_dir.substr(0, slash + 1);
            else base_dir = "";
            vector<std::unique_ptr<sarn::Stmt>> expanded;
            static const std::unordered_set<string> builtins = {
                "io","math","os","string","stdata","table","fs","random",
                "datetime","path","process","json","net","sync","regex",
                "crypto","buf","thread","vec","scene","http","stdgui"
            };
            const char* root_env = getenv("SARN_ROOT");
            string root_str = root_env ? root_env : ".";
            for (auto& s : m.stmts) {
                if (auto* fi = std::get_if<sarn::FileImportDecl>(&s->v)) {
                    string fpath = base_dir + fi->path;
                    std::ifstream ff(fpath, std::ios::binary);
                    if (!ff) { fprintf(stderr,"sarnc: cannot open import '%s'\n",fpath.c_str()); exit(1); }
                    std::ostringstream ss; ss << ff.rdbuf();
                    sarn::Lexer  fl(ss.str(), fpath, mode);
                    sarn::Parser fp(fl, diag, mode);
                    auto fm = fp.parse_module(fpath);
                    resolve_imports(*fm, fpath);
                    for (auto& fs2 : fm->stmts) expanded.push_back(std::move(fs2));
                } else if (auto* id = std::get_if<sarn::ImportDecl>(&s->v)) {
                    if (!builtins.count(id->module_name)) {
                        fs::path pkg_file;
                        bool found = resolve_package_file(id->module_name, base_file, pkg_file);
                        if (found) {
                            std::ifstream ff(pkg_file, std::ios::binary);
                            if (!ff) {
                                fprintf(stderr,"sarnc: cannot open import '%s'\n", pkg_file.string().c_str());
                                exit(1);
                            }
                            std::ostringstream ss; ss << ff.rdbuf();
                            sarn::Lexer  fl(ss.str(), pkg_file.string(), mode);
                            sarn::Parser fp(fl, diag, mode);
                            auto fm = fp.parse_module(pkg_file.string());
                            resolve_imports(*fm, pkg_file.string());
                            for (auto& fs2 : fm->stmts) {
                                if (auto* fd = std::get_if<sarn::FuncDecl>(&fs2->v))
                                    if (fd->exported) fd->name = id->module_name+"."+fd->name;
                                if (auto* td = std::get_if<sarn::TypeDecl>(&fs2->v))
                                    if (td->exported) td->name = id->module_name+"."+td->name;
                                expanded.push_back(std::move(fs2));
                            }
                        } else {
                            expanded.push_back(std::move(s));
                        }
                    } else expanded.push_back(std::move(s));
                } else expanded.push_back(std::move(s));
            }
            m.stmts = std::move(expanded);
        };
        resolve_imports(*mod, input_file);
    }

    if (diag.has_errors()) { diag.dump_all(); return 1; }
    { sarn::Resolver r(diag, cfg); r.resolve(*mod); }
    if (diag.has_errors()) { diag.dump_all(); return 1; }
    { sarn::TypeChecker tc(diag, cfg); tc.check(*mod); }
    if (diag.has_errors()) { diag.dump_all(); return 1; }

#ifdef SARN_HAS_LLVM
    sarn::IREmitter emitter(diag, cfg, input_file);
    if (!emitter.emit(*mod)) { diag.dump_all(); return 1; }
    if (!emitter.write_ll(output_ll)) return 1;
    return 0;
#else
    fprintf(stderr, "sarnc: LLVM not available\n");
    return 1;
#endif
}

/* ─── Linker / build pipeline ────────────────────────────────────────────── */

/*
 * do_build  –  compile .sarn → .ll → .obj → .exe
 *
 * src_file   : path to .sarn file
 * out_exe    : desired path for the output exe (must end with .exe)
 * run_after  : if true, launch the exe after a successful build
 */
static bool do_build(const string& src_file,
                     const string& out_exe,
                     bool          run_after,
                     int           output_mode = 1) {
    SarnConfig config = load_config();
    string llvm    = find_llvm_bin(config.llvm_bin);
    string clang   = find_tool(llvm, "clang.exe");
    string sarnlib = find_sarn_lib(config.runtime_lib);
    string raylib  = find_raylib_lib(config.raylib_lib);
    // out3 currently uses the same static-CRT strategy as out2.
    if (output_mode == 3) output_mode = 2;

    if (clang.empty())   { log_err("clang.exe not found."); return false; }
    if (sarnlib.empty()) { log_err("sarn.lib not found. Build the project first."); return false; }

    fs::path src = fs::absolute(src_file);
    if (!fs::exists(src)) { log_err("Source file not found: %s", src.string().c_str()); return false; }

    /* temp dir for intermediates */
    char tmp_buf[MAX_PATH]; GetTempPathA(MAX_PATH, tmp_buf);
    string tmp       = string(tmp_buf);
    string stem      = src.stem().string();
    string ll_file   = tmp + stem + ".ll";
    string obj_file  = tmp + stem + ".obj";

    /* ── Step 1: compile .sarn → .ll ───────────────────────────────────── */
    log_info("Compiling  %s", src.filename().string().c_str());
    if (compile_to_ll(src.string(), ll_file) != 0) {
        log_err("Compilation failed.");
        return false;
    }

    /* ── Step 2: .ll → .obj ─────────────────────────────────────────────── */
    log_info("IR \xe2\x86\x92 obj...");
    string llc = find_tool(llvm, "llc.exe");
    bool   got_obj = false;
    if (!llc.empty()) {
        string out;
        got_obj = (run_cmd("\"" + llc + "\" \"" + ll_file + "\" -o \"" + obj_file + "\"", &out) == 0);
    }
    if (!got_obj) {
        string out;
        if (run_cmd("\"" + clang + "\" -x ir -c \"" + ll_file + "\" -o \"" + obj_file + "\"", &out) != 0) {
            log_err("IR -> obj failed."); fputs(out.c_str(), stderr); return false;
        }
    }

    /* ── Step 3: link → .exe ────────────────────────────────────────────── */
    log_info("Linking...");
    fs::path exe_path = fs::absolute(out_exe);
    fs::create_directories(exe_path.parent_path());

    string sys = "-lOpenGL32 -lgdi32 -lwinmm -ladvapi32 -lUser32 -lShell32 -lws2_32 -lwinhttp";
    string crt = output_mode >= 2
        ? "-Wl,/NODEFAULTLIB:msvcrt -Wl,/NODEFAULTLIB:ucrt -Wl,/NODEFAULTLIB:vcruntime -llibcmt -llibucrt -llibvcruntime"
        : "-lmsvcrt -lucrt -lvcruntime";
    string nod = "-Wl,/NODEFAULTLIB:libcmt";

    string link = "\"" + clang + "\" \"" + obj_file + "\""
                  " \"" + sarnlib + "\"";
    if (!raylib.empty()) link += " \"" + raylib + "\"";
    link += " " + sys + " " + nod + " " + crt;
    link += " -o \"" + exe_path.string() + "\"";

    string link_out;
    int link_rc = run_cmd(link, &link_out);
    for (auto& line : [&]{ vector<string> v; std::istringstream ss(link_out); string l;
                            while(std::getline(ss,l)) v.push_back(l); return v; }()) {
        if (line.find("error") != string::npos)   fprintf(stderr, CR "%s" C0 "\n", line.c_str());
        else if (line.find("warning") != string::npos) fprintf(stderr, CY "%s" C0 "\n", line.c_str());
    }
    if (link_rc != 0 || !fs::exists(exe_path)) { log_err("Linking failed."); return false; }

    /* ── Step 4: copy raylib.dll next to exe ────────────────────────────── */
    fs::path raylib_dll = config.raylib_dll.empty()
        ? fs::path("C:\\vcpkg\\installed\\x64-windows\\bin\\raylib.dll")
        : fs::path(config.raylib_dll);
    fs::path runtime_bin = config.runtime_bin.empty()
        ? raylib_dll.parent_path()
        : fs::path(config.runtime_bin);
    if (output_mode <= 2) for (const char* dll : {"raylib.dll", "glfw3.dll"}) {
        fs::path source = runtime_bin / dll;
        if (fs::exists(source)) {
            std::error_code ec;
            fs::copy_file(source, exe_path.parent_path() / dll,
                          fs::copy_options::overwrite_existing, ec);
        }
    }

    log_ok("Built: %s", exe_path.string().c_str());

    /* ── Step 5: run (working dir = exe directory so assets resolve) ─────── */
    if (run_after) {
        printf("\n");
        run_exe(exe_path.string());
    }

    /* cleanup intermediates */
    DeleteFileA(ll_file.c_str());
    DeleteFileA(obj_file.c_str());

    return true;
}

/* ─── Package management ─────────────────────────────────────────────────── */

static void cmd_pkg_install(const string& pkg) {
    auto pr = pkg_root_dir();
    fs::create_directories(pr);

    string dep_name = pkg;
    string dep_ver  = "0.0.0";
    auto at = pkg.find('@');
    if (at != string::npos) {
        dep_name = pkg.substr(0, at);
        dep_ver = pkg.substr(at + 1);
    }

    string url;
    if (pkg.rfind("https://",0)==0 || pkg.rfind("http://",0)==0) {
        url = pkg;
        dep_name = fs::path(pkg).stem().string();
    } else {
        auto reg_path = sarn_root() / "packageReg.json";
        if (!fs::exists(reg_path)) { log_err("packageReg.json not found"); return; }
        std::ifstream rf(reg_path);
        string content((std::istreambuf_iterator<char>(rf)), {});

        string key = "\"" + dep_name + "\"";
        auto pos = content.find(key);
        if (pos == string::npos) { log_err("Package '%s' not in registry", dep_name.c_str()); return; }
        pos = content.find(':', pos + key.size());
        pos = content.find('"', pos); auto eq = content.find('"', pos+1);
        url = content.substr(pos+1, eq-pos-1);
    }

    string dest = (pr / dep_name).string();
    if (fs::exists(dest)) fs::remove_all(dest);
    if (run_cmd("git clone \""+url+"\" \""+dest+"\"") == 0) {
        if (dep_ver == "0.0.0") {
            fs::path pkg_manifest = fs::path(dest) / "sarn.json";
            if (fs::exists(pkg_manifest)) {
                std::ifstream mf(pkg_manifest);
                std::string js((std::istreambuf_iterator<char>(mf)), {});
                auto vpos = js.find("\"version\"");
                if (vpos != std::string::npos) {
                    auto cpos = js.find(':', vpos);
                    auto q1 = js.find('"', cpos + 1);
                    auto q2 = js.find('"', q1 + 1);
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        dep_ver = js.substr(q1 + 1, q2 - q1 - 1);
                }
            }
        }

        std::ofstream meta(fs::path(dest) / "sarn.package");
        meta << "{\n"
             << "  \"name\": \"" << dep_name << "\",\n"
             << "  \"version\": \"" << dep_ver << "\",\n"
             << "  \"url\": \"" << url << "\"\n"
             << "}\n";
        log_ok("Installed %s@%s", dep_name.c_str(), dep_ver.c_str());
    } else {
        log_err("Clone failed.");
    }
}

static void cmd_pkg_remove(const string& pkg) {
    auto d = pkg_root_dir() / pkg;
    if (fs::exists(d)) { fs::remove_all(d); log_ok("Removed %s", pkg.c_str()); }
    else log_err("'%s' not installed", pkg.c_str());
}

static void cmd_pkg_list() {
    auto pr = pkg_root_dir();
    if (!fs::exists(pr)) { puts("No packages installed."); return; }
    printf(CC "Installed packages:\n" C0);
    for (auto& e : fs::directory_iterator(pr))
        if (e.is_directory())
            printf("  " CW "%s" C0 "\n", e.path().filename().string().c_str());
}

/* ─── Win32 REPL editor ──────────────────────────────────────────────────── */

static HWND   g_hEdit   = nullptr;
static HWND   g_hWin    = nullptr;
static HBRUSH g_hBgBrush= nullptr;
static std::atomic<bool> g_repl_alive{false};
static std::atomic<bool> g_win_up{false};
static std::mutex        g_compile_lock;
static WNDPROC           g_orig_edit = nullptr;

static const COLORREF BG  = RGB(0x1e,0x1e,0x2e);
static const COLORREF FG  = RGB(0xcd,0xd6,0xf4);
static const COLORREF ACC = RGB(0x89,0xb4,0xfa);

static LRESULT CALLBACK EditSub(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CHAR && wp == '\t') { SendMessageA(h, EM_REPLACESEL, TRUE, (LPARAM)"    "); return 0; }
    return CallWindowProcA(g_orig_edit, h, msg, wp, lp);
}

static LRESULT CALLBACK EditorProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hEdit = CreateWindowExA(0,"EDIT","",
            WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL|
            ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL|ES_WANTRETURN,
            0,0,1,1,hWnd,(HMENU)1001,nullptr,nullptr);
        HFONT font = CreateFontA(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,FIXED_PITCH|FF_MODERN,"Consolas");
        SendMessageA(g_hEdit, WM_SETFONT, (WPARAM)font, TRUE);
        UINT ts = 16; SendMessageA(g_hEdit, EM_SETTABSTOPS, 1, (LPARAM)&ts);
        g_orig_edit = (WNDPROC)SetWindowLongPtrA(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditSub);
        SetWindowTextA(g_hEdit,
            "--!!type:strict\r\n\r\nfunction main(): int\r\n"
            "    print(\"Hello, Sarn!\")\r\n    return 0\r\nend\r\n");
        SetFocus(g_hEdit);
        break;
    }
    case WM_SIZE: {
        RECT rc; GetClientRect(hWnd, &rc);
        SetWindowPos(g_hEdit, nullptr, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
        break;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, FG); SetBkColor(dc, BG);
        if (!g_hBgBrush) g_hBgBrush = CreateSolidBrush(BG);
        return (LRESULT)g_hBgBrush;
    }
    case WM_CLOSE: g_repl_alive = false; DestroyWindow(hWnd); break;
    case WM_DESTROY: g_repl_alive = false; PostQuitMessage(0); break;
    default: return DefWindowProcA(hWnd, msg, wp, lp);
    }
    return 0;
}

static void repl_run_code() {
    std::lock_guard<std::mutex> lk(g_compile_lock);
    if (!g_hEdit) { log_err("No editor window."); return; }
    int len = GetWindowTextLengthA(g_hEdit);
    if (len == 0) { log_warn("Editor is empty."); return; }
    string code(len+1, '\0');
    GetWindowTextA(g_hEdit, code.data(), len+1);
    code.resize(len);

    char tmp[MAX_PATH]; GetTempPathA(MAX_PATH, tmp);
    string src = string(tmp) + "sarn_repl.sarn";
    string ll  = string(tmp) + "sarn_repl.ll";
    {
        std::ofstream f(src);
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i]=='\r' && i+1<code.size() && code[i+1]=='\n') continue;
            f << code[i];
        }
    }

    /* output exe next to temp source */
    string exe = string(tmp) + "sarn_repl.exe";
    if (compile_to_ll(src, ll) == 0)
        do_build(src, exe, true);
    DeleteFileA(src.c_str());
    DeleteFileA(ll.c_str());
    printf("\n");
}

static void editor_thread_fn(HINSTANCE hInst) {
    WNDCLASSA wc{};
    wc.lpfnWndProc   = EditorProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = "SarnEditorWnd";
    wc.hbrBackground = CreateSolidBrush(BG);
    wc.hCursor       = LoadCursorA(nullptr,(LPCSTR)IDC_ARROW);
    wc.hIcon         = LoadIconA  (nullptr,(LPCSTR)IDI_APPLICATION);
    RegisterClassA(&wc);

    g_hWin = CreateWindowExA(WS_EX_APPWINDOW, "SarnEditorWnd",
        "Sarn REPL  |  F5 = Run  |  type 'help' in terminal",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,CW_USEDEFAULT,960,660,
        nullptr,nullptr,hInst,nullptr);
    if (!g_hWin) return;
    ShowWindow(g_hWin, SW_SHOW);
    UpdateWindow(g_hWin);
    g_win_up = true;

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0)) {
        if (msg.message==WM_KEYDOWN && msg.wParam==VK_F5 && msg.hwnd==g_hEdit)
            std::thread(repl_run_code).detach();
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    g_repl_alive = false;
}

static int cmd_repl() {
    if (GetConsoleWindow() == nullptr) {
        AllocConsole();
        FILE* d;
        freopen_s(&d,"CONIN$","r",stdin);
        freopen_s(&d,"CONOUT$","w",stdout);
        freopen_s(&d,"CONOUT$","w",stderr);
        ansi_on();
    }
    g_repl_alive = true;
    HINSTANCE hInst = GetModuleHandleA(nullptr);
    std::thread win_t(editor_thread_fn, hInst);

    while (!g_win_up && g_repl_alive) Sleep(20);
    if (!g_repl_alive) { if (win_t.joinable()) win_t.join(); return 1; }

    printf(CC CBL
        "\n  Sarn REPL v%s\n"
        "  Write code in the editor, then:\n"
        "    [Enter]   – run from terminal\n"
        "    F5        – run from inside editor\n"
        "    clear     – reset editor\n"
        "    exit      – quit\n\n" C0, SARN_VER);

    while (g_repl_alive) {
        printf(CG "sarn> " C0); fflush(stdout);
        string line;
        if (!std::getline(std::cin, line)) break;
        if (line=="exit"||line=="quit") {
            g_repl_alive = false;
            if (g_hWin) PostMessageA(g_hWin, WM_CLOSE, 0, 0);
            break;
        }
        if (line=="clear") { if (g_hEdit) SetWindowTextA(g_hEdit,""); log_ok("Editor cleared."); continue; }
        if (line=="help")  { puts("  [Enter] run  |  F5 run from editor  |  clear  |  exit"); continue; }
        if (line.empty())  { repl_run_code(); continue; }
        log_warn("Unknown command '%s'. Type 'help'.", line.c_str());
    }
    if (win_t.joinable()) win_t.join();
    return 0;
}

/* ─── Compiler debug helpers (AST / token dump) ─────────────────────────── */

static string ind(int n) { return string((size_t)n*2,' '); }

static void print_type(const sarn::TypeNode* t, int i) {
    if (!t) { printf("%s<null-type>\n",ind(i).c_str()); return; }
    std::visit([&](auto&& v){
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,sarn::PrimitiveType>)
            printf("%sPrimitive(%s)\n",ind(i).c_str(),v.name.c_str());
        else if constexpr(std::is_same_v<T,sarn::OptionalType>)
        { printf("%sOptional\n",ind(i).c_str()); print_type(v.inner.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::UnionType>)
        { printf("%sUnion\n",ind(i).c_str()); for(auto&m:v.members)print_type(m.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::GenericType>)
        { printf("%sGeneric(%s)\n",ind(i).c_str(),v.name.c_str()); for(auto&a:v.args)print_type(a.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::PtrType>)
        { printf("%sPtr\n",ind(i).c_str()); print_type(v.pointee.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::RecordType>)
        { printf("%sRecord\n",ind(i).c_str()); for(auto&[n,tp]:v.fields){printf("%s  .%s: ",ind(i).c_str(),n.c_str());print_type(tp.get(),0);} }
        else if constexpr(std::is_same_v<T,sarn::TupleType>)
        { printf("%sTuple(%zu)\n",ind(i).c_str(),v.members.size()); for(auto&m:v.members)print_type(m.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::FuncType>)
        { printf("%sFuncType\n",ind(i).c_str()); for(auto&p:v.params)print_type(p.get(),i+1); printf("%s-> ",ind(i).c_str()); print_type(v.ret.get(),0); }
    }, t->v);
}

static void print_stmt(const sarn::Stmt* s, int depth);

static void print_expr(const sarn::Expr* e, int i) {
    if (!e) { printf("%s<null>\n",ind(i).c_str()); return; }
    std::visit([&](auto&& v){
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,sarn::NullLit>)   printf("%sNull\n",ind(i).c_str());
        else if constexpr(std::is_same_v<T,sarn::BoolLit>)  printf("%sBool(%s)\n",ind(i).c_str(),v.val?"true":"false");
        else if constexpr(std::is_same_v<T,sarn::IntLit>)   printf("%sInt(%lld)\n",ind(i).c_str(),(long long)v.val);
        else if constexpr(std::is_same_v<T,sarn::FloatLit>) printf("%sFloat(%g)\n",ind(i).c_str(),v.val);
        else if constexpr(std::is_same_v<T,sarn::StrLit>)   printf("%sStr(\"%s\")\n",ind(i).c_str(),v.val.c_str());
        else if constexpr(std::is_same_v<T,sarn::Ident>)    printf("%sIdent(%s)\n",ind(i).c_str(),v.name.c_str());
        else if constexpr(std::is_same_v<T,sarn::Binop>)  { printf("%sBinop(%s)\n",ind(i).c_str(),v.op.c_str()); print_expr(v.lhs.get(),i+1); print_expr(v.rhs.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::Unop>)   { printf("%sUnop(%s)\n",ind(i).c_str(),v.op.c_str()); print_expr(v.operand.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::Call>)   { printf("%sCall\n",ind(i).c_str()); print_expr(v.callee.get(),i+1); for(auto&a:v.args)print_expr(a.get(),i+2); }
        else if constexpr(std::is_same_v<T,sarn::Field>)  { printf("%sField(.%s)\n",ind(i).c_str(),v.name.c_str()); print_expr(v.table.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::Index>)  { printf("%sIndex\n",ind(i).c_str()); print_expr(v.table.get(),i+1); print_expr(v.key.get(),i+1); }
        else printf("%s...\n",ind(i).c_str());
    }, e->v);
}

static void print_stmt(const sarn::Stmt* s, int i) {
    if (!s) return;
    std::visit([&](auto&& v){
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,sarn::LocalDecl>)
        { printf("%sLocal(%s)\n",ind(i).c_str(),v.name.c_str()); if(v.init)print_expr(v.init.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::FuncDecl>)
        { printf("%sFunc(%s, %zu params)\n",ind(i).c_str(),v.name.c_str(),v.params.size()); for(auto&st:v.body)print_stmt(st.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::ReturnStmt>)
        { printf("%sReturn\n",ind(i).c_str()); for(auto&ex:v.values)print_expr(ex.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::IfStmt>)
        { printf("%sIf\n",ind(i).c_str()); print_expr(v.cond.get(),i+1); for(auto&st:v.then_body)print_stmt(st.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::WhileStmt>)
        { printf("%sWhile\n",ind(i).c_str()); print_expr(v.cond.get(),i+1); for(auto&st:v.body)print_stmt(st.get(),i+1); }
        else if constexpr(std::is_same_v<T,sarn::CallStmt>)
        { printf("%sCallStmt\n",ind(i).c_str()); print_expr(v.call.get(),i+1); }
        else printf("%s...\n",ind(i).c_str());
    }, s->v);
}

/* ─── main ───────────────────────────────────────────────────────────────── */

int main(int argc, char** argv) {
    ansi_on();

    /* Expose SARN_ROOT so import resolution works */
    string root = sarn_root().string();
    SetEnvironmentVariableA("SARN_ROOT", root.c_str());
    SetEnvironmentVariableA("SLUA_ROOT", root.c_str()); /* legacy compat */

    /* ── No args → REPL ──────────────────────────────────────────────── */
    if (argc == 1) return cmd_repl();

    string first = argv[1];

    /* ── Package commands ─────────────────────────────────────────────── */
    if (first == "install") {
        if (argc < 3) { log_err("Usage: sarnc install <pkg|url>"); return 1; }
        cmd_pkg_install(argv[2]); return 0;
    }
    if (first == "remove") {
        if (argc < 3) { log_err("Usage: sarnc remove <pkg>"); return 1; }
        cmd_pkg_remove(argv[2]); return 0;
    }
    if (first == "list") { cmd_pkg_list(); return 0; }
    if (first == "config") {
        SarnConfig config = load_config();
        printf("llvm_bin=%s\n", config.llvm_bin.c_str());
        printf("runtime_lib=%s\n", config.runtime_lib.c_str());
        printf("raylib_lib=%s\n", config.raylib_lib.c_str());
        printf("raylib_dll=%s\n", config.raylib_dll.c_str());
        return 0;
    }
    if (first == "version" || first == "--version") {
        printf("sarnc %s\n", SARN_VER); return 0;
    }

    /* ── .sarn file → build (+ optional run) ─────────────────────────── */
    if (first.size() > 5 &&
        first.compare(first.size()-5, 5, ".sarn") == 0) {

        /* scan remaining args */
        string out_exe;
        bool emit_tokens    = false;
        bool emit_ast       = false;
        bool override_strict= false;
        bool override_ns    = false;
        int output_mode     = 1;

        for (int i = 2; i < argc; ++i) {
            string a = argv[i];
            if (a == "-o" && i+1 < argc) {
                out_exe = argv[++i];
                /* ensure .exe extension */
                if (out_exe.size() < 4 ||
                    out_exe.compare(out_exe.size()-4,4,".exe") != 0)
                    out_exe += ".exe";
            }
            else if (a == "--emit-tokens")  emit_tokens     = true;
            else if (a == "--emit-ast")     emit_ast        = true;
            else if (a == "--strict")       override_strict = true;
            else if (a == "--nonstrict")    override_ns     = true;
            else if (a == "--out1")         output_mode     = 1;
            else if (a == "--out2")         output_mode     = 2;
            else if (a == "--out3")         output_mode     = 3;
        }

        /* debug dump modes – no build */
        if (emit_tokens) {
            string source = read_file(first);
            sarn::Directives d = sarn::detect_directives(source, first);
            sarn::CompileMode m = override_strict ? sarn::CompileMode::STRICT :
                                  override_ns     ? sarn::CompileMode::NONSTRICT : d.type;
            sarn::DiagEngine diag(m);
            sarn::Lexer lx(source, first, m);
            while (!lx.at_eof()) {
                sarn::Token t = lx.next();
                printf("  [%3d:%3d] kind=%-5d  '%s'\n",
                    t.loc.line, t.loc.col, (int)t.kind, t.text.c_str());
            }
            return 0;
        }
        if (emit_ast) {
            string source = read_file(first);
            sarn::Directives d = sarn::detect_directives(source, first);
            sarn::CompileMode m = override_strict ? sarn::CompileMode::STRICT :
                                  override_ns     ? sarn::CompileMode::NONSTRICT : d.type;
            sarn::DiagEngine     diag(m);
            sarn::SemanticConfig cfg = sarn::SemanticConfig::for_mode(m);
            sarn::Lexer  lx(source, first, m);
            sarn::Parser pr(lx, diag, m);
            auto mod = pr.parse_module(first);
            printf("Module: %s  stmts=%zu\n", first.c_str(), mod->stmts.size());
            for (auto& s : mod->stmts) print_stmt(s.get(), 1);
            return 0;
        }

        /* determine output path */
        bool run_after = out_exe.empty();
        if (out_exe.empty()) {
            /* place exe next to source file */
            fs::path src = fs::absolute(first);
            out_exe = (src.parent_path() / (src.stem().string() + ".exe")).string();
        }

        return do_build(first, out_exe, run_after, output_mode) ? 0 : 1;
    }

    /* ── Unknown ──────────────────────────────────────────────────────── */
    fprintf(stderr, CR "[ERR]  " C0 "Unknown command: %s\n", first.c_str());
    fprintf(stderr,
        "\nUsage:\n"
        "  sarnc                        Open REPL\n"
        "  sarnc <file.sarn>            Compile + link + run\n"
        "  sarnc <file.sarn> -o <exe>   Compile + link (no run)\n"
        "  --out1                       Dynamic executable + DLLs\n"
        "  --out2                       Static CRT executable\n"
        "  --out3                       Same as --out2 for now\n"
        "  sarnc <file.sarn> --emit-ast\n"
        "  sarnc install <pkg|url>\n"
        "  sarnc remove  <pkg>\n"
        "  sarnc list\n");
    return 1;
}