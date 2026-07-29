#!/usr/bin/env python3
"""Fix cancelModLoaderInstall for merged contexts."""

import sys
sys.stdout.reconfigure(encoding="utf-8")

with open("D:/latest-code/cpp/src/backend/version_backend.cpp", "r", encoding="utf-8") as f:
    content = f.read()

old_func = 'void VersionBackend::cancelModLoaderInstall() {\n\n    // Cancel all active ModLoaderInstaller instances\n\n    for (auto it = m_mlInstallers.begin(); it != m_mlInstallers.end(); ++it) {\n        if (it.value()) it.value()->cancel();\n    }\n\n    // Also cancel any active VersionDownloader (merged install MC download phase)\n\n    for (auto it = m_downloaders.begin(); it != m_downloaders.end(); ++it) {\n\n        it.value()->cancel();\n\n        it.value()->disconnect();\n\n        it.value()->deleteLater();\n\n    }\n\n    m_downloaders.clear();\n\n    m_dlStates.clear();\n\n    m_activeIds.clear();\n\n    m_activeCount = 0;\n\n    // Clean up session state for all active installer sessions\n\n    for (auto it = m_mlInstallers.begin(); it != m_mlInstallers.end(); ++it) {\n        if (auto* ds = dlSession(it.key())) ds->markFailed(tr("\u5df2\u53d6\u6d88"));\n    }\n\n    setInstalling(false);\n\n    setInstallPhase(tr("\u7a7a\u95f2"));\n\n    emit logMessage(tr("\u5b89\u88c5\u5df2\u53d6\u6d88"));\n\n}'

assert old_func in content, "old text not found!"

new_func = 'void VersionBackend::cancelModLoaderInstall() {\n\n    // Cancel all active ModLoaderInstaller instances\n\n    for (auto it = m_mlInstallers.begin(); it != m_mlInstallers.end(); ++it) {\n        if (it.value()) it.value()->cancel();\n    }\n\n    // Cancel merged context installers\n    for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {\n        if (it.value() && it.value()->installer) it.value()->installer->cancel();\n        if (it.value() && it.value()->mcDownloader) it.value()->mcDownloader->cancel();\n    }\n\n    // Also cancel any active VersionDownloader (merged install MC download phase)\n\n    for (auto it = m_downloaders.begin(); it != m_downloaders.end(); ++it) {\n\n        it.value()->cancel();\n\n        it.value()->disconnect();\n\n        it.value()->deleteLater();\n\n    }\n\n    m_downloaders.clear();\n\n    m_dlStates.clear();\n\n    m_activeIds.clear();\n\n    m_activeCount = 0;\n\n    // Clean up session state for all active installer sessions\n\n    for (auto it = m_mlInstallers.begin(); it != m_mlInstallers.end(); ++it) {\n        if (auto* ds = dlSession(it.key())) ds->markFailed(tr("\u5df2\u53d6\u6d88"));\n    }\n    for (auto it = m_mergedContexts.begin(); it != m_mergedContexts.end(); ++it) {\n        if (auto* ds = dlSession(it.key())) ds->markFailed(tr("\u5df2\u53d6\u6d88"));\n    }\n\n    setInstalling(false);\n\n    setInstallPhase(tr("\u7a7a\u95f2"));\n\n    emit logMessage(tr("\u5b89\u88c5\u5df2\u53d6\u6d88"));\n\n}'

content = content.replace(old_func, new_func, 1)
print("cancelModLoaderInstall updated")

with open("D:/latest-code/cpp/src/backend/version_backend.cpp", "w", encoding="utf-8") as f:
    f.write(content)

print("Done")
