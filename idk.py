import os, re

root = r"C:\Users\rajeev\sarn-compiler"
skip_dirs = {".git", "build", ".packages"}
skip_exts = {".exe", ".lib", ".obj", ".dll", ".pdb", ".png", ".jpg", ".ico", ".bin"}

def sub(s):
    s = re.sub(r'SARN', 'SARN', s)
    s = re.sub(r'Sarn', 'Sarn', s)
    s = re.sub(r'sarn', 'sarn', s)
    return s

def replace_in_file(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            content = f.read()
        new = sub(content)
        if new != content:
            with open(path, "w", encoding="utf-8") as f:
                f.write(new)
            print(f"[CONTENT] {path}")
    except Exception as e:
        print(f"[SKIP] {path}: {e}")

# --- rename files and folders (bottom-up so children renamed before parents) ---
for dirpath, dirs, files in os.walk(root, topdown=False):
    if any(s in dirpath.split(os.sep) for s in skip_dirs):
        continue

    for fname in files:
        if os.path.splitext(fname)[1].lower() in skip_exts:
            continue
        new_fname = sub(fname)
        old = os.path.join(dirpath, fname)
        new = os.path.join(dirpath, new_fname)
        if new_fname != fname:
            os.rename(old, new)
            print(f"[RENAME FILE] {old} → {new}")
            replace_in_file(new)
        else:
            replace_in_file(old)

    for dname in dirs:
        if dname in skip_dirs:
            continue
        new_dname = sub(dname)
        if new_dname != dname:
            old = os.path.join(dirpath, dname)
            new = os.path.join(dirpath, new_dname)
            os.rename(old, new)
            print(f"[RENAME DIR]  {old} → {new}")