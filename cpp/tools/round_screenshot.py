# -*- coding: utf-8 -*-
"""
round_screenshot.py — 把 Shadow Launcher 矩形截图抠成圆角透明 PNG（给网站做展示图）

背景：启动器窗口是矩形窗口 + 绘制圆角（StyleTokens.radiusWindow=16 逻辑像素），
系统截图工具截出来四角带着桌面背景。本脚本按启动器圆角半径把四角抠成透明。

用法：
    python round_screenshot.py <输入图.png> [半径] [输出.png]
    python round_screenshot.py shot.png                  # 默认 r=16 -> shot_round.png
    python round_screenshot.py shot.png 24               # 系统缩放 150% 时 r=16*1.5=24
    python round_screenshot.py shot.png 16 --fill #0c0f16 # 四角填网站深色背景（不透明版）

说明：
- 半径单位是像素，与系统缩放有关：100%→16、125%→20、150%→24（16 × 缩放比例）
- 默认输出透明圆角 PNG（网页深色背景上无缝）
- --fill <色值>：输出不透明版本，四角填充指定颜色（如网站背景 #0c0f16）
"""
import sys
from PIL import Image, ImageDraw

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    fill = None
    if '--fill' in sys.argv:
        fill = sys.argv[sys.argv.index('--fill') + 1]

    if len(args) < 1:
        print(__doc__)
        sys.exit(1)

    src = args[0]
    radius = int(args[1]) if len(args) > 1 else 16
    out = args[2] if len(args) > 2 else src.rsplit('.', 1)[0] + '_round.png'

    img = Image.open(src).convert('RGBA')
    w, h = img.size
    r = min(radius, w // 2, h // 2)

    # 圆角遮罩
    mask = Image.new('L', (w, h), 0)
    d = ImageDraw.Draw(mask)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=r, fill=255)

    if fill:
        # 不透明版：底色填充 + 内容
        canvas = Image.new('RGBA', (w, h), fill)
        canvas.paste(img, (0, 0), mask)
        out_img = canvas
    else:
        # 透明版：alpha 应用圆角遮罩
        img.putalpha(mask)
        out_img = img

    out_img.save(out)
    print(f'OK: {out}  size={w}x{h}  radius={r}  fill={fill or "transparent"}')

if __name__ == '__main__':
    main()
