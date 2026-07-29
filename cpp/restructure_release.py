# Shadow Launcher Release 目录结构重组工具
# 将散乱的 release 文件夹整理为 v0.4.0-beta 新结构：
#   Release/
#   ├── ShadowLauncher.exe    ← 主程序
#   ├── qt.conf               ← Qt 路径重定向
#   ├── launcher/             ← 其他所有文件
#   └── .minecraft/           ← 不动

import os, sys, shutil

def restructure(release_dir):
    release_dir = os.path.abspath(release_dir)
    launcher_dir = os.path.join(release_dir, 'launcher')
    
    if not os.path.isfile(os.path.join(release_dir, 'ShadowLauncher.exe')):
        print(f'[错误] {release_dir} 中找不到 ShadowLauncher.exe')
        return False
    
    if os.path.isdir(launcher_dir):
        print(f'[跳过] launcher/ 已存在，可能是新结构')
        return True
    
    print(f'重组: {release_dir}')
    os.makedirs(launcher_dir, exist_ok=True)
    
    # 需要移动的目录
    dirs_to_move = [
        'platforms', 'qml', 'styles', 'imageformats',
        'iconengines', 'tls', 'multimedia', 'texture',
        'bearer', 'audio', 'generic', 'canbus',
        'position', 'sensorgestures', 'sensors',
        'playlistformats', 'mediaservice', 'webview',
        'qpa', 'scenegraph', 'translations',
        'skins', 'bin',
    ]
    
    # 需要移动的文件（通配符用 startsWith 匹配）
    file_prefixes = [
        'Qt6', 'D3Dcompiler', 'msvcp', 'vcruntime', 'concrt',
    ]
    file_exact = [
        'opengl32sw.dll', 'QtWebEngineProcess.exe',
        'SLUpdater.exe', 'build_info.txt',
    ]
    
    moved_dirs = 0
    moved_files = 0
    
    # 移动目录
    for d in dirs_to_move:
        src = os.path.join(release_dir, d)
        if os.path.isdir(src):
            dst = os.path.join(launcher_dir, d)
            shutil.move(src, dst)
            moved_dirs += 1
            print(f'  目录: {d}/')
    
    # 移动文件
    for f in os.listdir(release_dir):
        src = os.path.join(release_dir, f)
        if not os.path.isfile(src):
            continue
        
        # 跳过核心文件
        if f in ('ShadowLauncher.exe', 'qt.conf', 'compat.json'):
            continue
        if f.startswith('.minecraft'):  # .minecraft 可能是目录
            continue
        
        should_move = False
        for prefix in file_prefixes:
            if f.startswith(prefix):
                should_move = True
                break
        if f in file_exact:
            should_move = True
        
        if should_move:
            dst = os.path.join(launcher_dir, f)
            shutil.move(src, dst)
            moved_files += 1
            print(f'  文件: {f}')
    
    # 创建/更新 qt.conf
    qt_conf = os.path.join(release_dir, 'qt.conf')
    with open(qt_conf, 'w', encoding='utf-8') as f:
        f.write('[Paths]\n')
        f.write('Plugins = launcher/plugins\n')
        f.write('Qml2Imports = launcher/qml\n')
        f.write('Translations = launcher/translations\n')
    print(f'  qt.conf 已更新')
    
    print(f'\n完成: 移动了 {moved_dirs} 个目录, {moved_files} 个文件')
    print(f'新结构:')
    print(f'  {release_dir}\\')
    print(f'  ├── ShadowLauncher.exe')
    print(f'  ├── qt.conf')
    print(f'  └── launcher/  ({sum(1 for _ in os.scandir(launcher_dir) if _.is_dir())} 个子目录)')
    return True

if __name__ == '__main__':
    if len(sys.argv) > 1:
        restructure(sys.argv[1])
    else:
        # 默认路径：build/Release/
        script_dir = os.path.dirname(os.path.abspath(__file__))
        default_dir = os.path.join(script_dir, 'build', 'Release')
        if os.path.isdir(default_dir):
            restructure(default_dir)
        else:
            print(f'用法: python {sys.argv[0]} [release目录路径]')
            print(f'示例: python {sys.argv[0]} D:\\latest-code\\cpp\\build\\Release')
