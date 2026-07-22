#!/usr/bin/env python3
"""Screenshot + Vision analysis tool for Shadow Launcher visual verification.
Usage:
    python screenshot_check.py [--save PATH] [--prompt "custom prompt"]
    python screenshot_check.py --image PATH [--prompt "custom prompt"]
"""

import json
import os
import sys
import base64
import time
import urllib.request
import urllib.error

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

def load_config():
    path = os.path.expanduser('~/.openclaw/openclaw.json')
    with open(path, 'r', encoding='utf-8') as f:
        cfg = json.load(f)
    provider = cfg.get('models', {}).get('providers', {}).get('bailian-vl', {})
    return provider.get('apiKey', ''), provider.get('baseUrl', 'https://ws-4xjtosb9pp4zoqz1.cn-beijing.maas.aliyuncs.com/compatible-mode/v1')

def take_screenshot(save_path):
    """Take a screenshot using .NET and save to path."""
    import clr
    # Use PowerShell-style approach via subprocess
    import subprocess
    ps_script = f'''
    Add-Type -AssemblyName System.Windows.Forms; 
    Add-Type -AssemblyName System.Drawing; 
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds; 
    $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height; 
    $g = [System.Drawing.Graphics]::FromImage($bmp); 
    $g.CopyFromScreen($bounds.X, $bounds.Y, 0, 0, $bounds.Size); 
    $g.Dispose(); 
    $bmp.Save('{save_path}', [System.Drawing.Imaging.ImageFormat]::Png); 
    $bmp.Dispose();
    '''
    subprocess.run(['powershell', '-Command', ps_script], check=True)
    return save_path

def image_to_base64(path):
    with open(path, 'rb') as f:
        return base64.b64encode(f.read()).decode('utf-8')

def call_vision_api(api_key, base_url, image_b64, prompt, model='qwen3-vl-plus'):
    """Call Bailian vision API."""
    url = f"{base_url.rstrip('/')}/chat/completions"
    
    body = json.dumps({
        "model": model,
        "messages": [{
            "role": "user",
            "content": [
                {"type": "text", "text": prompt},
                {"type": "image_url", "image_url": {"url": f"data:image/png;base64,{image_b64}"}}
            ]
        }],
        "max_tokens": 800,
        "temperature": 0.0
    }).encode('utf-8')
    
    req = urllib.request.Request(url, data=body, method='POST')
    req.add_header('Content-Type', 'application/json; charset=utf-8')
    req.add_header('Authorization', f'Bearer {api_key}')
    
    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            result = json.loads(resp.read().decode('utf-8'))
            return result['choices'][0]['message']['content']
    except urllib.error.HTTPError as e:
        return f"HTTP Error {e.code}: {e.read().decode('utf-8', errors='replace')}"
    except Exception as e:
        return f"Error: {e}"

DEFAULT_PROMPT = """你是逆向 QA 质检员，默认截图存在缺陷，主动排查问题，禁止先判定正常。
铁则：任意项不符即整体存在缺陷；界面可显示≠功能正常；所有结论必须标具体位置，禁"无明显问题"类模糊表述。

必查两类：
1. 功能缺失：需求的模块/按钮/弹窗/状态/交互反馈是否全存在，操作后预期变化是否生效，有无空白占位、未渲染内容
2. UI 异常：元素堆叠重叠、形状形变、错位遮挡、溢出边界、排版错乱、文字错漏/显示不全、样式失真

强制输出结构：
总结论：存在缺陷 / 无缺陷
功能缺陷：
位置：预期 → 实际
UI 缺陷：
位置：异常描述
待确认：
（可疑项必须列，不得跳过）
强制终检：重新核对全部需求与截图，确认无遗漏再输出。

需求清单：
- 启动器主窗口应完整显示
- 左侧导航栏可见，包含：下载、版本、设置、启动等入口
- 主内容区应正常渲染，无黑块/空白区域
- 按钮和交互元素应完整显示
- 文字应清晰可读，无乱码/莫吉托
- 图标应正常显示，无缺失占位符
"""

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--image', help='Path to existing screenshot')
    parser.add_argument('--save', default=os.path.join(SCRIPT_DIR, 'screenshot_check.png'),
                        help='Path to save screenshot')
    parser.add_argument('--prompt', default=DEFAULT_PROMPT, help='Custom prompt')
    parser.add_argument('--model', default='qwen3-vl-plus', help='Vision model name')
    parser.add_argument('--launch', action='store_true', 
                        help='Kill existing launcher, wait, then take screenshot')
    parser.add_argument('--wait', type=int, default=5, 
                        help='Seconds to wait before screenshot (for UI loading)')
    args = parser.parse_args()
    
    api_key, base_url = load_config()
    if not api_key:
        print("ERROR: bailian-vl API key not found in openclaw.json")
        sys.exit(1)
    
    if args.image:
        image_path = args.image
    else:
        if args.launch:
            import subprocess
            # Kill existing launcher
            subprocess.run(['taskkill', '/f', '/im', 'ShadowLauncher.exe'], 
                          capture_output=True)
            time.sleep(1)
            # Launch in dev mode
            env = os.environ.copy()
            env['SHADOW_DEV'] = '1'
            env['SHADOW_SUPPRESS_URL_LOG'] = '1'
            subprocess.Popen(
                ['D:\\latest-code\\cpp\\build\\Release\\ShadowLauncher.exe'],
                env=env
            )
            print(f"Waiting {args.wait}s for launcher...")
            time.sleep(args.wait)
        image_path = take_screenshot(args.save)
    
    print(f"Analyzing: {image_path}")
    b64 = image_to_base64(image_path)
    result = call_vision_api(api_key, base_url, b64, args.prompt, args.model)
    print("\n" + "="*60)
    print("VISION ANALYSIS RESULT")
    print("="*60)
    print(result)
    
    return result

if __name__ == '__main__':
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    main()
