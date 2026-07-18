import pathlib, re

def replace_in_file(path, replacements):
    content = pathlib.Path(path).read_text('utf-8')
    for old, new in replacements:
        content = content.replace(old, new)
    pathlib.Path(path).write_text(content, 'utf-8')
    print(f'  {pathlib.Path(path).name}: {len(replacements)} replacements')

# === main.cpp ===
base = pathlib.Path(r'D:/latest-code/cpp/src')
replace_in_file(base / 'main.cpp', [
    # Simplify checkpoint - remove thread ID and STARTUP timing
    ('auto checkpoint = [&](const QString& stage) {\n        qCInfo(logApp) << "[STARTUP +" << startupTimer.elapsed() << "ms] " << stage\n                       << "(thread:" << QThread::currentThreadId() << ")";\n    };',
     'auto checkpoint = [&](const QString& stage) { qCInfo(logApp) << stage; };'),
    # First frame render
    ('qCInfo(logApp) << "[STARTUP +" << pStartupTimer->elapsed() << "ms] First frame rendered";',
     'qCInfo(logApp) << QStringLiteral("首帧渲染完成");'),
    # Version banner
    ('qCInfo(logApp) << "=== Shadow Launcher v" << app.applicationVersion() << "starting ===" << "PID:" << QCoreApplication::applicationPid();',
     'qCInfo(logApp) << QStringLiteral("启动器已启动  v") << app.applicationVersion() << QStringLiteral("  PID:") << QCoreApplication::applicationPid();'),
    ('qCInfo(logApp) << "Loaded translation:" << lang;',
     'qCInfo(logApp) << QStringLiteral("语言已加载: ") << lang;'),
    ('qCInfo(logApp) << "Using source language (zh_CN)";',
     'qCInfo(logApp) << QStringLiteral("使用默认语言");'),
])

# === account_backend.cpp ===
replace_in_file(base / 'backend/account_backend.cpp', [
    ('<< "AccountBackend constructed"', '<< QStringLiteral("账号模块已初始化")'),
    ('<< "Microsoft login SUCCESS:"', '<< QStringLiteral("正版登录成功: ")'),
    ('<< "Offline login:"', '<< QStringLiteral("离线登录: ")'),
    ('<< "UUID:"', '<< QStringLiteral(" UUID:")'),
    ('<< "Offline skin preview:"', '<< QStringLiteral("离线皮肤预览: ")'),
    ('<< "\u2192"', '<< QStringLiteral(" -> ")'),
    ('<< "Offline head already cached:"', '<< QStringLiteral("离线头像已缓存: ")'),
    ('<< "Offline head rendered + saved:"', '<< QStringLiteral("离线头像已渲染: ")'),
    ('<< "Offline head copied:"', '<< QStringLiteral("离线头像已复制: ")'),
    ('<< "Logged out";', '<< QStringLiteral("已登出");'),
    ('<< "Embedded login"', '<< QStringLiteral("内嵌登录")'),
    ('<< "enabled"', '<< QStringLiteral("已启用")'),
    ('<< "disabled"', '<< QStringLiteral("已禁用")'),
    ('<< "Using embedded CEF login";', '<< QStringLiteral("使用内嵌浏览器登录");'),
    ('<< "Using localhost callback login";', '<< QStringLiteral("使用本地回调登录");'),
    ('<< "Background refresh triggered";', '<< QStringLiteral("后台刷新已触发");'),
    ('<< "Background token refresh timer started (1h interval)";', '<< QStringLiteral("后台令牌刷新已启动");'),
    ('<< "renderHead3D: skin format"', '<< QStringLiteral("皮肤格式: ")'),
    ('<< "renderHead3D: hasHat="', '<< QStringLiteral("皮肤帽子: ")'),
    ('<< ", format="', '<< QStringLiteral(" 格式:")'),
    ('<< "PCL-2D head:"', '<< QStringLiteral("头像: ")'),
    ('<< "Skin from cache, face:"', '<< QStringLiteral("皮肤缓存: ")'),
    ('<< "Skin: placeholder for"', '<< QStringLiteral("皮肤占位: ")'),
    ('<< "Skin: using cached face:"', '<< QStringLiteral("皮肤缓存: ")'),
    ('<< "Skin: rendered from cached raw:"', '<< QStringLiteral("皮肤渲染缓存: ")'),
    ('<< "Skin: fetching from Mojang API..."', '<< QStringLiteral("正在获取Mojang皮肤...")'),
    ('<< "UUID corrected from Mojang:"', '<< QStringLiteral("UUID已修正: ")'),
    ('<< "Found cape:"', '<< QStringLiteral("披风: ")'),
    ('<< "Cape cached:"', '<< QStringLiteral("披风已缓存: ")'),
    ('<< "No active skin in Mojang profile, using default"', '<< QStringLiteral("无Mojang皮肤,使用默认")'),
    ('<< "Downloading skin from Mojang CDN:"', '<< QStringLiteral("下载Mojang皮肤: ")'),
    ('<< "Online skin + face:"', '<< QStringLiteral("皮肤下载完成: ")'),
    ('<< "Microsoft session saved (encrypted):"', '<< QStringLiteral("微软会话已保存: ")'),
    ('<< "Field decrypt failed, trying plaintext:"', '<< QStringLiteral("解密失败,尝试明文: ")'),
    ('<< "Migrating plaintext session to encrypted"', '<< QStringLiteral("迁移明文会话至加密存储")'),
    ('<< "Found valid Microsoft session:"', '<< QStringLiteral("找到微软会话: ")'),
    ('<< "Saved session has no refresh token, clearing"', '<< QStringLiteral("会话无刷新令牌,已清除")'),
    ('<< "Saved session is empty, starting fresh"', '<< QStringLiteral("会话为空,重新开始")'),
    ('<< "Session MC token expired, reusing refresh token..."', '<< QStringLiteral("MC令牌过期,使用刷新令牌...")'),
    ('<< "Saved Microsoft session fully expired, clearing"', '<< QStringLiteral("微软会话已过期,已清除")'),
    ('<< "Token refresh already in progress, skipping duplicate request"', '<< QStringLiteral("令牌刷新中,跳过重复请求")'),
    ('<< "Attempting Microsoft token refresh..."', '<< QStringLiteral("正在刷新微软令牌...")'),
    ('<< "Microsoft token refreshed! Now refreshing MC token..."', '<< QStringLiteral("微软令牌已刷新,获取MC令牌...")'),
    ('<< "TokenCrypto: loaded existing key"', '<< QStringLiteral("密钥已加载")'),
])

# === app_backend.cpp ===
pathlib.Path(base / 'backend/app_backend.cpp').write_text(
    pathlib.Path(base / 'backend/app_backend.cpp').read_text('utf-8')
    .replace('<< "AppBackend constructed',
             '<< QStringLiteral("应用模块已初始化'),
    'utf-8')

# === launch_backend.cpp ===
pathlib.Path(base / 'backend/launch_backend.cpp').write_text(
    pathlib.Path(base / 'backend/launch_backend.cpp').read_text('utf-8')
    .replace('<< "LaunchBackend constructed";',
             '<< QStringLiteral("启动模块已初始化");'),
    'utf-8')

print('\n=== All files updated ===')
