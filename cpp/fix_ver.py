# Fix forgeMavenVer references in version_backend.cpp
path = r"D:\latest-code\cpp\src\backend\version_backend.cpp"
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace('forgeMavenVer()', 'm_forgeMavenVer')
content = content.replace('QString forgeMavenVer = m_forgeMavenVer;', 'QString fmv = m_forgeMavenVer;')
content = content.replace('.arg(forgeMavenVer)', '.arg(fmv)')

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)
print('Done')
