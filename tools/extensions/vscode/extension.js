'use strict';

const vscode = require('vscode');
const cp     = require('child_process');
const path   = require('path');
const os     = require('os');
const fs     = require('fs');

const MODULE_METHODS = {
    math: [
        { name:'sqrt', sig:'sqrt(x: number): number',                    doc:'Square root of x.' },
        { name:'sin',  sig:'sin(x: number): number',                     doc:'Sine of x in radians.' },
        { name:'cos',  sig:'cos(x: number): number',                     doc:'Cosine of x in radians.' },
        { name:'tan',  sig:'tan(x: number): number',                     doc:'Tangent of x in radians.' },
        { name:'log',  sig:'log(x: number): number',                     doc:'Natural logarithm of x.' },
        { name:'log2', sig:'log2(x: number): number',                    doc:'Base-2 logarithm of x.' },
        { name:'exp',  sig:'exp(x: number): number',                     doc:'e raised to the power x.' },
        { name:'pow',  sig:'pow(base: number, exp: number): number',     doc:'base raised to exp.' },
        { name:'inf',  sig:'inf(): number',                              doc:'Returns positive infinity.' },
        { name:'nan',  sig:'nan(): number',                              doc:'Returns NaN (Not-a-Number).' },
    ],
    io: [
        { name:'read_line',   sig:'read_line(): string',                             doc:'Read a line from stdin.' },
        { name:'read_char',   sig:'read_char(): int',                                doc:'Read one character code from stdin.' },
        { name:'clear',       sig:'clear(): void',                                   doc:'Clear the terminal screen.' },
        { name:'set_color',   sig:'set_color(color: string): void',                  doc:'Set text color: red/green/yellow/blue/magenta/cyan/white.' },
        { name:'reset_color', sig:'reset_color(): void',                             doc:'Reset terminal color.' },
        { name:'print_color', sig:'print_color(msg: string, color: string): void',  doc:'Print message in given color.' },
        { name:'flush',       sig:'flush(): void',                                   doc:'Flush stdout buffer.' },
        { name:'print',       sig:'print(s: string): void',                         doc:'Print string to stdout.' },
    ],
    os: [
        { name:'time',   sig:'time(): int',                   doc:'Unix timestamp in seconds.' },
        { name:'sleep',  sig:'sleep(ms: int): void',          doc:'Sleep for milliseconds.' },
        { name:'sleepS', sig:'sleepS(s: int): void',          doc:'Sleep for seconds.' },
        { name:'getenv', sig:'getenv(key: string): string',   doc:'Get an environment variable.' },
        { name:'system', sig:'system(cmd: string): void',     doc:'Run a shell command.' },
        { name:'cwd',    sig:'cwd(): string',                 doc:'Current working directory.' },
    ],
    string: [
        { name:'len',      sig:'len(s: string): int',                            doc:'Byte length of string.' },
        { name:'byte',     sig:'byte(s: string, i: int): int',                   doc:'Byte value at 0-based index i.' },
        { name:'char',     sig:'char(b: int): string',                           doc:'Single-character string from byte value.' },
        { name:'sub',      sig:'sub(s: string, from: int, to: int): string',     doc:'Substring, 0-based inclusive.' },
        { name:'upper',    sig:'upper(s: string): string',                       doc:'Convert to uppercase.' },
        { name:'lower',    sig:'lower(s: string): string',                       doc:'Convert to lowercase.' },
        { name:'find',     sig:'find(s: string, needle: string, from: int): int',doc:'First occurrence index, -1 if not found.' },
        { name:'trim',     sig:'trim(s: string): string',                        doc:'Trim leading and trailing whitespace.' },
        { name:'to_int',   sig:'to_int(s: string): int',                        doc:'Parse integer from string.' },
        { name:'to_float', sig:'to_float(s: string): number',                   doc:'Parse float from string.' },
        { name:'concat',   sig:'concat(a: string, b: string): string',          doc:'Concatenate two strings.' },
    ],
    stdata: [
        { name:'typeof',    sig:'typeof(v: any): string',                doc:'Returns type name: "int" "number" "bool" "string" "null".' },
        { name:'tostring',  sig:'tostring(v: any): string',              doc:'Convert any value to string representation.' },
        { name:'tointeger', sig:'tointeger(v: any): int',                doc:'Convert value to integer.' },
        { name:'tofloat',   sig:'tofloat(v: any): number',              doc:'Convert value to float.' },
        { name:'tobool',    sig:'tobool(v: any): bool',                 doc:'Non-zero/non-null = true.' },
        { name:'isnull',    sig:'isnull(v: any): bool',                 doc:'True if value is null.' },
        { name:'assert',    sig:'assert(cond: bool, msg: string): void', doc:'Panic with msg if condition is false.' },
    ],
    window: [
        { name:'init',          sig:'init(w: int, h: int, title: string): void',      doc:'Open a window. Requires: import stdgui.' },
        { name:'close',         sig:'close(): void',                                  doc:'Close the window.' },
        { name:'should_close',  sig:'should_close(): int',                            doc:'Returns 1 if the window should close.' },
        { name:'begin_drawing', sig:'begin_drawing(): void',                          doc:'Start a render frame.' },
        { name:'end_drawing',   sig:'end_drawing(): void',                            doc:'End frame and present to screen.' },
        { name:'clear',         sig:'clear(r: int, g: int, b: int, a: int): void',   doc:'Clear background to RGBA color.' },
        { name:'set_fps',       sig:'set_fps(fps: int): void',                        doc:'Set the target FPS.' },
        { name:'get_fps',       sig:'get_fps(): int',                                 doc:'Get current FPS.' },
        { name:'frame_time',    sig:'frame_time(): number',                           doc:'Delta time of last frame in seconds.' },
        { name:'width',         sig:'width(): int',                                   doc:'Screen width in pixels.' },
        { name:'height',        sig:'height(): int',                                  doc:'Screen height in pixels.' },
    ],
    draw: [
        { name:'rect',           sig:'rect(x,y,w,h,r,g,b,a: int): void',                    doc:'Draw a filled rectangle.' },
        { name:'rect_outline',   sig:'rect_outline(x,y,w,h,thick,r,g,b,a: int): void',      doc:'Draw a rectangle outline.' },
        { name:'circle',         sig:'circle(cx,cy: int, radius: number, r,g,b,a: int): void', doc:'Draw a filled circle.' },
        { name:'circle_outline', sig:'circle_outline(cx,cy: int, radius: number, r,g,b,a: int): void', doc:'Draw a circle outline.' },
        { name:'line',           sig:'line(x1,y1,x2,y2,thick,r,g,b,a: int): void',         doc:'Draw a line with thickness.' },
        { name:'triangle',       sig:'triangle(x1,y1,x2,y2,x3,y3,r,g,b,a: int): void',     doc:'Draw a filled triangle.' },
        { name:'text',           sig:'text(txt: string, x,y,size,r,g,b,a: int): void',      doc:'Draw text using the default font.' },
        { name:'measure_text',   sig:'measure_text(txt: string, size: int): int',            doc:'Get pixel width of rendered text.' },
        { name:'text_font',      sig:'text_font(fid: int, txt: string, x,y,size: int, spacing: number, r,g,b,a: int): void', doc:'Draw text with a custom loaded font.' },
    ],
    input: [
        { name:'key_down',      sig:'key_down(key: int): int',       doc:'Returns 1 while key is held down.' },
        { name:'key_pressed',   sig:'key_pressed(key: int): int',    doc:'Returns 1 on the frame key is first pressed.' },
        { name:'key_released',  sig:'key_released(key: int): int',   doc:'Returns 1 on the frame key is released.' },
        { name:'mouse_x',       sig:'mouse_x(): int',                doc:'Mouse cursor X position in pixels.' },
        { name:'mouse_y',       sig:'mouse_y(): int',                doc:'Mouse cursor Y position in pixels.' },
        { name:'mouse_pressed', sig:'mouse_pressed(btn: int): int',  doc:'Mouse button just pressed. 0=left 1=right 2=middle.' },
        { name:'mouse_down',    sig:'mouse_down(btn: int): int',     doc:'Mouse button held down.' },
        { name:'mouse_wheel',   sig:'mouse_wheel(): number',         doc:'Mouse wheel scroll delta this frame.' },
    ],
    ui: [
        { name:'button',        sig:'button(x,y,w,h: int, label: string): int',                      doc:'Clickable button. Returns 1 when clicked.' },
        { name:'label',         sig:'label(x,y,w,h: int, text: string): void',                       doc:'Static non-interactive text label.' },
        { name:'checkbox',      sig:'checkbox(x,y,size: int, text: string, checked: int): int',      doc:'Toggle checkbox. Returns new checked state.' },
        { name:'slider',        sig:'slider(x,y,w,h: int, min,max,val: number): number',             doc:'Drag slider. Returns new value.' },
        { name:'progress_bar',  sig:'progress_bar(x,y,w,h: int, val,max: number): void',            doc:'Progress bar display.' },
        { name:'panel',         sig:'panel(x,y,w,h: int, title: string): void',                      doc:'Background panel with title bar.' },
        { name:'text_input',    sig:'text_input(x,y,w,h: int, buf: string, bufSize,active: int): int', doc:'Text input field. Returns active state.' },
        { name:'set_font_size', sig:'set_font_size(size: int): void',                                doc:'Set font size for all UI elements.' },
        { name:'set_accent',    sig:'set_accent(r,g,b: int): void',                                  doc:'Set UI accent color (0-255 per channel).' },
    ],
    font: [
        { name:'load',   sig:'load(path: string, size: int): int',  doc:'Load font from file path. Returns font ID or -1 on failure.' },
        { name:'unload', sig:'unload(id: int): void',               doc:'Unload a previously loaded font by ID.' },
    ],
};

const KEYWORDS_CONTROL = ['if','then','else','elseif','end','for','while','do','repeat','until','return','break','continue','and','or','not'];
const KEYWORDS_DECL = ['local','const','global','function','type','extern','export','import','enum','defer','in','comptime','module'];
const KEYWORDS_MEM = ['alloc','free','alloc_typed','stack_alloc','deref','store','addr','cast','ptr_cast','panic','typeof','sizeof'];
const CONSTANTS = ['null','true','false'];
const TYPES = ['int','int4','int8','int16','int32','int64','uint8','uint16','uint32','uint64','number','float','double','string','bool','void','any','ptr','char','byte','table'];

let diagnosticCollection;
let lintTimer = null;

function isSluaDocument(doc) {
    if (!doc) return false;
    return doc.fileName.toLowerCase().endsWith('.slua');
}

function getCompilerPath() {
    const cfg = vscode.workspace.getConfiguration('slua');
    const val = cfg.get('compilerPath', '').trim();
    if (val) return val;
    const folders = vscode.workspace.workspaceFolders;
    if (folders) {
        for (const f of folders) {
            const p = path.join(f.uri.fsPath, 'build', 'compiler', 'sluac.exe');
            try { fs.accessSync(p); return p; } catch {}
        }
    }
    return 'sluac.exe';
}

function getProjectRoot() {
    const cfg = vscode.workspace.getConfiguration('slua');
    const val = cfg.get('sluaRoot', '').trim();
    if (val) return val;
    const folders = vscode.workspace.workspaceFolders;
    return folders ? folders[0].uri.fsPath : '';
}

function parseCompilerOutput(stderr, document) {
    const diags = [];
    for (const raw of stderr.split('\n')) {
        const line = raw.trim();
        if (!line.startsWith('[')) continue;

        let m = line.match(/^\[([EWN])\w+\]\s+\S+?:(\d+):(\d+)\s+(.+)$/);
        if (m) {
            const sev = m[1] === 'E' ? vscode.DiagnosticSeverity.Error
                : m[1] === 'W' ? vscode.DiagnosticSeverity.Warning:vscode.DiagnosticSeverity.Information;
            const ln  = Math.max(0, parseInt(m[2]) - 1);
            const col = Math.max(0, parseInt(m[3]) - 1);
            const lineText = ln < document.lineCount ? document.lineAt(ln).text : '';
            const endCol = Math.max(col + 1, lineText.length);
            diags.push(new vscode.Diagnostic(
                new vscode.Range(ln, col, ln, endCol),
                m[4].trim(), sev
            ));
            continue;
        }

        m = line.match(/^\[([EWN])\w+\]\s+\S+?:\s+(.+)$/);
        if (m) {
            const sev = m[1] === 'E' ? vscode.DiagnosticSeverity.Error
                    :m[1] === 'W' ? vscode.DiagnosticSeverity.Warning:vscode.DiagnosticSeverity.Information;
            diags.push(new vscode.Diagnostic(
                new vscode.Range(0, 0, 0, document.lineAt(0).text.length || 1),
                m[2].trim(), sev
            ));
        }
    }
    return diags;
}

function runLintOnFile(document) {
    if (!isSluaDocument(document)) return;
    const compiler = getCompilerPath();
    const cwd      = getProjectRoot() || path.dirname(document.uri.fsPath);
    const tmpOut   = path.join(os.tmpdir(), `sluac_lint_${Date.now()}.ll`);
    const proc     = cp.spawn(compiler, [document.uri.fsPath, '-o', tmpOut], { cwd });
    let stderr = '';
    proc.stderr.on('data', chunk => { stderr += chunk.toString(); });
    proc.on('close', () => {
        try { fs.unlinkSync(tmpOut); } catch {}
        diagnosticCollection.set(document.uri, parseCompilerOutput(stderr, document));
    });
    proc.on('error', () => {});
}

function runLintOnContent(document) {
    if (!isSluaDocument(document)) return;
    const compiler = getCompilerPath();
    const cwd      = getProjectRoot() || path.dirname(document.uri.fsPath);
    const tmpIn    = path.join(os.tmpdir(), `sluac_in_${Date.now()}.slua`);
    const tmpOut   = path.join(os.tmpdir(), `sluac_out_${Date.now()}.ll`);
    try { fs.writeFileSync(tmpIn, document.getText(), 'utf8'); } catch { return; }
    const proc = cp.spawn(compiler, [tmpIn, '-o', tmpOut], { cwd });
    let stderr = '';
    proc.stderr.on('data', chunk => { stderr += chunk.toString(); });
    proc.on('close', () => {
        try { fs.unlinkSync(tmpIn);  } catch {}
        try { fs.unlinkSync(tmpOut); } catch {}
        diagnosticCollection.set(document.uri, parseCompilerOutput(stderr, document));
    });
    proc.on('error', () => {
        try { fs.unlinkSync(tmpIn);  } catch {}
        try { fs.unlinkSync(tmpOut); } catch {}
    });
}

// ─── Completion 

class SluaCompletionProvider {
    provideCompletionItems(document, position) {
        const lineText   = document.lineAt(position).text;
        const textBefore = lineText.substring(0, position.character);

        const modMatch = textBefore.match(/\b([a-zA-Z_]\w*)\.([a-zA-Z_]\w*)$/);
        if (modMatch && MODULE_METHODS[modMatch[1]]) {
            return MODULE_METHODS[modMatch[1]].map(m => {
                const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                item.detail        = `${modMatch[1]}.${m.sig}`;
                item.documentation = new vscode.MarkdownString(m.doc);
                return item;
            });
        }

        const items = [];

        for (const kw of [...KEYWORDS_CONTROL, ...KEYWORDS_DECL, ...KEYWORDS_MEM])
            items.push(new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword));

        for (const c of CONSTANTS)
            items.push(new vscode.CompletionItem(c, vscode.CompletionItemKind.Constant));

        for (const t of TYPES) {
            const it = new vscode.CompletionItem(t, vscode.CompletionItemKind.TypeParameter);
            it.detail = 'S Lua built-in type';
            items.push(it);
        }

        for (const mod of Object.keys(MODULE_METHODS)) {
            const it = new vscode.CompletionItem(mod, vscode.CompletionItemKind.Module);
            const needs = ['window','draw','input','ui','font'].includes(mod) ? 'stdgui' : mod;
            it.detail = `module — requires import ${needs}`;
            items.push(it);
        }

        const printItem = new vscode.CompletionItem('print', vscode.CompletionItemKind.Function);
        printItem.insertText    = new vscode.SnippetString('print(${1:value})');
        printItem.detail        = 'print(value: any): void';
        printItem.documentation = new vscode.MarkdownString('Print a value followed by a newline.');
        items.push(printItem);

        const text = document.getText();
        const seen = new Set(items.map(i => String(i.label)));
        const addSymbols = (re, kind) => {
            let m;
            while ((m = re.exec(text)) !== null) {
                if (!seen.has(m[1])) {
                    items.push(new vscode.CompletionItem(m[1], kind));
                    seen.add(m[1]);
                }
            }
        };
        addSymbols(/\blocal\s+([a-zA-Z_]\w*)/g,    vscode.CompletionItemKind.Variable);
        addSymbols(/\bconst\s+([a-zA-Z_]\w*)/g,    vscode.CompletionItemKind.Constant);
        addSymbols(/\bglobal\s+([a-zA-Z_]\w*)/g,   vscode.CompletionItemKind.Variable);
        addSymbols(/\bfunction\s+([a-zA-Z_]\w*(?:\.[a-zA-Z_]\w*)?)/g, vscode.CompletionItemKind.Function);
        addSymbols(/\btype\s+([a-zA-Z_]\w*)/g,     vscode.CompletionItemKind.Class);
        addSymbols(/\benum\s+([a-zA-Z_]\w*)/g,     vscode.CompletionItemKind.Enum);

        return items;
    }
}

// ─── Hover effects

class SluaHoverProvider {
    provideHover(document, position) {
        const wordRange = document.getWordRangeAtPosition(position, /[a-zA-Z_]\w*/);
        if (!wordRange) return null;
        const word     = document.getText(wordRange);
        const lineText = document.lineAt(position).text;

        // method hover: cursor on "sqrt" in "math.sqrt"
        if (wordRange.start.character > 0 && lineText[wordRange.start.character - 1] === '.') {
            const prefix   = lineText.substring(0, wordRange.start.character - 1);
            const modMatch = prefix.match(/\b([a-zA-Z_]\w*)$/);
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

        // module hover: cursor on "math"
        if (MODULE_METHODS[word]) {
            const lines = MODULE_METHODS[word].map(m => `- \`${m.name}\` — ${m.doc}`).join('\n');
            return new vscode.Hover(
                new vscode.MarkdownString(`**module \`${word}\`**\n\n${lines}`),
                wordRange
            );
        }

        // Type hover
        const typeInfo = {
            int:    '64-bit signed integer. Aliases: int8 int16 int32 int64.',
            number: '64-bit double precision float. Aliases: float double.',
            string: 'Immutable string (char* pointer).',
            bool:   'Boolean — true or false.',
            void:   'No value. Only valid as function return type.',
            any:    'Dynamic untyped value.',
            ptr:    'Pointer type: ptr<T>',
            table:  'Dynamic hash table.',
            uint8:'8-bit unsigned', uint16:'16-bit unsigned', uint32:'32-bit unsigned', uint64:'64-bit unsigned',
            int8:'8-bit signed', int16:'16-bit signed', int32:'32-bit signed', int64:'64-bit signed (= int)',
            char:'char (alias: int8)', byte:'byte (alias: uint8)',
        };
        if (typeInfo[word])
            return new vscode.Hover(
                new vscode.MarkdownString(`**type** \`${word}\` — ${typeInfo[word]}`),
                wordRange
            );

        // Keyword hover
        const kwInfo = {
            local:      'Mutable local variable: `local name: Type = value`',
            const:      'Immutable local constant: `const NAME: Type = value`',
            global:     'Module-level global: `global name: Type = value`',
            function:   'Function declaration: `function name(params): RetType ... end`',
            import:     'Import module: `import math`  Available: math os io string stdata stdgui',
            type:       'Record type: `type Name = { field: Type, ... }`',
            enum:       'Enumeration: `enum Name = { A = 0, B, C }`',
            defer:      'Deferred cleanup: `defer free(ptr)` — runs when scope exits',
            alloc_typed:'Heap allocate: `alloc_typed(Type, count): ptr<Type>`',
            deref:      'Dereference: `deref(ptr): T`',
            store:      'Write through pointer: `store(ptr, value)`',
            cast:       'Type cast: `cast(TargetType, expr)`',
            panic:      'Abort: `panic("message")`',
            null:       'Null pointer / zero value literal.',
            true:       'Boolean true literal.',
            false:      'Boolean false literal.',
        };
        if (kwInfo[word])
            return new vscode.Hover(
                new vscode.MarkdownString(`**S Lua** — ${kwInfo[word]}`),
                wordRange
            );

        return null;
    }
}

function cmdRunFile() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        vscode.window.showErrorMessage('S Lua: No editor is active.');
        return;
    }
    const doc = editor.document;
    if (!isSluaDocument(doc)) {
        vscode.window.showErrorMessage('S Lua: Active file is not a .slua file.');
        return;
    }

    doc.save().then(() => {
        const root = getProjectRoot();
        const ps1  = path.join(root, 'slua.ps1');
        const rel  = path.relative(root, doc.uri.fsPath).replace(/\\/g, '/');

        let terminal = vscode.window.terminals.find(t => t.name === 'S Lua');
        if (!terminal) {
            terminal = vscode.window.createTerminal({
                name: 'S Lua',
                cwd: root,
                shellPath: 'powershell.exe',
                shellArgs: ['-ExecutionPolicy', 'Bypass']
            });
        }
        terminal.show(true);
        terminal.sendText(`& "${ps1}" Slua-Run "${rel}"`, true);
    });
}

function cmdBuildCompiler() {
    const root = getProjectRoot();
    const bat  = path.join(root, 'cmake_configure.bat');

    let terminal = vscode.window.terminals.find(t => t.name === 'S Lua Build');
    if (!terminal) {
        terminal = vscode.window.createTerminal({
            name: 'S Lua Build',
            cwd: root,
            shellPath: 'cmd.exe'
        });
    }
    terminal.show(true);
    terminal.sendText(`"${bat}"`, true);
}

function cmdRunDiagnostics() {
    const doc = vscode.window.activeTextEditor?.document;
    if (!doc || !isSluaDocument(doc)) {
        vscode.window.showErrorMessage('S Lua: No .slua file is active.');
        return;
    }
    runLintOnFile(doc);
    vscode.window.showInformationMessage('S Lua: Running diagnostics…');
}

function cmdEmitAST() {
    const doc = vscode.window.activeTextEditor?.document;
    if (!doc || !isSluaDocument(doc)) return;
    doc.save().then(() => {
        const compiler = getCompilerPath();
        const root     = getProjectRoot();
        let t = vscode.window.terminals.find(x => x.name === 'S Lua AST');
        if (!t) t = vscode.window.createTerminal({ name: 'S Lua AST', cwd: root });
        t.show(true);
        t.sendText(`& "${compiler}" "${doc.uri.fsPath}" --emit-ast`, true);
    });
}

function cmdEmitIR() {
    const doc = vscode.window.activeTextEditor?.document;
    if (!doc || !isSluaDocument(doc)) return;
    doc.save().then(() => {
        const compiler = getCompilerPath();
        const root     = getProjectRoot();
        const outLL    = doc.uri.fsPath.replace(/\.slua$/i, '.ll');
        let t = vscode.window.terminals.find(x => x.name === 'S Lua IR');
        if (!t) t = vscode.window.createTerminal({ name: 'S Lua IR', cwd: root });
        t.show(true);
        t.sendText(`& "${compiler}" "${doc.uri.fsPath}" -o "${outLL}"`, true);
    });
}



function activate(context) {

    diagnosticCollection = vscode.languages.createDiagnosticCollection('slua');
    context.subscriptions.push(diagnosticCollection);

    context.subscriptions.push(
        vscode.workspace.onDidOpenTextDocument(doc => {
            if (isSluaDocument(doc)) runLintOnFile(doc);
        })
    );

    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument(doc => {
            const cfg = vscode.workspace.getConfiguration('slua');
            if (isSluaDocument(doc) && cfg.get('lintOnSave', true))
                runLintOnFile(doc);
        })
    );

    context.subscriptions.push(
        vscode.workspace.onDidChangeTextDocument(ev => {
            if (!isSluaDocument(ev.document)) return;
            const cfg = vscode.workspace.getConfiguration('slua');
            if (!cfg.get('lintOnChange', true)) return;
            const delay = cfg.get('lintDelay', 700);
            if (lintTimer) clearTimeout(lintTimer);
            lintTimer = setTimeout(() => runLintOnContent(ev.document), delay);
        })
    );

    context.subscriptions.push(
        vscode.workspace.onDidCloseTextDocument(doc =>
            diagnosticCollection.delete(doc.uri)
        )
    );

    const selector = [
        { language: 'slua' },
        { scheme: 'file', pattern: '**/*.slua' }
    ];

    context.subscriptions.push(
        vscode.languages.registerCompletionItemProvider(
            selector,
            new SluaCompletionProvider(),
            '.', ':'
        )
    );

    context.subscriptions.push(
        vscode.languages.registerHoverProvider(selector, new SluaHoverProvider())
    );

    // --- Commands ---

    context.subscriptions.push(
        vscode.commands.registerCommand('slua.runFile',cmdRunFile)
    );
    context.subscriptions.push(
        vscode.commands.registerCommand('slua.buildCompiler',cmdBuildCompiler)
    );
    context.subscriptions.push(
        vscode.commands.registerCommand('slua.compile',cmdRunDiagnostics)
    );
    context.subscriptions.push(
        vscode.commands.registerCommand('slua.emitAST',cmdEmitAST)
    );
    context.subscriptions.push(
        vscode.commands.registerCommand('slua.emitIR',cmdEmitIR)
    );

    vscode.workspace.textDocuments.forEach(doc => {
        if (isSluaDocument(doc)) runLintOnFile(doc);
    });
}

function deactivate() {
    if (lintTimer) clearTimeout(lintTimer);
    if (diagnosticCollection) diagnosticCollection.dispose();
}

module.exports = { activate, deactivate };