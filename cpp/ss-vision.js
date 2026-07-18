// ss-vision.js — Screenshot + Vision AI Pipeline
// Usage: node ss-vision.js [--prompt "分析..."] [--url http://...] [--model qwen-vl-max]
//
// Takes a screenshot from the running Shadow Launcher ScreenshotServer,
// sends it to qwen-vl-max for visual analysis, and prints the result.

const http = require('http');
const https = require('https');
const fs = require('fs');
const path = require('path');

// ── Config ──
const DASHSCOPE_KEY = 'sk-96f65b5c15324c268153c0b1d7821891';
const API_BASE = 'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions';

const SCREENSHOT_URL = process.argv.find(a => a.startsWith('--url='))?.split('=')[1] || 'http://127.0.0.1:9999/screenshot';
const MODEL = process.argv.find(a => a.startsWith('--model='))?.split('=')[1] || 'qwen-vl-max';
const SAVE = process.argv.find(a => a.startsWith('--save='))?.split('=')[1];

// Find prompt: after --prompt flag or as trailing args
let promptIdx = process.argv.indexOf('--prompt');
const PROMPT = promptIdx >= 0
    ? process.argv.slice(promptIdx + 1).filter(a => !a.startsWith('--')).join(' ')
    : '请详细分析这个用户界面的视觉一致性，包括：\n'
    + '1. 整体色彩搭配是否协调，是否有不统一的颜色\n'
    + '2. 间距和对齐是否一致\n'
    + '3. 字体大小层次是否合理\n'
    + '4. 按钮、卡片等UI组件的风格是否统一\n'
    + '5. 指出具体哪些地方不一致，并给出改进建议';

// ── Fetch screenshot ──
async function fetchScreenshot(url) {
    return new Promise((resolve, reject) => {
        http.get(url, res => {
            if (res.statusCode !== 200) {
                reject(new Error(`HTTP ${res.statusCode} fetching screenshot`));
                return;
            }
            const chunks = [];
            // Check Content-Type
            const ct = res.headers['content-type'] || '';
            if (!ct.startsWith('image/')) {
                reject(new Error(`Expected image, got ${ct}`));
                return;
            }
            res.on('data', c => chunks.push(c));
            res.on('end', () => resolve(Buffer.concat(chunks)));
        }).on('error', reject);
    });
}

// ── Call DashScope Vision API ──
async function analyzeImage(imageBuffer, prompt) {
    const base64 = imageBuffer.toString('base64');
    const body = JSON.stringify({
        model: MODEL,
        messages: [{
            role: 'user',
            content: [
                { type: 'text', text: prompt },
                { type: 'image_url', image_url: { url: `data:image/png;base64,${base64}` } }
            ]
        }],
        max_tokens: 2048
    });

    return new Promise((resolve, reject) => {
        const req = https.request(API_BASE, {
            method: 'POST',
            headers: {
                'Authorization': `Bearer ${DASHSCOPE_KEY}`,
                'Content-Type': 'application/json',
                'Content-Length': Buffer.byteLength(body)
            }
        }, res => {
            let data = '';
            res.on('data', c => data += c);
            res.on('end', () => {
                if (res.statusCode !== 200) {
                    reject(new Error(`API ${res.statusCode}: ${data}`));
                    return;
                }
                try {
                    const json = JSON.parse(data);
                    const content = json.choices?.[0]?.message?.content || '(empty response)';
                    resolve(content);
                } catch (e) {
                    reject(new Error(`Parse error: ${e.message}\n${data}`));
                }
            });
        });
        req.on('error', reject);
        req.write(body);
        req.end();
    });
}

// ── Main ──
async function main() {
    console.error(`[ss-vision] Fetching screenshot from ${SCREENSHOT_URL}...`);
    const img = await fetchScreenshot(SCREENSHOT_URL);
    console.error(`[ss-vision] Got ${(img.length / 1024).toFixed(1)} KB image`);
    
    if (SAVE) {
        fs.writeFileSync(SAVE, img);
        console.error(`[ss-vision] Saved to ${SAVE}`);
    }

    console.error(`[ss-vision] Sending to ${MODEL} for analysis...`);
    const analysis = await analyzeImage(img, PROMPT);
    
    console.log(analysis);
}

main().catch(err => {
    console.error(`[ss-vision] ERROR: ${err.message}`);
    process.exit(1);
});
