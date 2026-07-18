import subprocess, os

files = [
    ('beta_agreement', r'C:\Users\蔡朝彬\Desktop\协议\Shadow Launcher 内测人员协议.docx'),
    ('terms_of_service', r'C:\Users\蔡朝彬\Desktop\协议\Shadow Launcher 用户协议.docx'),
    ('privacy_policy', r'C:\Users\蔡朝彬\Desktop\协议\Shadow Launcher 隐私协议.docx'),
]

template = r'''<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<style>
  body {{
    font-family: -apple-system, "Microsoft YaHei", "PingFang SC", sans-serif;
    font-size: 14px; line-height: 1.8; color: #e0e0e0;
    background: #0c0f16; margin: 0; padding: 24px 20px 60px;
  }}
  h1 {{ font-size: 20px; color: #ffffff; border-bottom: 1px solid #222540; padding-bottom: 12px; margin-bottom: 20px; }}
  h2 {{ font-size: 16px; color: #d0d4e8; margin-top: 24px; }}
  h3 {{ font-size: 14px; color: #b0b8d0; }}
  p {{ margin: 8px 0; }}
  ul, ol {{ padding-left: 24px; }}
  li {{ margin: 4px 0; }}
  strong {{ color: #ffffff; }}
  a {{ color: #6090d0; }}
</style>
</head>
<body>
{body}
</body>
</html>'''

outdir = r'D:\latest-code\cpp\qml\agreements'
titles = ['Shadow Launcher 内测人员协议', 'Shadow Launcher 用户协议', 'Shadow Launcher 隐私协议']

for (name, src), title in zip(files, titles):
    dst = os.path.join(outdir, name + '.html')
    r = subprocess.run(['pandoc', src, '-f', 'docx', '-t', 'html', '--standalone'],
                       capture_output=True, encoding='utf-8', errors='replace')
    body = r.stdout
    if r.returncode != 0 or not body:
        print('FAIL', name, r.stderr[:200] if r.stderr else 'no output')
        continue
    if '<body>' in body:
        body = body.split('<body>',1)[1].split('</body>',1)[0]
    html = template.format(title=title, body=body)
    with open(dst, 'w', encoding='utf-8') as f:
        f.write(html)
    print('OK', name + '.html', len(html), 'bytes')
