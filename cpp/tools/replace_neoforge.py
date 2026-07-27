import os

os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

with open('src/core/mod_loader_installer.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

start_marker = '// ============================================================\n\n// ============================================================\n// NeoForge — extract version from installer, download libs, write JSON'
end_marker = '\n}\n\n\nstatic QByteArray decompressLzma'

start_pos = content.find(start_marker)
end_pos = content.find(end_marker, start_pos)

if start_pos == -1 or end_pos == -1:
    print('Markers not found')
    exit(1)

replacement = '''// ============================================================
// NeoForge — now handled by runBootstrapperProcess (主流启动器's ForgelikeInjector)
// The old ~870-line manual parser has been removed.
// ============================================================


void ModLoaderInstaller::installNeoForge(const QByteArray& jarData, const QJsonObject&)
{
    Q_UNUSED(jarData)
    // This path should never be reached — NeoForge now routes through
    // runBootstrapperProcess() which matches 主流启动器's ForgelikeInjector.
    qCWarning(logLoader) << QStringLiteral("installNeoForge(const QByteArray&) called unexpectedly — bootstrapper handles NeoForge");
    emit finished(false, QStringLiteral("\u5185\u90e8\u9519\u8bef\uff1aNeoForge \u5b89\u88c5\u8def\u5f84\u5f02\u5e38"));
    m_running = false;
}'''

new_content = content[:start_pos] + replacement + content[end_pos:]
with open('src/core/mod_loader_installer.cpp', 'w', encoding='utf-8') as f:
    f.write(new_content)
print(f'Replaced {end_pos - start_pos} bytes, new file length: {len(new_content)}')
