'use strict';

const vscode = require('vscode');
const cp     = require('child_process');
const path   = require('path');
const os     = require('os');
const fs     = require('fs');

// ─── Module method data ───────────────────────────────────────────────────────

const KEYWORDS_CONTROL = [
    'if','then','else','elseif','end','for','while','do',
    'repeat','until','return','break','continue','and','or','not'
];
const KEYWORDS_DECL = [
    'local','const','global','function','type','extern',
    'export','import','enum','defer','in','comptime','module'
];
const KEYWORDS_MEM = [
    'alloc','free','alloc_typed','stack_alloc','deref',
    'store','addr','cast','ptr_cast','panic','typeof','sizeof'
];
const CONSTANTS = ['null','true','false'];
const TYPES = [
    'int','int8','int16','int32','int64',
    'uint8','uint16','uint32','uint64',
    'number','float','double','string','bool',
    'void','any','ptr','char','byte','table'
];

const MODULE_METHODS = {
    math: [
        { name:'sqrt', sig:'sqrt(x: number): number',            doc:'Square root of x.' },
        { name:'sin',  sig:'sin(x: number): number',             doc:'Sine (radians).' },
        { name:'cos',  sig:'cos(x: number): number',             doc:'Cosine (radians).' },
        { name:'tan',  sig:'tan(x: number): number',             doc:'Tangent (radians).' },
        { name:'log',  sig:'log(x: number): number',             doc:'Natural log.' },
        { name:'log2', sig:'log2(x: number): number',            doc:'Base-2 log.' },
        { name:'exp',  sig:'exp(x: number): number',             doc:'e^x.' },
        { name:'pow',  sig:'pow(base: number, exp: number): number', doc:'base^exp.' },
        { name:'inf',  sig:'inf(): number',                      doc:'Positive infinity.' },
        { name:'nan',  sig:'nan(): number',                      doc:'Not-a-Number.' },
    ],
    io: [
        { name:'read_line',   sig:'read_line(): string',                       doc:'Read a line from stdin.' },
        { name:'read_char',   sig:'read_char(): int',                          doc:'Read one character code from stdin.' },
        { name:'clear',       sig:'clear(): void',                             doc:'Clear the terminal.' },
        { name:'set_color',   sig:'set_color(color: string): void',            doc:'Set terminal color. Colors: red/green/yellow/blue/magenta/cyan/white.' },
        { name:'reset_color', sig:'reset_color(): void',                       doc:'Reset terminal color to default.' },
        { name:'print_color', sig:'print_color(msg: string, color: string): void', doc:'Print message in the given color.' },
        { name:'flush',       sig:'flush(): void',                             doc:'Flush stdout.' },
        { name:'print',       sig:'print(s: string): void',                   doc:'Print string to stdout.' },
    ],
    os: [
        { name:'time',   sig:'time(): int',                  doc:'Unix timestamp in seconds.' },
        { name:'sleep',  sig:'sleep(ms: int): void',         doc:'Sleep for ms milliseconds.' },
        { name:'sleepS', sig:'sleepS(s: int): void',         doc:'Sleep for s seconds.' },
        { name:'getenv', sig:'getenv(key: string): string',  doc:'Get environment variable.' },
        { name:'system', sig:'system(cmd: string): void',    doc:'Run a shell command.' },
        { name:'cwd',    sig:'cwd(): string',                doc:'Current working directory.' },
    ],
    string: [
        { name:'len',      sig:'len(s: string): int',                         doc:'Byte length of string.' },
        { name:'byte',     sig:'byte(s: string, i: int): int',                doc:'Byte value at 0-based index i.' },
        { name:'char',     sig:'char(b: int): string',                        doc:'Single-char string from byte value.' },
        { name:'sub',      sig:'sub(s: string, from: int, to: int): string',  doc:'Substring, 0-based inclusive.' },
        { name:'upper',    sig:'upper(s: string): string',                    doc:'Uppercase.' },
        { name:'lower',    sig:'lower(s: string): string',                    doc:'Lowercase.' },
        { name:'find',     sig:'find(s: string, needle: string, from: int): int', doc:'First occurrence, returns -1 if not found.' },
        { name:'trim',     sig:'trim(s: string): string',                     doc:'Trim leading/trailing whitespace.' },
        { name:'to_int',   sig:'to_int(s: string): int',                     doc:'Parse integer from string.' },
        { name:'to_float', sig:'to_float(s: string): number',                doc:'Parse float from string.' },
        { name:'concat',   sig:'concat(a: string, b: string): string',       doc:'Concatenate two strings.' },
    ],
    stdata: [
        { name:'typeof',    sig:'typeof(v: any): string',               doc:'Returns type name: "int", "number", "bool", "string", or "null".' },
        { name:'tostring',  sig:'tostring(v: any): string',             doc:'Convert any value to string.' },
        { name:'tointeger', sig:'tointeger(v: any): int',               doc:'Convert value to int.' },
        { name:'tofloat',   sig:'tofloat(v: any): number',             doc:'Convert value to number.' },
        { name:'tobool',    sig:'tobool(v: any): bool',                doc:'Truthy check - non-zero/non-null is true.' },
        { name:'isnull',    sig:'isnull(v: any): bool',                doc:'True if value is null.' },
        { name:'assert',    sig:'assert(cond: bool, msg: string): void', doc:'Panic with msg if cond is false.' },
    ],
    window: [
        { name:'init',          sig:'init(w: int, h: int, title: string): void', doc:'Open window. Requires: import stdgui' },
        { name:'close',         sig:'close(): void',                      doc:'Close window.' },
        { name:'should_close',  sig:'should_close(): int',                doc:'Returns 1 if window close was requested.' },
        { name:'begin_drawing', sig:'begin_drawing(): void',              doc:'Begin render frame.' },
        { name:'end_drawing',   sig:'end_drawing(): void',                doc:'End frame and swap buffers.' },
        { name:'clear',         sig:'clear(r: int, g: int, b: int, a: int): void', doc:'Clear background to RGBA.' },
        { name:'set_fps',       sig:'set_fps(fps: int): void',            doc:'Set target FPS.' },
        { name:'get_fps',       sig:'get_fps(): int',                     doc:'Get current FPS.' },
        { name:'frame_time',    sig:'frame_time(): number',               doc:'Delta time of last frame in seconds.' },
        { name:'width',         sig:'width(): int',                       doc:'Screen width in pixels.' },
        { name:'height',        sig:'height(): int',                      doc:'Screen height in pixels.' },
    ],
    draw: [
        { name:'rect',           sig:'rect(x,y,w,h, r,g,b,a: int): void',          doc:'Draw filled rectangle.' },
        { name:'rect_outline',   sig:'rect_outline(x,y,w,h,thick, r,g,b,a: int): void', doc:'Draw rectangle outline.' },
        { name:'circle',         sig:'circle(cx,cy: int, radius: number, r,g,b,a: int): void', doc:'Draw filled circle.' },
        { name:'circle_outline', sig:'circle_outline(cx,cy: int, radius: number, r,g,b,a: int): void', doc:'Draw circle outline.' },
        { name:'line',           sig:'line(x1,y1,x2,y2,thick, r,g,b,a: int): void', doc:'Draw a line.' },
        { name:'triangle',       sig:'triangle(x1,y1,x2,y2,x3,y3, r,g,b,a: int): void', doc:'Draw filled triangle.' },
        { name:'text',           sig:'text(txt: string, x,y,size, r,g,b,a: int): void', doc:'Draw text with default font.' },
        { name:'measure_text',   sig:'measure_text(txt: string, size: int): int',   doc:'Get pixel width of text.' },
        { name:'text_font',      sig:'text_font(fid: int, txt: string, x,y,size: int, spacing: number, r,g,b,a: int): void', doc:'Draw text with a loaded font.' },
    ],
    input: [
        { name:'key_down',      sig:'key_down(key: int): int',      doc:'Returns 1 if key is held down.' },
        { name:'key_pressed',   sig:'key_pressed(key: int): int',   doc:'Returns 1 if key was just pressed this frame.' },
        { name:'key_released',  sig:'key_released(key: int): int',  doc:'Returns 1 if key was just released.' },
        { name:'mouse_x',       sig:'mouse_x(): int',               doc:'Mouse cursor X position.' },
        { name:'mouse_y',       sig:'mouse_y(): int',               doc:'Mouse cursor Y position.' },
        { name:'mouse_pressed', sig:'mouse_pressed(btn: int): int', doc:'Returns 1 if mouse button just pressed. 0=left, 1=right, 2=middle.' },
        { name:'mouse_down',    sig:'mouse_down(btn: int): int',    doc:'Returns 1 if mouse button held.' },
        { name:'mouse_wheel',   sig:'mouse_wheel(): number',        doc:'Mouse wheel scroll delta.' },
    ],
    ui: [
        { name:'button',       sig:'button(x,y,w,h: int, label: string): int',          doc:'Button. Returns 1 on click.' },
        { name:'label',        sig:'label(x,y,w,h: int, text: string): void',           doc:'Static text label.' },
        { name:'checkbox',     sig:'checkbox(x,y,size: int, text: string, checked: int): int', doc:'Checkbox toggle. Returns new state.' },
        { name:'slider',       sig:'slider(x,y,w,h: int, min,max,val: number): number', doc:'Slider. Returns new value.' },
        { name:'progress_bar', sig:'progress_bar(x,y,w,h: int, val,max: number): void', doc:'Progress bar display.' },
        { name:'panel',        sig:'panel(x,y,w,h: int, title: string): void',          doc:'Panel with title bar background.' },
        { name:'text_input',   sig:'text_input(x,y,w,h: int, buf: string, bufSize,active: int): int', doc:'Text input field. Returns active state.' },
        { name:'set_font_size', sig:'set_font_size(size: int): void',                   doc:'Set UI element font size.' },
        { name:'set_accent',   sig:'set_accent(r,g,b: int): void',                      doc:'Set UI accent color (RGB 0-255).' },
    ],
    font: [
        { name:'load',   sig:'load(path: string, size: int): int', doc:'Load font from file. Returns font ID, or -1 on failure.' },
        { name:'unload', sig:'unload(id: int): void',              doc:'Unload font by ID.' },
    ],
};

// ─── State ────────────────────────────────────────────────────────────────────

/** @type {vscode.DiagnosticCollection} */
let diagnostics;
/** @type {NodeJS.Timeout | null} */
let changeTimer = null;

// ─── Utilities ────────────────────────────────────────────────────────────────

function isSluaFile(document) {
    if (!document) return false;
    // Check both languageId AND file extension so it works even before
    // the language is registered (i.e., before VSIX install takes effect).
    return document.languageId === 'slua' ||
           document.fileName.toLowerCase().endsWith('.slua');
}

function getCompilerPath() {
    const cfg = vscode.workspace.getConfiguration('slua');
    const configured = cfg.get('compilerPath', '').trim();
    if (configured) return configured;

    const folders = vscode.workspace.workspaceFolders;
    if (folders) {
        for (const f of folders) {
            const candidate = path.join(f.uri.fsPath, 'build', 'compiler', 'sluac.exe');
            try { fs.accessSync(candidate); return candidate; } catch {}
        }
    }
    return 'sluac.exe';
}

function getSluaRoot() {
    const cfg = vscode.workspace.getConfiguration('slua');
    const configured = cfg.get('sluaRoot', '').trim();
    if (configured) return configured;
    const folders = vscode.workspace.workspaceFolders;
    return folders ? folders[0].uri.fsPath : '';
}

function getWorkspaceRoot() {
    const folders = vscode.workspace.workspaceFolders;
    return folders ? folders[0].uri.fsPath : '';
}

// ─── Diagnostics ─────────────────────────────────────────────────────────────

/**
 * Parse sluac stderr into VS Code Diagnostics.
 * Handles both:
 *   [EE0012] filepath:line:col  message
 *   [W0001] filepath:  message  (no line:col)
 */
function parseStderr(stderr, document) {
    const result = [];
    for (const raw of stderr.split('\n')) {
        const line = raw.trim();
        if (!line.startsWith('[')) continue;

        // Pattern with line:col
        let m = line.match(/^\[([EWN])[\w]+\]\s+\S+?:(\d+):(\d+)\s+(.+)$/);
        if (m) {
            const sev = m[1] === 'E' ? vscode.DiagnosticSeverity.Error
                      : m[1] === 'W' ? vscode.DiagnosticSeverity.Warning
                      :                vscode.DiagnosticSeverity.Information;
            const ln  = Math.max(0, parseInt(m[2]) - 1);
            const col = Math.max(0, parseInt(m[3]) - 1);
            const msg = m[4].trim();
            const endCol = ln < document.lineCount
                ? Math.max(col + 1, document.lineAt(ln).text.length)
                : col + 1;
            result.push(new vscode.Diagnostic(
                new vscode.Range(ln, col, ln, endCol), msg, sev
            ));
            continue;
        }

        // Pattern without line:col (file-level warning/error)
        m = line.match(/^\[([EWN])[\w]+\]\s+\S+?:\s+(.+)$/);
        if (m) {
            const sev = m[1] === 'E' ? vscode.DiagnosticSeverity.Error
                      : m[1] === 'W' ? vscode.DiagnosticSeverity.Warning
                      :                vscode.DiagnosticSeverity.Information;
            const endCol = document.lineAt(0).text.length || 1;
            result.push(new vscode.Diagnostic(
                new vscode.Range(0, 0, 0, endCol), m[2].trim(), sev
            ));
        }
    }
    return result;
}

function lintSavedDocument(document) {
    if (!isSluaFile(document)) return;
    const compiler = getCompilerPath();
    const cwd      = getWorkspaceRoot() || path.dirname(document.uri.fsPath);
    const tmpOut   = path.join(os.tmpdir(), `slua_out_${Date.now()}.ll`);

    const proc = cp.spawn(compiler, [document.uri.fsPath, '-o', tmpOut], { cwd });
    let stderr = '';
    proc.stderr.on('data', d => { stderr += d.toString(); });
    proc.on('close', () => {
        try { fs.unlinkSync(tmpOut); } catch {}
        diagnostics.set(document.uri, parseStderr(stderr, document));
    });
    proc.on('error', () => {}); // compiler not found - silent
}

function lintDocumentContent(document) {
    if (!isSluaFile(document)) return;
    const compiler = getCompilerPath();
    const cwd      = getWorkspaceRoot() || path.dirname(document.uri.fsPath);
    const tmpIn    = path.join(os.tmpdir(), `slua_in_${Date.now()}.slua`);
    const tmpOut   = path.join(os.tmpdir(), `slua_out_${Date.now()}.ll`);

    try { fs.writeFileSync(tmpIn, document.getText(), 'utf8'); } catch { return; }

    const proc = cp.spawn(compiler, [tmpIn, '-o', tmpOut], { cwd });
    let stderr = '';
    proc.stderr.on('data', d => { stderr += d.toString(); });
    proc.on('close', () => {
        try { fs.unlinkSync(tmpIn);  } catch {}
        try { fs.unlinkSync(tmpOut); } catch {}
        diagnostics.set(document.uri, parseStderr(stderr, document));
    });
    proc.on('error', () => {
        try { fs.unlinkSync(tmpIn);  } catch {}
        try { fs.unlinkSync(tmpOut); } catch {}
    });
}

// ─── Completion provider ──────────────────────────────────────────────────────

function getLocalSymbols(document) {
    const text = document.getText();
    const symbols = [];
    const addAll = (re, kind) => {
        let m;
        while ((m = re.exec(text)) !== null) symbols.push({ name: m[1], kind });
    };
    addAll(/\blocal\s+([a-zA-Z_]\w*)/g,    vscode.CompletionItemKind.Variable);
    addAll(/\bconst\s+([a-zA-Z_]\w*)/g,    vscode.CompletionItemKind.Constant);
    addAll(/\bglobal\s+([a-zA-Z_]\w*)/g,   vscode.CompletionItemKind.Variable);
    addAll(/\bfunction\s+([a-zA-Z_]\w*(?:\.[a-zA-Z_]\w*)?)/g, vscode.CompletionItemKind.Function);
    addAll(/\btype\s+([a-zA-Z_]\w*)/g,     vscode.CompletionItemKind.Class);
    addAll(/\benum\s+([a-zA-Z_]\w*)/g,     vscode.CompletionItemKind.Enum);
    return symbols;
}

class SluaCompletionProvider {
    provideCompletionItems(document, position) {
        const lineText   = document.lineAt(position).text;
        const textBefore = lineText.substring(0, position.character);

        // Module method completion after "module."
        const moduleMatch = textBefore.match(/\b(\w+)\.(\w*)$/);
        if (moduleMatch && MODULE_METHODS[moduleMatch[1]]) {
            return MODULE_METHODS[moduleMatch[1]].map(m => {
                const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                item.detail        = `${moduleMatch[1]}.${m.sig}`;
                item.documentation = new vscode.MarkdownString(m.doc);
                return item;
            });
        }

        const items = [];

        // Keywords
        for (const kw of [...KEYWORDS_CONTROL, ...KEYWORDS_DECL, ...KEYWORDS_MEM])
            items.push(new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword));
        for (const c of CONSTANTS)
            items.push(new vscode.CompletionItem(c, vscode.CompletionItemKind.Constant));

        // Types
        for (const t of TYPES) {
            const item = new vscode.CompletionItem(t, vscode.CompletionItemKind.TypeParameter);
            item.detail = 'S Lua built-in type';
            items.push(item);
        }

        // Module names
        for (const mod of Object.keys(MODULE_METHODS)) {
            const item = new vscode.CompletionItem(mod, vscode.CompletionItemKind.Module);
            item.detail = `module (requires import ${['window','draw','input','ui','font'].includes(mod) ? 'stdgui' : mod})`;
            items.push(item);
        }

        // print built-in
        const printItem = new vscode.CompletionItem('print', vscode.CompletionItemKind.Function);
        printItem.insertText    = new vscode.SnippetString('print(${1:value})');
        printItem.detail        = 'print(value: any): void';
        printItem.documentation = new vscode.MarkdownString('Print a value followed by a newline.');
        items.push(printItem);

        // Symbols from current document
        const seen = new Set(items.map(i => String(i.label)));
        for (const sym of getLocalSymbols(document)) {
            if (!seen.has(sym.name)) {
                items.push(new vscode.CompletionItem(sym.name, sym.kind));
                seen.add(sym.name);
            }
        }

        return items;
    }
}

// ─── Hover provider ───────────────────────────────────────────────────────────

class SluaHoverProvider {
    provideHover(document, position) {
        const wordRange = document.getWordRangeAtPosition(position, /[a-zA-Z_]\w*/);
        if (!wordRange) return null;
        const word      = document.getText(wordRange);
        const lineText  = document.lineAt(position).text;

        // Method hover: math.sqrt → look up module method
        if (wordRange.start.character > 0 && lineText[wordRange.start.character - 1] === '.') {
            const prefix   = lineText.substring(0, wordRange.start.character - 1);
            const modMatch = prefix.match(/\b(\w+)$/);
            if (modMatch && MODULE_METHODS[modMatch[1]]) {
                const method = MODULE_METHODS[modMatch[1]].find(m => m.name === word);
                if (method) {
                    const md = new vscode.MarkdownString();
                    md.appendCodeblock(`${modMatch[1]}.${method.sig}`, 'slua');
                    md.appendMarkdown('\n\n' + method.doc);
                    return new vscode.Hover(md, wordRange);
                }
            }
        }

        // Type hover
        const typeInfo = {
            int:    'Integer — 64-bit signed. Aliases: int8 int16 int32 int64.',
            number: 'Floating point — 64-bit double. Aliases: float double.',
            string: 'String — immutable `char*` pointer.',
            bool:   'Boolean — `true` or `false`.',
            void:   'No value. Only valid as function return type.',
            any:    'Dynamic untyped value.',
            ptr:    'Pointer type: `ptr<T>`',
            table:  'Dynamic hash table.',
            uint8:'8-bit unsigned int', uint16:'16-bit unsigned int',
            uint32:'32-bit unsigned int', uint64:'64-bit unsigned int',
            int8:'8-bit signed int', int16:'16-bit signed int',
            int32:'32-bit signed int', int64:'64-bit signed int (same as int)',
            char:'8-bit char (alias: int8)', byte:'8-bit byte (alias: uint8)',
        };
        if (typeInfo[word])
            return new vscode.Hover(
                new vscode.MarkdownString(`**type** \`${word}\` — ${typeInfo[word]}`),
                wordRange
            );

        // Keyword hover
        const kwInfo = {
            local:      '`local name: Type = value` — mutable local variable.',
            const:      '`const NAME: Type = value` — immutable local constant.',
            global:     '`global name: Type = value` — module-level mutable variable.',
            function:   '`function name(params): RetType ... end` — function declaration.',
            import:     '`import module` — import built-in module.\n\nModules: `math` `os` `io` `string` `stdata` `stdgui`',
            type:       '`type Name = { field: Type, ... }` — record (struct) type.',
            enum:       '`enum Name = { A = 0, B, C }` — integer enumeration.',
            defer:      '`defer stmt` — run stmt when current scope exits (RAII pattern).',
            alloc_typed:'`alloc_typed(Type, count): ptr<Type>` — heap allocate typed array.',
            deref:      '`deref(ptr): T` — dereference pointer.',
            store:      '`store(ptr, value)` — write value through pointer.',
            cast:       '`cast(TargetType, expr)` — explicit type cast.',
            panic:      '`panic("message")` — abort program with message.',
            null:       'Null pointer / zero literal.',
            true:       'Boolean true.', false:'Boolean false.',
        };
        if (kwInfo[word])
            return new vscode.Hover(
                new vscode.MarkdownString(`**S Lua** — ${kwInfo[word]}`),
                wordRange
            );

        // Module hover (show all methods)
        if (MODULE_METHODS[word]) {
            const methods = MODULE_METHODS[word].map(m => `- \`${m.name}\` — ${m.doc}`).join('\n');
            return new vscode.Hover(
                new vscode.MarkdownString(`**module \`${word}\`**\n\n${methods}`),
                wordRange
            );
        }

        return null;
    }
}

// ─── Commands ─────────────────────────────────────────────────────────────────

/**
 * Run the current .slua file via slua.ps1
 * Works even when VS Code shows the file as "Lua" (before extension install).
 */
function runFileInTerminal(document) {
    if (!document) {
        vscode.window.showErrorMessage('S Lua: No file is open.');
        return;
    }
    if (!document.fileName.toLowerCase().endsWith('.slua')) {
        vscode.window.showErrorMessage('S Lua: Active file is not a .slua file.');
        return;
    }

    // Save before running so the latest content is used
    document.save().then(() => {
        const root = getSluaRoot();
        const rel  = vscode.workspace.asRelativePath(document.uri, false);
        const ps1  = path.join(root, 'slua.ps1');

        // Reuse existing terminal or create one
        let terminal = vscode.window.terminals.find(t => t.name === 'S Lua');
        if (!terminal) terminal = vscode.window.createTerminal({ name: 'S Lua', cwd: root });
        terminal.show(true);

        if (!root) {
            terminal.sendText('Write-Host "[ERROR] Set slua.sluaRoot in VS Code settings." -ForegroundColor Red', true);
            return;
        }

        terminal.sendText(
            `& powershell -ExecutionPolicy Bypass -File "${ps1}" Slua-Run "${rel}"`,
            true
        );
    });
}

function buildCompiler() {
    const root = getSluaRoot();
    const bat  = path.join(root, 'cmake_configure.bat');

    let terminal = vscode.window.terminals.find(t => t.name === 'S Lua Build');
    if (!terminal) terminal = vscode.window.createTerminal({ name: 'S Lua Build', cwd: root });
    terminal.show(true);
    terminal.sendText(`& "${bat}"`, true);
}

function emitAST(document) {
    if (!document || !document.fileName.toLowerCase().endsWith('.slua')) return;
    document.save().then(() => {
        const compiler = getCompilerPath();
        const root     = getSluaRoot();
        let terminal   = vscode.window.terminals.find(t => t.name === 'S Lua AST');
        if (!terminal)  terminal = vscode.window.createTerminal({ name: 'S Lua AST', cwd: root });
        terminal.show(true);
        terminal.sendText(`& "${compiler}" "${document.uri.fsPath}" --emit-ast`, true);
    });
}

function emitIR(document) {
    if (!document || !document.fileName.toLowerCase().endsWith('.slua')) return;
    document.save().then(() => {
        const compiler = getCompilerPath();
        const root     = getSluaRoot();
        const outLL    = document.uri.fsPath.replace(/\.slua$/i, '.ll');
        let terminal   = vscode.window.terminals.find(t => t.name === 'S Lua IR');
        if (!terminal)  terminal = vscode.window.createTerminal({ name: 'S Lua IR', cwd: root });
        terminal.show(true);
        terminal.sendText(`& "${compiler}" "${document.uri.fsPath}" -o "${outLL}"`, true);
        vscode.window.showInformationMessage(`S Lua: IR will be written to ${path.basename(outLL)}`);
    });
}

// ─── Activation ───────────────────────────────────────────────────────────────

function activate(context) {
    diagnostics = vscode.languages.createDiagnosticCollection('slua');
    context.subscriptions.push(diagnostics);

    // Lint on open
    context.subscriptions.push(
        vscode.workspace.onDidOpenTextDocument(doc => {
            if (isSluaFile(doc)) lintSavedDocument(doc);
        })
    );

    // Lint on save
    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument(doc => {
            const cfg = vscode.workspace.getConfiguration('slua');
            if (isSluaFile(doc) && cfg.get('lintOnSave', true))
                lintSavedDocument(doc);
        })
    );

    // Lint on change (debounced)
    context.subscriptions.push(
        vscode.workspace.onDidChangeTextDocument(ev => {
            const doc = ev.document;
            if (!isSluaFile(doc)) return;
            const cfg = vscode.workspace.getConfiguration('slua');
            if (!cfg.get('lintOnChange', true)) return;
            const delay = cfg.get('lintDelay', 700);
            if (changeTimer) clearTimeout(changeTimer);
            changeTimer = setTimeout(() => lintDocumentContent(doc), delay);
        })
    );

    // Clear diagnostics on close
    context.subscriptions.push(
        vscode.workspace.onDidCloseTextDocument(doc => diagnostics.delete(doc.uri))
    );

    // Completion
    context.subscriptions.push(
        vscode.languages.registerCompletionItemProvider(
            [{ language: 'slua' }, { pattern: '**/*.slua' }],
            new SluaCompletionProvider(),
            '.', ':'
        )
    );

    // Hover
    context.subscriptions.push(
        vscode.languages.registerHoverProvider(
            [{ language: 'slua' }, { pattern: '**/*.slua' }],
            new SluaHoverProvider()
        )
    );

    // Commands
    context.subscriptions.push(
        vscode.commands.registerCommand('slua.runFile', () => {
            const doc = vscode.window.activeTextEditor?.document;
            runFileInTerminal(doc);
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('slua.buildCompiler', () => {
            buildCompiler();
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('slua.compile', () => {
            const doc = vscode.window.activeTextEditor?.document;
            if (!doc || !isSluaFile(doc)) {
                vscode.window.showErrorMessage('S Lua: No .slua file is active.');
                return;
            }
            lintSavedDocument(doc);
            vscode.window.showInformationMessage('S Lua: Running diagnostics…');
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('slua.emitAST', () => {
            emitAST(vscode.window.activeTextEditor?.document);
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('slua.emitIR', () => {
            emitIR(vscode.window.activeTextEditor?.document);
        })
    );

    // Lint all already-open .slua files at startup
    vscode.workspace.textDocuments.forEach(doc => {
        if (isSluaFile(doc)) lintSavedDocument(doc);
    });
}

function deactivate() {
    if (changeTimer) clearTimeout(changeTimer);
    if (diagnostics) diagnostics.dispose();
}

module.exports = { activate, deactivate };