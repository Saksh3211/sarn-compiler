'use strict';

const vscode = require('vscode');
const cp     = require('child_process');
const path   = require('path');
const os     = require('os');
const fs     = require('fs');

// ─── Module methods database ──────────────────────────────────────────────────

const MODULE_METHODS = {
    math: [
        { name:'sqrt', sig:'sqrt(x: number): number',                doc:'Square root of x.' },
        { name:'sin',  sig:'sin(x: number): number',                 doc:'Sine of x in radians.' },
        { name:'cos',  sig:'cos(x: number): number',                 doc:'Cosine of x in radians.' },
        { name:'tan',  sig:'tan(x: number): number',                 doc:'Tangent of x in radians.' },
        { name:'log',  sig:'log(x: number): number',                 doc:'Natural logarithm of x.' },
        { name:'log2', sig:'log2(x: number): number',                doc:'Base-2 logarithm of x.' },
        { name:'exp',  sig:'exp(x: number): number',                 doc:'e raised to the power x.' },
        { name:'pow',  sig:'pow(base: number, exp: number): number', doc:'base raised to exp.' },
        { name:'inf',  sig:'inf(): number',                          doc:'Positive infinity.' },
        { name:'nan',  sig:'nan(): number',                          doc:'Not-a-Number.' },
    ],
    io: [
        { name:'read_line',   sig:'read_line(): string',                            doc:'Read a line from stdin.' },
        { name:'read_char',   sig:'read_char(): int',                               doc:'Read one character code.' },
        { name:'clear',       sig:'clear(): void',                                  doc:'Clear the terminal.' },
        { name:'set_color',   sig:'set_color(color: string): void',                 doc:'Set color: red/green/yellow/blue/magenta/cyan/white.' },
        { name:'reset_color', sig:'reset_color(): void',                            doc:'Reset terminal color.' },
        { name:'print_color', sig:'print_color(msg: string, color: string): void', doc:'Print in color.' },
        { name:'flush',       sig:'flush(): void',                                  doc:'Flush stdout.' },
        { name:'print',       sig:'print(s: string): void',                        doc:'Print string.' },
    ],
    os: [
        { name:'time',   sig:'time(): int',                  doc:'Unix timestamp.' },
        { name:'sleep',  sig:'sleep(ms: int): void',         doc:'Sleep milliseconds.' },
        { name:'sleepS', sig:'sleepS(s: int): void',         doc:'Sleep seconds.' },
        { name:'getenv', sig:'getenv(key: string): string',  doc:'Get env variable.' },
        { name:'system', sig:'system(cmd: string): void',    doc:'Run shell command.' },
        { name:'cwd',    sig:'cwd(): string',                doc:'Current directory.' },
    ],
    string: [
        { name:'len',      sig:'len(s: string): int',                            doc:'String length.' },
        { name:'byte',     sig:'byte(s: string, i: int): int',                   doc:'Byte at index i (0-based).' },
        { name:'char',     sig:'char(b: int): string',                           doc:'Char from byte value.' },
        { name:'sub',      sig:'sub(s: string, from: int, to: int): string',     doc:'Substring (0-based inclusive).' },
        { name:'upper',    sig:'upper(s: string): string',                       doc:'Uppercase.' },
        { name:'lower',    sig:'lower(s: string): string',                       doc:'Lowercase.' },
        { name:'find',     sig:'find(s: string, needle: string, from: int): int',doc:'Find first occurrence, -1 if not found.' },
        { name:'trim',     sig:'trim(s: string): string',                        doc:'Trim whitespace.' },
        { name:'to_int',   sig:'to_int(s: string): int',                        doc:'Parse integer.' },
        { name:'to_float', sig:'to_float(s: string): number',                   doc:'Parse float.' },
        { name:'concat',   sig:'concat(a: string, b: string): string',          doc:'Concatenate.' },
    ],
    stdata: [
        { name:'typeof',    sig:'typeof(v: any): string',                doc:'"int" "number" "bool" "string" "null".' },
        { name:'tostring',  sig:'tostring(v: any): string',              doc:'Convert to string.' },
        { name:'tointeger', sig:'tointeger(v: any): int',                doc:'Convert to int.' },
        { name:'tofloat',   sig:'tofloat(v: any): number',              doc:'Convert to float.' },
        { name:'tobool',    sig:'tobool(v: any): bool',                 doc:'Truthy check.' },
        { name:'isnull',    sig:'isnull(v: any): bool',                 doc:'True if null.' },
        { name:'assert',    sig:'assert(cond: bool, msg: string): void', doc:'Panic if false.' },
    ],
    window: [
        { name:'init',          sig:'init(w: int, h: int, title: string): void', doc:'Open window. Requires import stdgui.' },
        { name:'close',         sig:'close(): void',                             doc:'Close window.' },
        { name:'should_close',  sig:'should_close(): int',                       doc:'Returns 1 if should close.' },
        { name:'begin_drawing', sig:'begin_drawing(): void',                     doc:'Begin frame.' },
        { name:'end_drawing',   sig:'end_drawing(): void',                       doc:'End frame.' },
        { name:'clear',         sig:'clear(r,g,b,a: int): void',                 doc:'Clear background.' },
        { name:'set_fps',       sig:'set_fps(fps: int): void',                   doc:'Set target FPS.' },
        { name:'get_fps',       sig:'get_fps(): int',                            doc:'Current FPS.' },
        { name:'frame_time',    sig:'frame_time(): number',                      doc:'Delta time.' },
        { name:'width',         sig:'width(): int',                              doc:'Screen width.' },
        { name:'height',        sig:'height(): int',                             doc:'Screen height.' },
    ],
    draw: [
        { name:'rect',           sig:'rect(x,y,w,h,r,g,b,a: int): void',            doc:'Filled rectangle.' },
        { name:'rect_outline',   sig:'rect_outline(x,y,w,h,thick,r,g,b,a: int): void', doc:'Rectangle outline.' },
        { name:'circle',         sig:'circle(cx,cy: int, radius: number, r,g,b,a: int): void', doc:'Filled circle.' },
        { name:'circle_outline', sig:'circle_outline(cx,cy: int, radius: number, r,g,b,a: int): void', doc:'Circle outline.' },
        { name:'line',           sig:'line(x1,y1,x2,y2,thick,r,g,b,a: int): void',  doc:'Line.' },
        { name:'triangle',       sig:'triangle(x1,y1,x2,y2,x3,y3,r,g,b,a: int): void', doc:'Filled triangle.' },
        { name:'text',           sig:'text(txt: string, x,y,size,r,g,b,a: int): void', doc:'Text (default font).' },
        { name:'measure_text',   sig:'measure_text(txt: string, size: int): int',    doc:'Text pixel width.' },
        { name:'text_font',      sig:'text_font(fid: int, txt: string, x,y,size: int, spacing: number, r,g,b,a: int): void', doc:'Text with custom font.' },
    ],
    input: [
        { name:'key_down',      sig:'key_down(key: int): int',      doc:'1 while key held.' },
        { name:'key_pressed',   sig:'key_pressed(key: int): int',   doc:'1 on first press frame.' },
        { name:'key_released',  sig:'key_released(key: int): int',  doc:'1 on release frame.' },
        { name:'mouse_x',       sig:'mouse_x(): int',               doc:'Mouse X position.' },
        { name:'mouse_y',       sig:'mouse_y(): int',               doc:'Mouse Y position.' },
        { name:'mouse_pressed', sig:'mouse_pressed(btn: int): int', doc:'Mouse button just pressed.' },
        { name:'mouse_down',    sig:'mouse_down(btn: int): int',    doc:'Mouse button held.' },
        { name:'mouse_wheel',   sig:'mouse_wheel(): number',        doc:'Scroll delta.' },
    ],
    ui: [
        { name:'button',        sig:'button(x,y,w,h: int, label: string): int',        doc:'Button — returns 1 on click.' },
        { name:'label',         sig:'label(x,y,w,h: int, text: string): void',         doc:'Static label.' },
        { name:'checkbox',      sig:'checkbox(x,y,size: int, text: string, checked: int): int', doc:'Toggle.' },
        { name:'slider',        sig:'slider(x,y,w,h: int, min,max,val: number): number', doc:'Slider.' },
        { name:'progress_bar',  sig:'progress_bar(x,y,w,h: int, val,max: number): void', doc:'Progress bar.' },
        { name:'panel',         sig:'panel(x,y,w,h: int, title: string): void',        doc:'Panel with title.' },
        { name:'text_input',    sig:'text_input(x,y,w,h: int, buf: string, bufSize,active: int): int', doc:'Text input.' },
        { name:'set_font_size', sig:'set_font_size(size: int): void',                  doc:'Set UI font size.' },
        { name:'set_accent',    sig:'set_accent(r,g,b: int): void',                    doc:'Set accent color.' },
    ],
    font: [
        { name:'load',   sig:'load(path: string, size: int): int', doc:'Load font. Returns ID or -1.' },
        { name:'unload', sig:'unload(id: int): void',              doc:'Unload font by ID.' },
    ],
};

const KEYWORDS_CONTROL = ['if','then','else','elseif','end','for','while','do','repeat','until','return','break','continue','and','or','not'];
const KEYWORDS_DECL    = ['local','const','global','function','type','extern','export','import','enum','defer','in','comptime','module'];
const KEYWORDS_MEM     = ['alloc','free','alloc_typed','stack_alloc','deref','store','addr','cast','ptr_cast','panic','typeof','sizeof'];
const CONSTANTS        = ['null','true','false'];
const TYPES            = ['int','int8','int16','int32','int64','uint8','uint16','uint32','uint64','number','float','double','string','bool','void','any','ptr','char','byte','table'];

// ─── Defaults ────────────────────────────────────────────────────────────────

const THEME_DEFAULTS = {
    'directives':     { color: '#f38ba8', fontStyle: 'bold italic' },
    'controlKeywords':{ color: '#cba6f7', fontStyle: 'bold' },
    'declKeywords':   { color: '#89b4fa', fontStyle: 'bold' },
    'memoryKeywords': { color: '#fab387', fontStyle: 'bold' },
    'types':          { color: '#89dceb', fontStyle: 'italic' },
    'constants':      { color: '#fab387', fontStyle: 'bold' },
    'modules':        { color: '#94e2d5', fontStyle: '' },
    'functions':      { color: '#f9e2af', fontStyle: 'bold' },
    'functionCalls':  { color: '#89b4fa', fontStyle: '' },
    'strings':        { color: '#a6e3a1', fontStyle: '' },
    'numbers':        { color: '#fab387', fontStyle: '' },
    'comments':       { color: '#6c7086', fontStyle: 'italic' },
    'operators':      { color: '#f2cdcd', fontStyle: '' },
    'typeNames':      { color: '#89dceb', fontStyle: 'italic bold' },
};

// ─── State ────────────────────────────────────────────────────────────────────

let diagnosticCollection;
let lintTimer = null;

// ─── Utilities ────────────────────────────────────────────────────────────────

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

// ─── Theme customization ──────────────────────────────────────────────────────

/**
 * Read slua.theme.* settings and write them into editor.tokenColorCustomizations
 * so they apply immediately on top of whatever theme the user has active.
 */
function applyThemeCustomizations() {
    const cfg = vscode.workspace.getConfiguration('slua.theme');

    function get(key, field) {
        return cfg.get(`${key}.${field}`, THEME_DEFAULTS[key][field]);
    }
    function rule(scope, key) {
        return {
            scope,
            settings: {
                foreground: get(key, 'color'),
                fontStyle:  get(key, 'fontStyle'),
            }
        };
    }

    const textMateRules = [
        rule('keyword.other.directive.slua',            'directives'),
        rule('keyword.control.slua',                    'controlKeywords'),
        rule(['keyword.declaration.slua',
              'keyword.declaration.function.slua'],     'declKeywords'),
        rule('keyword.operator.memory.slua',            'memoryKeywords'),
        rule('storage.type.primitive.slua',             'types'),
        rule('constant.language.slua',                  'constants'),
        rule(['support.class.module.slua',
              'support.function.builtin.slua'],         'modules'),
        rule('entity.name.function.slua',               'functions'),
        rule('entity.name.function.call.slua',          'functionCalls'),
        rule(['string.quoted.double.slua',
              'string.quoted.single.slua'],             'strings'),
        rule(['constant.numeric.integer.slua',
              'constant.numeric.float.slua'],           'numbers'),
        rule(['comment.line.double-dash.slua',
              'comment.block.slua'],                    'comments'),
        rule(['keyword.operator.comparison.slua',
              'keyword.operator.arithmetic.slua',
              'keyword.operator.concat.slua',
              'keyword.operator.arrow.slua',
              'keyword.operator.bitwise.slua',
              'keyword.operator.assignment.slua'],      'operators'),
        rule('entity.name.type.slua',                   'typeNames'),
    ];

    // Flatten array scopes into comma-joined strings for TextMate compatibility
    const flatRules = textMateRules.map(r => ({
        scope: Array.isArray(r.scope) ? r.scope.join(', ') : r.scope,
        settings: r.settings,
    }));

    const editorCfg = vscode.workspace.getConfiguration('editor');
    const existing  = editorCfg.get('tokenColorCustomizations') || {};

    // Preserve any user customizations for other languages
    // We write our rules under a namespaced key inside textMateRules
    existing.textMateRules = flatRules;

    editorCfg.update(
        'tokenColorCustomizations',
        existing,
        vscode.ConfigurationTarget.Global
    );
}

/**
 * Remove S Lua token customizations from editor.tokenColorCustomizations
 */
function resetThemeCustomizations() {
    const editorCfg = vscode.workspace.getConfiguration('editor');
    const existing  = editorCfg.get('tokenColorCustomizations') || {};
    delete existing.textMateRules;
    editorCfg.update(
        'tokenColorCustomizations',
        Object.keys(existing).length ? existing : undefined,
        vscode.ConfigurationTarget.Global
    );
    vscode.window.showInformationMessage('S Lua: Theme customizations cleared.');
}

// ─── Theme editor webview ────────────────────────────────────────────────────

function openThemeCustomizer(context) {
    const panel = vscode.window.createWebviewPanel(
        'sluaThemeCustomizer',
        'S Lua Theme Customizer',
        vscode.ViewColumn.Beside,
        { enableScripts: true }
    );

    const cfg = vscode.workspace.getConfiguration('slua.theme');

    function getVal(key, field) {
        return cfg.get(`${key}.${field}`, THEME_DEFAULTS[key][field]);
    }

    const tokenGroups = [
        { key: 'directives',      label: '--!! Directives',        example: '--!!type:strict' },
        { key: 'controlKeywords', label: 'Control Keywords',       example: 'if then else for while return' },
        { key: 'declKeywords',    label: 'Declaration Keywords',   example: 'local const function type import' },
        { key: 'memoryKeywords',  label: 'Memory Keywords',        example: 'alloc deref store cast panic' },
        { key: 'types',           label: 'Built-in Types',         example: 'int number string bool ptr void' },
        { key: 'constants',       label: 'Constants',              example: 'null true false' },
        { key: 'modules',         label: 'Module Names',           example: 'math io os string window draw' },
        { key: 'functions',       label: 'Function Declarations',  example: 'function main() ...' },
        { key: 'functionCalls',   label: 'Function Calls',         example: 'print(x)  math.sqrt(y)' },
        { key: 'strings',         label: 'String Literals',        example: '"Hello, World!"' },
        { key: 'numbers',         label: 'Numeric Literals',       example: '42  3.14  0xFF' },
        { key: 'comments',        label: 'Comments',               example: '-- this is a comment' },
        { key: 'operators',       label: 'Operators',              example: '== ~= <= .. -> + - * /' },
        { key: 'typeNames',       label: 'User Type Names',        example: 'Vec2.length  Stack.push' },
    ];

    const rowsHtml = tokenGroups.map(g => {
        const color     = getVal(g.key, 'color');
        const fontStyle = getVal(g.key, 'fontStyle');
        return `
        <tr class="row" data-key="${g.key}">
          <td class="label">${g.label}</td>
          <td class="example" style="color:${color}; font-style:${fontStyle.includes('italic')?'italic':'normal'}; font-weight:${fontStyle.includes('bold')?'bold':'normal'}">
            ${escapeHtml(g.example)}
          </td>
          <td>
            <input type="color" class="color-picker" data-key="${g.key}" value="${color}" title="Pick color">
            <input type="text"  class="color-text"   data-key="${g.key}" value="${color}" placeholder="#rrggbb" maxlength="7">
          </td>
          <td>
            <select class="font-style-select" data-key="${g.key}">
              <option value="" ${fontStyle===''?'selected':''}>normal</option>
              <option value="bold" ${fontStyle==='bold'?'selected':''}>bold</option>
              <option value="italic" ${fontStyle==='italic'?'selected':''}>italic</option>
              <option value="bold italic" ${fontStyle==='bold italic'?'selected':''}>bold italic</option>
              <option value="underline" ${fontStyle==='underline'?'selected':''}>underline</option>
            </select>
          </td>
        </tr>`;
    }).join('\n');

panel.webview.html = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>S Lua Theme Customizer</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: 'Segoe UI', system-ui, sans-serif;
    background: #1e1e2e;
    color: #cdd6f4;
    padding: 20px;
  }
  h1 {
    color: #cba6f7;
    font-size: 18px;
    margin-bottom: 6px;
  }
  .subtitle { color: #6c7086; font-size: 12px; margin-bottom: 20px; }
  table { width: 100%; border-collapse: collapse; }
  th {
    text-align: left;
    padding: 8px 10px;
    font-size: 11px;
    color: #6c7086;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    border-bottom: 1px solid #313244;
  }
  .row { border-bottom: 1px solid #181825; transition: background 0.1s; }
  .row:hover { background: #24273a; }
  td { padding: 8px 10px; vertical-align: middle; }
  .label { color: #bac2de; font-weight: 500; width: 180px; }
  .example {
    font-family: 'Cascadia Code', 'Fira Code', monospace;
    font-size: 12px;
    width: 240px;
    white-space: nowrap;
  }
  input[type="color"] {
    width: 32px; height: 28px;
    border: 1px solid #45475a;
    border-radius: 4px;
    cursor: pointer;
    background: none;
    padding: 2px;
    vertical-align: middle;
  }
  input[type="text"].color-text {
    width: 80px;
    background: #313244;
    border: 1px solid #45475a;
    border-radius: 4px;
    color: #cdd6f4;
    padding: 4px 8px;
    font-family: monospace;
    font-size: 12px;
    margin-left: 6px;
    vertical-align: middle;
  }
  input[type="text"].color-text:focus {
    outline: none;
    border-color: #cba6f7;
  }
  select.font-style-select {
    background: #313244;
    border: 1px solid #45475a;
    border-radius: 4px;
    color: #cdd6f4;
    padding: 4px 8px;
    font-size: 12px;
    cursor: pointer;
  }
  select.font-style-select:focus { outline: none; border-color: #cba6f7; }
  .toolbar {
    display: flex;
    gap: 10px;
    margin-top: 24px;
    padding-top: 16px;
    border-top: 1px solid #313244;
  }
  button {
    padding: 8px 20px;
    border: none;
    border-radius: 6px;
    cursor: pointer;
    font-size: 13px;
    font-weight: 500;
  }
  .btn-apply  { background: #cba6f7; color: #1e1e2e; }
  .btn-reset  { background: #45475a; color: #cdd6f4; }
  .btn-apply:hover { background: #d4b8f8; }
  .btn-reset:hover { background: #585b70; }
  .status { color: #a6e3a1; font-size: 12px; margin-left: auto; align-self: center; display: none; }
</style>
</head>
<body>
<h1>🎨 S Lua Theme Customizer</h1>
<p class="subtitle">Changes apply instantly to all open .slua files. Settings are saved to your VS Code global config.</p>

<table>
  <thead>
    <tr>
      <th>Token Group</th>
      <th>Preview</th>
      <th>Color</th>
      <th>Font Style</th>
    </tr>
  </thead>
  <tbody>${rowsHtml}</tbody>
</table>

<div class="toolbar">
  <button class="btn-apply" onclick="applyAll()">✓ Apply All</button>
  <button class="btn-reset" onclick="resetAll()">↺ Reset Defaults</button>
  <span class="status" id="status">Applied!</span>
</div>

<script>
  const vscode = acquireVsCodeApi();

  // Live preview: update example cell when color/style changes
  document.querySelectorAll('.color-picker').forEach(picker => {
    picker.addEventListener('input', e => {
      const key = e.target.dataset.key;
      const textInput = document.querySelector('.color-text[data-key="' + key + '"]');
      textInput.value = e.target.value;
      updatePreview(key);
    });
  });

  document.querySelectorAll('.color-text').forEach(txt => {
    txt.addEventListener('input', e => {
      const key   = e.target.dataset.key;
      const val   = e.target.value;
      if (/^#[0-9a-fA-F]{6}$/.test(val)) {
        const picker = document.querySelector('.color-picker[data-key="' + key + '"]');
        picker.value = val;
        updatePreview(key);
      }
    });
  });

  document.querySelectorAll('.font-style-select').forEach(sel => {
    sel.addEventListener('change', e => {
      updatePreview(e.target.dataset.key);
    });
  });

  function updatePreview(key) {
    const color = document.querySelector('.color-picker[data-key="' + key + '"]').value;
    const fs    = document.querySelector('.font-style-select[data-key="' + key + '"]').value;
    const cell  = document.querySelector('.row[data-key="' + key + '"] .example');
    cell.style.color      = color;
    cell.style.fontStyle  = fs.includes('italic') ? 'italic' : 'normal';
    cell.style.fontWeight = fs.includes('bold')   ? 'bold'   : 'normal';
  }

  function applyAll() {
    const changes = {};
    document.querySelectorAll('.row').forEach(row => {
      const key   = row.dataset.key;
      const color = row.querySelector('.color-picker').value;
      const fs    = row.querySelector('.font-style-select').value;
      changes[key] = { color, fontStyle: fs };
    });
    vscode.postMessage({ command: 'applyTheme', changes });
    const status = document.getElementById('status');
    status.style.display = 'inline';
    setTimeout(() => { status.style.display = 'none'; }, 2000);
  }

  function resetAll() {
    vscode.postMessage({ command: 'resetTheme' });
  }
</script>
</body>
</html>`;

    panel.webview.onDidReceiveMessage(msg => {
        if (msg.command === 'applyTheme') {
            const slua = vscode.workspace.getConfiguration('slua.theme');
            for (const [key, val] of Object.entries(msg.changes)) {
                slua.update(`${key}.color`,     val.color,     vscode.ConfigurationTarget.Global);
                slua.update(`${key}.fontStyle`, val.fontStyle, vscode.ConfigurationTarget.Global);
            }
            // applyThemeCustomizations() will be called by onDidChangeConfiguration
        }
        if (msg.command === 'resetTheme') {
            cmdResetTheme();
        }
    }, undefined, context.subscriptions);
}

function escapeHtml(s) {
    return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

// ─── Diagnostics ─────────────────────────────────────────────────────────────

function parseCompilerOutput(stderr, document) {
    const diags = [];
    for (const raw of stderr.split('\n')) {
        const line = raw.trim();
        if (!line.startsWith('[')) continue;

        let m = line.match(/^\[([EWN])\w+\]\s+\S+?:(\d+):(\d+)\s+(.+)$/);
        if (m) {
            const sev = m[1]==='E' ? vscode.DiagnosticSeverity.Error
                      : m[1]==='W' ? vscode.DiagnosticSeverity.Warning
                      :              vscode.DiagnosticSeverity.Information;
            const ln  = Math.max(0, parseInt(m[2]) - 1);
            const col = Math.max(0, parseInt(m[3]) - 1);
            const lineText = ln < document.lineCount ? document.lineAt(ln).text : '';
            diags.push(new vscode.Diagnostic(
                new vscode.Range(ln, col, ln, Math.max(col+1, lineText.length)),
                m[4].trim(), sev
            ));
            continue;
        }

        m = line.match(/^\[([EWN])\w+\]\s+\S+?:\s+(.+)$/);
        if (m) {
            const sev = m[1]==='E' ? vscode.DiagnosticSeverity.Error
                      : m[1]==='W' ? vscode.DiagnosticSeverity.Warning
                      :              vscode.DiagnosticSeverity.Information;
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
    proc.stderr.on('data', d => { stderr += d.toString(); });
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
    proc.stderr.on('data', d => { stderr += d.toString(); });
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

// ─── Completion ───────────────────────────────────────────────────────────────

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
            it.detail = `module — import ${needs}`;
            items.push(it);
        }
        const pi = new vscode.CompletionItem('print', vscode.CompletionItemKind.Function);
        pi.insertText    = new vscode.SnippetString('print(${1:value})');
        pi.detail        = 'print(value: any): void';
        pi.documentation = new vscode.MarkdownString('Print value + newline.');
        items.push(pi);

        const text = document.getText();
        const seen = new Set(items.map(i => String(i.label)));
        const addSymbols = (re, kind) => {
            let m;
            while ((m = re.exec(text)) !== null)
                if (!seen.has(m[1])) { items.push(new vscode.CompletionItem(m[1], kind)); seen.add(m[1]); }
        };
        addSymbols(/\blocal\s+([a-zA-Z_]\w*)/g,   vscode.CompletionItemKind.Variable);
        addSymbols(/\bconst\s+([a-zA-Z_]\w*)/g,   vscode.CompletionItemKind.Constant);
        addSymbols(/\bglobal\s+([a-zA-Z_]\w*)/g,  vscode.CompletionItemKind.Variable);
        addSymbols(/\bfunction\s+([a-zA-Z_]\w*(?:\.[a-zA-Z_]\w*)?)/g, vscode.CompletionItemKind.Function);
        addSymbols(/\btype\s+([a-zA-Z_]\w*)/g,    vscode.CompletionItemKind.Class);
        addSymbols(/\benum\s+([a-zA-Z_]\w*)/g,    vscode.CompletionItemKind.Enum);
        return items;
    }
}

// ─── Hover ────────────────────────────────────────────────────────────────────

class SluaHoverProvider {
    provideHover(document, position) {
        const wordRange = document.getWordRangeAtPosition(position, /[a-zA-Z_]\w*/);
        if (!wordRange) return null;
        const word     = document.getText(wordRange);
        const lineText = document.lineAt(position).text;

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

        if (MODULE_METHODS[word]) {
            const lines = MODULE_METHODS[word].map(m => `- \`${m.name}\` — ${m.doc}`).join('\n');
            return new vscode.Hover(new vscode.MarkdownString(`**module \`${word}\`**\n\n${lines}`), wordRange);
        }

        const typeInfo = {
            int:'64-bit signed int. Aliases: int8 int16 int32 int64.',
            number:'64-bit double. Aliases: float double.',
            string:'Immutable string (char*).',
            bool:'Boolean.', void:'No value.', any:'Dynamic value.',
            ptr:'Pointer: ptr<T>', table:'Hash table.',
            uint8:'8-bit unsigned', uint16:'16-bit unsigned',
            uint32:'32-bit unsigned', uint64:'64-bit unsigned',
            int8:'8-bit signed', int16:'16-bit signed',
            int32:'32-bit signed', int64:'64-bit signed (= int)',
            char:'char (int8)', byte:'byte (uint8)',
        };
        if (typeInfo[word])
            return new vscode.Hover(new vscode.MarkdownString(`**type** \`${word}\` — ${typeInfo[word]}`), wordRange);

        const kwInfo = {
            local:'`local name: Type = value` — mutable local.',
            const:'`const NAME: Type = value` — immutable constant.',
            global:'`global name: Type = value` — module global.',
            function:'`function name(params): RetType ... end`',
            import:'`import module` — Available: math os io string stdata stdgui',
            type:'`type Name = { field: Type }` — record type.',
            enum:'`enum Name = { A=0, B, C }` — enumeration.',
            defer:'`defer free(ptr)` — runs when scope exits.',
            alloc_typed:'`alloc_typed(Type, count): ptr<Type>`',
            deref:'`deref(ptr): T`',
            store:'`store(ptr, value)`',
            cast:'`cast(TargetType, expr)`',
            panic:'`panic("message")` — abort.',
            null:'Null pointer literal.', true:'true', false:'false',
        };
        if (kwInfo[word])
            return new vscode.Hover(new vscode.MarkdownString(`**S Lua** — ${kwInfo[word]}`), wordRange);

        return null;
    }
}

// ─── Commands ─────────────────────────────────────────────────────────────────

function cmdRunFile() {
    const editor = vscode.window.activeTextEditor;
    if (!editor || !isSluaDocument(editor.document)) {
        vscode.window.showErrorMessage('S Lua: No .slua file is active.');
        return;
    }
    editor.document.save().then(() => {
        const root = getProjectRoot();
        const ps1  = path.join(root, 'slua.ps1');
        const rel  = path.relative(root, editor.document.uri.fsPath).replace(/\\/g, '/');
        let t = vscode.window.terminals.find(x => x.name === 'S Lua');
        if (!t) t = vscode.window.createTerminal({ name:'S Lua', cwd:root, shellPath:'powershell.exe', shellArgs:['-ExecutionPolicy','Bypass'] });
        t.show(true);
        t.sendText(`& "${ps1}" Slua-Run "${rel}"`, true);
    });
}

function cmdBuildCompiler() {
    const root = getProjectRoot();
    const bat  = path.join(root, 'cmake_configure.bat');
    let t = vscode.window.terminals.find(x => x.name === 'S Lua Build');
    if (!t) t = vscode.window.createTerminal({ name:'S Lua Build', cwd:root, shellPath:'cmd.exe' });
    t.show(true);
    t.sendText(`"${bat}"`, true);
}

function cmdRunDiagnostics() {
    const doc = vscode.window.activeTextEditor?.document;
    if (!doc || !isSluaDocument(doc)) { vscode.window.showErrorMessage('S Lua: No .slua file.'); return; }
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
        if (!t) t = vscode.window.createTerminal({ name:'S Lua AST', cwd:root });
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
        if (!t) t = vscode.window.createTerminal({ name:'S Lua IR', cwd:root });
        t.show(true);
        t.sendText(`& "${compiler}" "${doc.uri.fsPath}" -o "${outLL}"`, true);
    });
}

function cmdResetTheme() {
    // Reset all slua.theme.* settings to defaults
    const cfg = vscode.workspace.getConfiguration('slua.theme');
    for (const [key, defaults] of Object.entries(THEME_DEFAULTS)) {
        cfg.update(`${key}.color`,     defaults.color,     vscode.ConfigurationTarget.Global);
        cfg.update(`${key}.fontStyle`, defaults.fontStyle, vscode.ConfigurationTarget.Global);
    }
    resetThemeCustomizations();
    vscode.window.showInformationMessage('S Lua: Theme reset to defaults.');
}

// ─── Activation ───────────────────────────────────────────────────────────────

function activate(context) {

    diagnosticCollection = vscode.languages.createDiagnosticCollection('slua');
    context.subscriptions.push(diagnosticCollection);

    // Apply theme customizations on startup
    applyThemeCustomizations();

    // Re-apply when slua.theme.* settings change
    context.subscriptions.push(
        vscode.workspace.onDidChangeConfiguration(e => {
            if (e.affectsConfiguration('slua.theme')) {
                applyThemeCustomizations();
            }
        })
    );

    // Lint events
    context.subscriptions.push(
        vscode.workspace.onDidOpenTextDocument(doc => {
            if (isSluaDocument(doc)) runLintOnFile(doc);
        })
    );
    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument(doc => {
            const cfg = vscode.workspace.getConfiguration('slua');
            if (isSluaDocument(doc) && cfg.get('lintOnSave', true)) runLintOnFile(doc);
        })
    );
    context.subscriptions.push(
        vscode.workspace.onDidChangeTextDocument(ev => {
            if (!isSluaDocument(ev.document)) return;
            const cfg   = vscode.workspace.getConfiguration('slua');
            if (!cfg.get('lintOnChange', true)) return;
            const delay = cfg.get('lintDelay', 700);
            if (lintTimer) clearTimeout(lintTimer);
            lintTimer = setTimeout(() => runLintOnContent(ev.document), delay);
        })
    );
    context.subscriptions.push(
        vscode.workspace.onDidCloseTextDocument(doc => diagnosticCollection.delete(doc.uri))
    );

    // Language features (work even when file shows as 'lua' before extension installs)
    const selector = [{ language:'slua' }, { scheme:'file', pattern:'**/*.slua' }];
    context.subscriptions.push(
        vscode.languages.registerCompletionItemProvider(selector, new SluaCompletionProvider(), '.', ':')
    );
    context.subscriptions.push(
        vscode.languages.registerHoverProvider(selector, new SluaHoverProvider())
    );

    // Commands
    context.subscriptions.push(vscode.commands.registerCommand('slua.runFile',        cmdRunFile));
    context.subscriptions.push(vscode.commands.registerCommand('slua.buildCompiler',  cmdBuildCompiler));
    context.subscriptions.push(vscode.commands.registerCommand('slua.compile',        cmdRunDiagnostics));
    context.subscriptions.push(vscode.commands.registerCommand('slua.emitAST',        cmdEmitAST));
    context.subscriptions.push(vscode.commands.registerCommand('slua.emitIR',         cmdEmitIR));
    context.subscriptions.push(vscode.commands.registerCommand('slua.customizeTheme', () => openThemeCustomizer(context)));
    context.subscriptions.push(vscode.commands.registerCommand('slua.resetTheme',     cmdResetTheme));

    // Lint already-open files
    vscode.workspace.textDocuments.forEach(doc => {
        if (isSluaDocument(doc)) runLintOnFile(doc);
    });
}

function deactivate() {
    if (lintTimer) clearTimeout(lintTimer);
    if (diagnosticCollection) diagnosticCollection.dispose();
}

module.exports = { activate, deactivate };