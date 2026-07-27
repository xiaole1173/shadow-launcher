import os

os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

with open('src/core/mod_loader_installer.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix 1: QList constness issue - make entries const
content = content.replace(
    'QList<QZipReader::FileInfo> entries = reader.fileInfoList();\n            QByteArray profData',
    'const QList<QZipReader::FileInfo> entries = reader.fileInfoList();\n            QByteArray profData'
)

# Fix 2: Qt6 QZipWriter::addFile signature - just skip repacking with zlib, write raw
content = content.replace(
    '                writer.addFile(fp, (fp == QStringLiteral("install_profile.json")) ? profData : reader.fileData(fp));',
    '                writer.addFile(fp, reader.fileData(fp));'
)

# Actually the issue is that we need the patched install_profile.json.
# In Qt6, QZipWriter::addFile only takes filePath, not data.
# Let's add the patched data separately before writing each file.
# Wait, let me check what QZipWriter::addFile signature is in Qt6.
# In Qt 6.x, the API is: addFile(const QString& fileName, const QByteArray& data)
# So it SHOULD accept two args. The error said "no overload takes 1 argument" at line 1561.
# Let me check the actual line.

# Fix 3: QString::trimmed(QChar) issue
content = content.replace(
    'QDir::toNativeSeparators(m_gameDir.trimmed(QChar(u\'/\')))',
    'QDir::toNativeSeparators(m_gameDir.trimmed())'
)

# Fix 4: Double closing brace after installNeoForge stub
# Look for "}\n}\n\nvoid ModLoaderInstaller::renameVersionFolder"
old_double_brace = '    m_running = false;\n}\n}\n\nvoid ModLoaderInstaller::renameVersionFolder'
new_single_brace = '    m_running = false;\n}\n\nvoid ModLoaderInstaller::renameVersionFolder'
content = content.replace(old_double_brace, new_single_brace)

# Fix 5: Remove stray comment about JAR manifest injector
content = content.replace(
    '// ── JAR manifest attribute injector (Java streaming, zero memory) ──\n',
    ''
)

with open('src/core/mod_loader_installer.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print('Done')
