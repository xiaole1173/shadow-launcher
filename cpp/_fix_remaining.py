#!/usr/bin/env python3
"""Fix cancelModLoaderInstall and isModLoaderInstalling for merged contexts."""

with open('src/backend/version_backend.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Fix isModLoaderInstalling to also check merged contexts
old_is = '''bool VersionBackend::isModLoaderInstalling() const {

    for (auto it = m_mlInstallers.constBegin(); it != m_mlInstallers.constEnd(); ++it) {
        if (it.value() && it.value()->isRunning()) return true;
    }
    return false;

}'''

new_is = '''bool VersionBackend::isModLoaderInstalling() const {

    for (auto it = m_mlInstallers.constBegin(); it != m_mlInstallers.constEnd(); ++it) {
        if (it.value() && it.value()->isRunning()) return true;
    }
    for (auto it = m_mergedContexts.constBegin(); it != m_mergedContexts.constEnd(); ++it) {
        if (it.value() && it.value()->installer && it.value()->installer->isRunning()) return true;
    }
    return false;

}'''

if old_is in content:
    content = content.replace(old_is, new_is, 1)
    print("isModLoaderInstalling updated")
else:
    print("isModLoaderInstalling: old text not found!")

# 2. Fix cancelModLoaderInstall to also cancel merged contexts
old_cancel_start = 'void VersionBackend::cancelModLoaderInstall() {'
old_cancel_end = '        if (auto* ds = dlSession(it.key())) ds->markFailed(tr("\u5df2\u53d6\u6d88"));'
old_cancel_full = old_cancel_start + '''
    // Cancel all active ModLoaderInstaller instances

    for (auto it = m_mlInstallers.begin(); it != m_mlInstallers.end(); ++it) {
        if (it.value()) it.value()->cancel();
    }

    // Also cancel any active VersionDownloader (merged install MC download phase)

    for (auto it = m_downloaders.begin(); it != m_downloaders.end(); ++it) {

        it.value()->cancel();

        it.value()->disconnect();

        it.value()->deleteLater();

    }

    m_downloaders.clear();

    m_dlStates.clear();

    m_activeIds.clear();

    m_activeCount = 0;

    // Clean up session state for all active installer sessions


    for (auto it = m_mlInstallers.begin(); it != m_mlInstallers.end(); ++it) {
        if (auto* ds = dlSession(it.key())) ds->markFailed(tr("\u5df2\u53d6\u6d88"));
    }

    setInstalling(false);

    setInstallPhase(tr("\u7a7a\u95f2"));

    emit logMessage(tr("\u5b89\u88c5\u5df2\u53d6\u6d88"));

}'''

new_cancel = '''void VersionBackend::cancelModLoaderInstall() {

    // Cancel all active ModLoaderInstaller instances

    for (auto it = m_mlInstallers.begin(); it != m_mlInstallers.end(); ++it) {
        if (it.value()) it.value()->cancel();
    }

    // Cancel merged context installers
    for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {
        if (it.value() && it.value()->installer) it.value()->installer->cancel();
        if (it.value() && it.value()->mcDownloader) it.value()->mcDownloader->cancel();
    }

    // Also cancel any active VersionDownloader (merged install MC download phase)

    for (auto it = m_downloaders.begin(); it != m_downloaders.end(); ++it) {

        it.value()->cancel();

        it.value()->disconnect();

        it.value()->deleteLater();

    }

    m_downloaders.clear();

    m_dlStates.clear();

    m_activeIds.clear();

    m_activeCount = 0;

    // Clean up session state for all active installer sessions


    for (auto it = m_mlInstallers.begin(); it != m_mlInstallers.end(); ++it) {
        if (auto* ds = dlSession(it.key())) ds->markFailed(tr("\u5df2\u53d6\u6d88"));
    }
    for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {
        if (auto* ds = dlSession(it.key())) ds->markFailed(tr("\u5df2\u53d6\u6d88"));
    }

    setInstalling(false);

    setInstallPhase(tr("\u7a7a\u95f2"));

    emit logMessage(tr("\u5b89\u88c5\u5df2\u53d6\u6d88"));

}'''

if old_cancel_full in content:
    content = content.replace(old_cancel_full, new_cancel, 1)
    print("cancelModLoaderInstall updated")
else:
    print("cancelModLoaderInstall: old text not found!")

with open('src/backend/version_backend.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("Done")
