// QML Style Audit — extract all visual parameters and detect inconsistencies

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const QML_DIR = 'D:\\latest-code\\cpp\\qml';
const files = [];

// Walk directory
function walk(dir) {
    for (const f of fs.readdirSync(dir, { withFileTypes: true })) {
        const p = path.join(dir, f.name);
        if (f.isDirectory()) walk(p);
        else if (f.name.endsWith('.qml')) files.push(p);
    }
}
walk(QML_DIR);

console.log(`Scanning ${files.length} QML files...\n`);

// ── Data collectors ──
const colors = {};            // hex -> { count, sources: [file:line] }
const fontSizes = {};         // px -> { count, sources }
const fontFamilies = {};      // name -> { count, sources }
const radii = {};             // px -> { count, sources }
const spacings = {};          // px -> { count, sources }
const margins = {};           // px -> { count, sources }
const shadows = {};           // blur|dx|dy|color -> { count, sources }
const borders = {};           // width|color -> { count, sources }
const opacity = {};           // value -> { count, sources }
const animations = {
    duration: {},             // ms -> { count, sources }
    easing: {},               // type -> { count, sources }
};

// Helper
function record(map, key, file, line) {
    const shortFile = file.replace(QML_DIR + '\\', '');
    if (!map[key]) map[key] = { count: 0, sources: [] };
    map[key].count++;
    if (map[key].sources.length < 5) map[key].sources.push(`${shortFile}:${line}`);
}

// ── Regex patterns ──
// Remove comments first
function stripComments(text) {
    return text
        .replace(/\/\/.*$/gm, '')          // single-line
        .replace(/\/\*[\s\S]*?\*\//g, ''); // multi-line
}

for (const file of files) {
    let text;
    try { text = fs.readFileSync(file, 'utf8'); } catch { continue; }
    const lines = text.split('\n');
    const clean = stripComments(text);
    
    for (let i = 0; i < lines.length; i++) {
        const l = lines[i];
        const lineNum = i + 1;

        // Colors: #hex, Qt.rgba, Qt.hsla
        const hexMatches = l.match(/#([0-9a-fA-F]{3,8})/g);
        if (hexMatches) for (const h of hexMatches) record(colors, h.toLowerCase(), file, lineNum);

        // Named colors in quotes (e.g., color: "#333")
        const namedColors = l.match(/["']#([0-9a-fA-F]{3,8})["']/g);
        if (namedColors) for (const h of namedColors) {
            record(colors, h.replace(/["']/g, '').toLowerCase(), file, lineNum);
        }

        // font.pixelSize
        const fps = l.match(/font\.pixelSize\s*:\s*(\d+)/);
        if (fps) record(fontSizes, parseInt(fps[1]), file, lineNum);

        const ff = l.match(/font\.family\s*:\s*["']([^"']+)["']/);
        if (ff) record(fontFamilies, ff[1], file, lineNum);

        // radius
        const rad = l.match(/(?:^|\s)radius\s*:\s*(\d+)/);
        if (rad) record(radii, parseInt(rad[1]), file, lineNum);

        // spacing
        const sp = l.match(/(?:^|\s)spacing\s*:\s*(\d+)/);
        if (sp) record(spacings, parseInt(sp[1]), file, lineNum);

        // margins: leftMargin, rightMargin, topMargin, bottomMargin, margins
        const mg = l.match(/(?:leftMargin|rightMargin|topMargin|bottomMargin|margins)\s*:\s*(\d+)/i);
        if (mg) record(margins, parseInt(mg[1]), file, lineNum);

        // Shadow (parameters in a component's Shadow block or properties)
        const shBlur = l.match(/(?:^|\s)blur\s*:\s*(\d+\.?\d*)/);
        if (shBlur) record(shadows, `blur=${shBlur[1]}`, file, lineNum);

        // Border: border.width, border.color
        const bw = l.match(/border\.width\s*:\s*(\d+\.?\d*)/);
        if (bw) record(borders, `width=${bw[1]}`, file, lineNum);
        const bc = l.match(/border\.color\s*:\s*["']?(#[0-9a-fA-F]+)["']?/i);
        if (bc) record(borders, `color=${bc[1].toLowerCase()}`, file, lineNum);

        // opacity
        const op = l.match(/(?:^|\s)opacity\s*:\s*(0\.\d+|1\.0|1)\b/);
        if (op) record(opacity, parseFloat(op[1]), file, lineNum);

        // Animation duration
        const dur = l.match(/(?:^|\s)duration\s*:\s*(\d+)/);
        if (dur) record(animations.duration, parseInt(dur[1]), file, lineNum);

        // Easing types
        const easing = l.match(/easing\.type\s*\.\s*Easing\.(\w+)/);
        if (easing) record(animations.easing, easing[1], file, lineNum);
    }
}

// ── Report ──
function printSection(title, data, numericSort = false) {
    console.log(`\n${'='.repeat(60)}`);
    console.log(`  ${title}`);
    console.log(`${'='.repeat(60)}`);
    
    const entries = Object.entries(data);
    if (entries.length === 0) { console.log('  (none found)'); return; }
    
    const sorted = numericSort 
        ? entries.sort((a, b) => {
              const na = isNaN(Number(a[0])) ? 0 : Number(a[0]);
              const nb = isNaN(Number(b[0])) ? 0 : Number(b[0]);
              return na - nb;
          })
        : entries.sort((a, b) => b[1].count - a[1].count);
    
    const wideKeys = new Set();
    for (const [k, v] of sorted) {
        if (k.toString().length > 20) wideKeys.add(true);
    }
    
    const fmtKey = k => {
        if (k === 8) return '  8';
        return String(k).padStart(8);
    };
    
    for (const [k, v] of sorted.slice(0, 30)) {
        if (numericSort) {
            const padK = String(k).padStart(6);
            console.log(`  ${padK}  ×${String(v.count).padStart(4)}  — ${v.sources.slice(0, 3).join(', ')}`);
        } else {
            const padK = String(k).padEnd(16);
            console.log(`  ${padK}  ×${String(v.count).padStart(4)}`);
        }
    }
    if (sorted.length > 30) console.log(`  ... and ${sorted.length - 30} more`);
}

// Print report
printSection('[画面] 颜色值 (Color Hex)', colors);
printSection('📐 font.pixelSize', fontSizes, true);
printSection('[字号] font.family', fontFamilies);
printSection('[圆角] radius', radii, true);
printSection('↔️ spacing', spacings, true);
printSection('[下载] margins', margins, true);
printSection('🌓 opacity', opacity, true);
printSection('😶‍🌫️ animation duration (ms)', animations.duration, true);
printSection('🏃 easing types', animations.easing);
printSection('[用户] shadow/blur params', shadows);
printSection('[纹理] border params', borders);

// ── Find near-duplicate colors ──
console.log(`\n${'='.repeat(60)}`);
console.log('  [搜索] 疑似重复/相近颜色');
console.log(`${'='.repeat(60)}`);

const hexKeys = Object.keys(colors);
const similarGroups = [];
const checked = new Set();

// Parse hex to RGB
function hexToRGB(h) {
    h = h.replace('#', '');
    if (h.length === 3) h = h.split('').map(c => c + c).join('');
    const r = parseInt(h.slice(0, 2), 16);
    const g = parseInt(h.slice(2, 4), 16);
    const b = parseInt(h.slice(4, 6), 16);
    return [r, g, b];
}

function colorDistance(c1, c2) {
    const [r1, g1, b1] = hexToRGB(c1);
    const [r2, g2, b2] = hexToRGB(c2);
    return Math.sqrt((r1-r2)**2 + (g1-g2)**2 + (b1-b2)**2);
}

for (let i = 0; i < hexKeys.length; i++) {
    if (checked.has(hexKeys[i])) continue;
    const group = [hexKeys[i]];
    checked.add(hexKeys[i]);
    for (let j = i + 1; j < hexKeys.length; j++) {
        if (checked.has(hexKeys[j])) continue;
        const dist = colorDistance(hexKeys[i], hexKeys[j]);
        // Only compare if they're close
        if (dist < 30) {
            // Check if there are different hex strings for effectively same color
            const c1 = hexToRGB(hexKeys[i]);
            const c2 = hexToRGB(hexKeys[j]);
            if (c1[0] !== c2[0] || c1[1] !== c2[1] || c1[2] !== c2[2]) {
                group.push(hexKeys[j]);
                checked.add(hexKeys[j]);
            }
        }
    }
    if (group.length > 1) similarGroups.push(group);
}

if (similarGroups.length === 0) {
    console.log('  (没有明显相近的颜色组)');
} else {
    for (const g of similarGroups) {
        const total = g.reduce((s, c) => s + colors[c].count, 0);
        console.log(`  相近组 (总×${total}):`);
        for (const c of g) {
            const [r, g0, b] = hexToRGB(c);
            console.log(`    ${c.padEnd(10)} rgb(${r},${g0},${b})  ×${colors[c].count}  ${colors[c].sources.slice(0,3).join(', ')}`);
        }
        console.log();
    }
}

// ── Per-file style analysis ──
console.log(`${'='.repeat(60)}`);
console.log('  [统计] 每文件独有/罕见的样式值');
console.log(`${'='.repeat(60)}`);

// For each file, check if it uses colors that no other file uses
const fileColors = {};
const fileFontSizes = {};
const fileRadii = {};

for (const file of files) {
    const sf = file.replace(QML_DIR + '\\', '');
    fileColors[sf] = new Set();
    fileFontSizes[sf] = new Set();
    fileRadii[sf] = new Set();
    
    const text = fs.readFileSync(file, 'utf8');
    const lines = text.split('\n');
    for (let i = 0; i < lines.length; i++) {
        const l = lines[i];
        const ln = i + 1;
        
        const hex = l.match(/#([0-9a-fA-F]{3,8})\b/);
        if (hex && !l.includes('//') && !l.includes('/*')) {
            fileColors[sf].add(hex[0].toLowerCase());
        }
        
        const fps = l.match(/font\.pixelSize\s*:\s*(\d+)/);
        if (fps) fileFontSizes[sf].add(parseInt(fps[1]));
        
        const rad = l.match(/(?:^|\s)radius\s*:\s*(\d+)/);
        if (rad) fileRadii[sf].add(parseInt(rad[1]));
    }
}

// Find unique colors per file
const allColorUsage = {};
for (const [f, cset] of Object.entries(fileColors)) {
    for (const c of cset) {
        if (!allColorUsage[c]) allColorUsage[c] = [];
        allColorUsage[c].push(f);
    }
}

const rareColors = {};
for (const [c, filesUsing] of Object.entries(allColorUsage)) {
    if (filesUsing.length === 1) {
        rareColors[c] = filesUsing[0];
    }
}

if (Object.keys(rareColors).length > 0) {
    const byFile = {};
    for (const [c, f] of Object.entries(rareColors)) {
        if (!byFile[f]) byFile[f] = [];
        byFile[f].push(c);
    }
    for (const [f, cols] of Object.entries(byFile).sort((a, b) => b[1].length - a[1].length)) {
        console.log(`  📄 ${f}: ${cols.length} 种独有颜色`);
        for (const c of cols.slice(0, 8)) {
            const src = colors[c]?.sources?.slice(0, 2).join(', ') || '';
            console.log(`    ${c}  ${src}`);
        }
        if (cols.length > 8) console.log(`    ... 还有 ${cols.length - 8} 种`);
        console.log();
    }
} else {
    console.log('  (所有颜色都是共享的)');
}

// ── Component usage ──
console.log(`${'='.repeat(60)}`);
console.log('  [组件] 自定义组件使用统计');
console.log(`${'='.repeat(60)}`);

const componentUsage = {};
const customComponents = [
    'ShadowButton', 'ToastManager', 'InlineToast', 'KillButton',
    'ConfirmDialog', 'DetailInfoCard', 'DetailVersionCard',
    'ModLoaderCard', 'CapeCard', 'ExpandableGroupCard',
    'MinecraftHead2D', 'MinecraftHead3D', 'SkinPreview3D',
    'ShaderDropdown', 'BackgroundCropOverlay'
];

for (const file of files) {
    const text = fs.readFileSync(file, 'utf8');
    const sf = file.replace(QML_DIR + '\\', '');
    for (const comp of customComponents) {
        const rx = new RegExp(`\\b${comp}\\b`, 'g');
        const matches = text.match(rx);
        if (matches) {
            if (!componentUsage[comp]) componentUsage[comp] = { total: 0, files: {} };
            componentUsage[comp].total += matches.length;
            componentUsage[comp].files[sf] = matches.length;
        }
    }
}

for (const [comp, data] of Object.entries(componentUsage).sort((a, b) => b[1].total - a[1].total)) {
    const users = Object.keys(data.files).length;
    console.log(`  ${comp.padEnd(22)} ×${String(data.total).padEnd(4)}  (${users} 文件)`);
}

console.log(`\n${'='.repeat(60)}`);
console.log('  [完成] 审计完成');
console.log(`${'='.repeat(60)}`);
