// refactor-style.js — 根据 StyleTokens 自动替换 QML 中的硬编码样式值
//
// 用法: node refactor-style.js [--apply] [--dry-run] [--verbose]
//   --dry-run   只显示要改的内容（默认）
//   --apply     实际修改文件

const fs = require('fs');
const path = require('path');

const QML_DIR = 'D:\\latest-code\\cpp\\qml';
const APPLY = process.argv.includes('--apply');
const DRY_RUN = !APPLY;
const VERBOSE = process.argv.includes('--verbose');

// ── StyleTokens 定义（与 StyleTokens.qml 同步）──
const TOKENS = {
    // 背景
    'bgPrimary':    '#0c0f16',
    'bgSecondary':  '#11141c',
    'bgCard':       '#1a1f2e',
    'bgElevated':   '#1e2230',
    'bgHover':      '#2a3040',
    'bgInput':      '#1a1f2a',
    'surfaceOverlay':'#141820',
    'surfaceLight': '#1a1d24',

    // 文字
    'textPrimary':  '#e8ecf8',
    'textSecondary':'#e4e8f2',
    'textTertiary': '#9094a8',
    'textMuted':    '#505468',
    'textInverse':  '#ffffff',

    // 主色调
    'accent':       '#3b82f6',
    'accentHover':  '#5068c8',
    'accentLight':  '#6080e8',
    'accentVivid':  '#4a8fe7',
    'accentLink':   '#8aaeff',
    'accentSubtle': '#1a2848',

    // 状态
    'success':      '#4bc870',
    'successBg':    '#1a3a1a',
    'warning':      '#ff9800',
    'warningBg':    '#3a3000',
    'error':        '#ef4444',
    'errorBg':      '#3a1a1a',
    'errorLight':   '#e06060',
    'info':         '#5098e8',
    'infoBg':       '#1a3a5c',

    // 边框
    'border':       '#1e2230',
    'borderLight':  '#2a3040',
    'borderFocus':  '#5068c8',
};

// Radius tokens: value -> tokenName
const RADIUS = {
    // 0 is intentionally omitted — keep as literal 0 for flat corners
    2: 'radiusXs',
    4: 'radiusSm',
    6: 'radiusMd',
    8: 'radiusLg',
    12: 'radiusXl',
    16: 'radiusFull',
};

// Spacing tokens: value -> tokenName  
const SPACING = {
    4: 'spacingXs',
    8: 'spacingSm',
    12: 'spacingMd',
    16: 'spacingLg',
    24: 'spacingXl',
    32: 'spacing2xl',
    40: 'spacing3xl',
};

// Font size tokens: value -> tokenName
const FONT_SIZE = {
    10: 'fontSizeXs',
    11: 'fontSizeSm',
    13: 'fontSizeMd',
    15: 'fontSizeLg',
    18: 'fontSizeXl',
    24: 'fontSize2xl',
};

// ── Helpers ──
function hexToRgb(h) {
    h = h.replace('#', '');
    if (h.length === 3) h = h.split('').map(c => c + c).join('');
    if (h.length === 8) h = h.slice(2); // skip alpha
    return [parseInt(h.slice(0,2),16), parseInt(h.slice(2,4),16), parseInt(h.slice(4,6),16)];
}

function colorDist(h1, h2) {
    const [r1,g1,b1] = hexToRgb(h1);
    const [r2,g2,b2] = hexToRgb(h2);
    return Math.sqrt((r1-r2)**2 + (g1-g2)**2 + (b1-b2)**2);
}

function closestColor(hex) {
    if (!hex || hex.length < 4) return null;
    hex = hex.toLowerCase();
    // Normalize 3-digit
    if (hex.length === 4) {
        hex = '#' + hex[1]+hex[1] + hex[2]+hex[2] + hex[3]+hex[3];
    }
    if (hex.includes('gradient') || hex.includes('transparent')) return null;

    let best = null, bestDist = 999;
    for (const [name, tokenHex] of Object.entries(TOKENS)) {
        const d = colorDist(hex, tokenHex);
        if (d < bestDist) { bestDist = d; best = name; }
    }
    // Threshold: only match if within reasonable distance
    if (bestDist > 25) return null;
    return best;
}

function closestRadius(val) {
    const v = parseInt(val);
    if (isNaN(v)) return null;
    // Find exact or nearest
    const keys = Object.keys(RADIUS).map(Number).sort((a,b)=>a-b);
    let best = null, bestDiff = 999;
    for (const k of keys) {
        const diff = Math.abs(v - k);
        if (diff < bestDiff) { bestDiff = diff; best = k; }
    }
    if (bestDiff > 2) return null; // too far
    return RADIUS[best] ? `radius: StyleTokens.${RADIUS[best]}` : null;
}

function closestSpacing(val) {
    const v = parseInt(val);
    if (isNaN(v)) return null;
    const keys = Object.keys(SPACING).map(Number).sort((a,b)=>a-b);
    let best = null, bestDiff = 999;
    for (const k of keys) {
        const diff = Math.abs(v - k);
        if (diff < bestDiff) { bestDiff = diff; best = k; }
    }
    if (bestDiff > 2) return null;
    return SPACING[best] ? `StyleTokens.${SPACING[best]}` : null;
}

function closestFontSize(val) {
    const v = parseInt(val);
    if (isNaN(v)) return null;
    const keys = Object.keys(FONT_SIZE).map(Number).sort((a,b)=>a-b);
    let best = null, bestDiff = 999;
    for (const k of keys) {
        const diff = Math.abs(v - k);
        if (diff < bestDiff) { bestDiff = diff; best = k; }
    }
    if (bestDiff > 2) return null;
    return FONT_SIZE[best] ? `StyleTokens.${FONT_SIZE[best]}` : null;
}

// ── Scan all QML files ──
const files = [];
function walk(dir) {
    for (const f of fs.readdirSync(dir, { withFileTypes: true })) {
        const p = path.join(dir, f.name);
        if (f.isDirectory()) walk(p);
        else if (f.name.endsWith('.qml') && f.name !== 'StyleTokens.qml') files.push(p);
    }
}
walk(QML_DIR);

let totalReplacements = 0;
let totalFiles = 0;

for (const file of files) {
    let text;
    try { text = fs.readFileSync(file, 'utf8'); } catch { continue; }
    const lines = text.split('\n');
    const replacements = [];
    const shortFile = file.replace(QML_DIR + '\\', '');

    // Skip if already imports StyleTokens
    const hasImport = text.includes('StyleTokens');

    // Regex patterns for properties we want to replace
    // Pattern: propertyName: someColorValue
    // Match colors in various contexts

    // 1. Replace colors in `color: #hex` or `color: "#hex"` patterns
    // Match property assignments with hex colors
    const colorRe = /(color\s*:\s*)['"]?(#[0-9a-fA-F]{3,8})['"]?(\s*$|[,\s\/\/])/gm;
    let m;
    while ((m = colorRe.exec(text)) !== null) {
        const full = m[0];
        const prefix = m[1];
        const hex = m[2];
        const suffix = m[3];
        const token = closestColor(hex);
        if (token) {
            // Check if this is on a line that already uses StyleTokens
            const lineStart = text.lastIndexOf('\n', m.index) + 1;
            const line = text.substring(lineStart, text.indexOf('\n', lineStart) > 0 ? text.indexOf('\n', lineStart) : text.length);
            if (line.includes('StyleTokens')) continue; // already tokenized
            replacements.push({
                from: full,
                to: `${prefix}StyleTokens.${token}${suffix}`,
                desc: `color ${hex} → StyleTokens.${token}`,
                line: line.replace(/^\s+/, ''),
            });
            totalReplacements++;
        }
    }

    // 2. Replace radius
    const radiusRe = /(?:^|\s)(radius\s*:\s*)(\d+)/gm;
    while ((m = radiusRe.exec(text)) !== null) {
        const full = m[0];
        const prefix = m[1].trim();
        const val = parseInt(m[2]);
        const lineStart = text.lastIndexOf('\n', m.index) + 1;
        const line = text.substring(lineStart, text.indexOf('\n', lineStart) > 0 ? text.indexOf('\n', lineStart) : text.length);
        if (line.includes('StyleTokens')) continue;
        const rep = closestRadius(val);
        if (rep) {
            replacements.push({
                from: full.trim(),
                to: `${prefix} ${rep}`,
                desc: `radius ${val} → ${rep}`,
                line: line.replace(/^\s+/, ''),
            });
            totalReplacements++;
        }
    }

    // 3. Replace font.pixelSize
    const fpsRe = /(font\.pixelSize\s*:\s*)(\d+)/g;
    while ((m = fpsRe.exec(text)) !== null) {
        const full = m[0];
        const prefix = m[1];
        const val = parseInt(m[2]);
        const lineStart = text.lastIndexOf('\n', m.index) + 1;
        const line = text.substring(lineStart, text.indexOf('\n', lineStart) > 0 ? text.indexOf('\n', lineStart) : text.length);
        if (line.includes('StyleTokens')) continue;
        const rep = closestFontSize(val);
        if (rep) {
            replacements.push({
                from: full.trim(),
                to: `${prefix}${rep}`,
                desc: `fontSize ${val} → ${rep}`,
                line: line.replace(/^\s+/, ''),
            });
            totalReplacements++;
        }
    }

    if (replacements.length === 0) continue;

    totalFiles++;
    console.log(`\n📄 ${shortFile} (${replacements.length}处)`);

    // Sort in reverse order so line indices don't shift
    replacements.sort((a, b) => text.indexOf(b.from) - text.indexOf(a.from));

    for (const r of replacements.slice(0, VERBOSE ? 999 : 10)) {
        const actualIdx = text.indexOf(r.from);
        if (actualIdx < 0) continue;
        console.log(`  ${r.desc}`);
        if (VERBOSE) console.log(`    was: ${r.line}`);
    }
    if (!VERBOSE && replacements.length > 10) {
        console.log(`  ... 还有 ${replacements.length - 10} 处`);
    }

    if (APPLY) {
        // Apply replacements (reverse order to preserve indices)
        for (const r of replacements) {
            // Find in current text (offsets have shifted)
            const idx = text.indexOf(r.from);
            if (idx >= 0) {
                text = text.slice(0, idx) + r.to + text.slice(idx + r.from.length);
            }
        }
        // Import not needed — singleton auto-resolves from qmldir
        fs.writeFileSync(file, text, 'utf8');
    }
}

console.log(`\n${'='.repeat(50)}`);
if (DRY_RUN) {
    console.log(`[搜索] 预览: ${totalFiles} 个文件, ${totalReplacements} 处待修改`);
    console.log('运行 node refactor-style.js --apply 来实际应用');
} else {
    console.log(`[完成] 已修改: ${totalFiles} 个文件, ${totalReplacements} 处`);
}
console.log(`${'='.repeat(50)}`);
