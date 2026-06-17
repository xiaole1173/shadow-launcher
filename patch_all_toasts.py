"""Add toast notifications to all actionable buttons without them."""
import os

# ── MainWindow.qml ──
with open('shadow_launcher/ui/qml/MainWindow.qml', 'r', encoding='utf-8') as f:
    mw = f.read()

mw_patches = [
    # 1. 正版登录
    ('loginMode = 0; if (backend) backend.setLastLoginMode(0)',
     'loginMode = 0; if (backend) { backend.setLastLoginMode(0); toastManager.show("已切换至正版登录") }'),
    # 2. 离线模式
    ('loginMode = 1; if (backend) backend.setLastLoginMode(1)',
     'loginMode = 1; if (backend) { backend.setLastLoginMode(1); toastManager.show("已切换至离线模式") }'),
    # 3. 登出
    ('if (backend) backend.logout() }',
     'if (backend) { backend.logout(); toastManager.show("已登出") } }'),
    # 4. 版本选择 (not really an action, skip adding toast — it's just opening overlay)
    # 5. ← 返回启动 — navigation, skip
    # 6. 安装新版本 — navigates to download page
    ('showVersionSelect = false; switchPage(1)',
     'showVersionSelect = false; switchPage(1); toastManager.show("正在前往下载页面")'),
    # 7. ← 返回 — navigation, skip
    # 8. 自动检测 Java — need to check what's already there
    # 9. 浏览 Java
    ('if (backend) backend.pickJava()',
     'if (backend) { var ok = backend.pickJava(); if (ok) toastManager.show("已选择 Java 路径") }'),
    # 10. Mod 刷新
    ('if (backend) { var m = backend.listMods(); for (var i = 0;',
     'if (backend) { var m = backend.listMods(); for (var i = 0;'),
    # Actually need to add toast AFTER the for loop
    # Let me use the full block for mod refresh
    ('modListModel.clear(); if (backend) { var m = backend.listMods(); for (var i = 0; i < m.length; i++) { modListModel.append({ "name": m[i] }) } } }',
     'modListModel.clear(); if (backend) { var m = backend.listMods(); for (var i = 0; i < m.length; i++) { modListModel.append({ "name": m[i] }) }; toastManager.show("Mod 列表已刷新") } }'),
    # 11. Mod 删除
    ('if (backend) backend.deleteMod(name); modListModel.remove(index) }',
     'if (backend) { backend.deleteMod(name); modListModel.remove(index); toastManager.show("已删除 Mod: " + name) } }'),
    # 12. 资源包刷新
    ('rpListModel.clear(); if (backend) { var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++) { rpListModel.append({ "name": p[i] }) } } }',
     'rpListModel.clear(); if (backend) { var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++) { rpListModel.append({ "name": p[i] }) }; toastManager.show("资源包列表已刷新") } }'),
    # 13. 资源包打开文件夹
    ('if (backend) { var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++)',
     'if (backend) { backend.openResourcePacksFolder(); toastManager.show("已打开资源包文件夹"); var p = backend.listResourcePacks(); for (var i = 0; i < p.length; i++)'),
    # 14. 资源包删除
    ('if (backend) backend.deleteResourcePack(name); rpListModel.remove(index) }',
     'if (backend) { backend.deleteResourcePack(name); rpListModel.remove(index); toastManager.show("已删除资源包: " + name) } }'),
    # 15. 存档刷新
    ('saveListModel.clear(); if (backend) { var s = backend.listSaves(); for (var i = 0; i < s.length; i++) { saveListModel.append({ "name": s[i] }) } } }',
     'saveListModel.clear(); if (backend) { var s = backend.listSaves(); for (var i = 0; i < s.length; i++) { saveListModel.append({ "name": s[i] }) }; toastManager.show("存档列表已刷新") } }'),
    # 16. 存档删除
    ('if (backend) backend.deleteSave(name); saveListModel.remove(index) }',
     'if (backend) { backend.deleteSave(name); saveListModel.remove(index); toastManager.show("已删除存档: " + name) } }'),
]

applied_mw = 0
for old, new in mw_patches:
    if old in mw:
        mw = mw.replace(old, new)
        applied_mw += 1
    else:
        print(f'MW WARNING NOT FOUND: {old[:60]}...')

with open('shadow_launcher/ui/qml/MainWindow.qml', 'w', encoding='utf-8') as f:
    f.write(mw)
print(f'MainWindow: {applied_mw}/{len(mw_patches)} applied')

# ── DownloadPage.qml ──
with open('shadow_launcher/ui/qml/DownloadPage.qml', 'r', encoding='utf-8') as f:
    dp = f.read()

dp_patches = [
    # 刷新版本列表
    ('if (backend) backend.refreshVersionList()',
     'if (backend) { backend.refreshVersionList(); toastManager.show("版本列表已刷新") }'),
]
applied_dp = 0
for old, new in dp_patches:
    if old in dp:
        dp = dp.replace(old, new)
        applied_dp += 1
    else:
        print(f'DP WARNING NOT FOUND: {old[:60]}...')

with open('shadow_launcher/ui/qml/DownloadPage.qml', 'w', encoding='utf-8') as f:
    f.write(dp)
print(f'DownloadPage: {applied_dp}/{len(dp_patches)} applied')

# ── DownloadProgressPage.qml ──
with open('shadow_launcher/ui/qml/DownloadProgressPage.qml', 'r', encoding='utf-8') as f:
    dpp = f.read()

dpp_patches = [
    # 取消队列下载
    ('if (backend) backend.cancelQueuedDownload(modelData.versionId)',
     'if (backend) { backend.cancelQueuedDownload(modelData.versionId); toastManager.show("已取消下载") }'),
    # 取消下载/安装
    ('if (backend) backend.cancelInstall()',
     'if (backend) { backend.cancelInstall(); toastManager.show("已取消安装") }'),
]
applied_dpp = 0
for old, new in dpp_patches:
    if old in dpp:
        dpp = dpp.replace(old, new)
        applied_dpp += 1
    else:
        print(f'DPP WARNING NOT FOUND: {old[:60]}...')

with open('shadow_launcher/ui/qml/DownloadProgressPage.qml', 'w', encoding='utf-8') as f:
    f.write(dpp)
print(f'DownloadProgressPage: {applied_dpp}/{len(dpp_patches)} applied')

# ── Verify ──
for fname in ['MainWindow.qml', 'DownloadPage.qml', 'DownloadProgressPage.qml']:
    fpath = f'shadow_launcher/ui/qml/{fname}'
    with open(fpath, 'r', encoding='utf-8') as f:
        verify = f.read()
    brace_ok = verify.count('{') == verify.count('}')
    fffd = verify.count('\ufffd')
    print(f'{fname}: braces={brace_ok} fffd={fffd}')
