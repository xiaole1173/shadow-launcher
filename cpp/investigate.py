import zipfile, json, os, tempfile, glob

# 1. Check 主流启动器's version JAR - is it really the vanilla client?
pcl_jar = r'D:\Minecraft\.minecraft\versions\26.2-NeoForge_26.2.0.34-beta\26.2-NeoForge_26.2.0.34-beta.jar'
pcl_size = os.path.getsize(pcl_jar)
print(f'=== 主流启动器 版本 JAR ===')
print(f'大小: {pcl_size} bytes')

# Check inside the JAR
with zipfile.ZipFile(pcl_jar) as z:
    names = z.namelist()
    has_neoforge = any('neoforge' in n.lower() for n in names)
    has_minecraft = any('minecraft' in n.lower() for n in names)
    # Count class files
    classes = [n for n in names if n.endswith('.class')]
    print(f'条目数: {len(names)}')
    print(f'class 文件数: {len(classes)}')
    print(f'含 neoforge 相关: {has_neoforge}')
    print(f'含 minecraft 相关: {has_minecraft}')
    # Show first 20 entries
    print(f'\n前 20 个条目:')
    for n in names[:20]:
        print(f'  {n}')

print()

# 2. Check what the installer JAR's version.json has for assets/clientVersion
temp = tempfile.gettempdir()
installers = [f for f in os.listdir(temp) if 'neoforge-installer' in f and f.endswith('.jar')]
if installers:
    path = os.path.join(temp, installers[0])
    with zipfile.ZipFile(path) as z:
        vj = json.loads(z.read('version.json'))
        print(f'=== 安装器 version.json ===')
        print(f'assets: {vj.get("assets")}')
        print(f'clientVersion: {vj.get("clientVersion")}')
        # Check if net.minecraft:client is in libraries
        has_mc_client = any('net.minecraft:client' in l.get('name','') for l in vj.get('libraries',[]) if isinstance(l,dict))
        print(f'net.minecraft:client 在 libraries 中: {has_mc_client}')

print()

# 3. Check our JSON - where does net.minecraft:client come from?
our_json = r'D:\latest-code\cpp\build\Release\.minecraft\versions\26.2-neoforge-26.2.0.34-beta\26.2-neoforge-26.2.0.34-beta.json'
our = json.load(open(our_json, 'r', encoding='utf-8'))
print(f'=== 我们的 JSON ===')
print(f'assets: {our.get("assets")}')
print(f'clientVersion: {our.get("clientVersion")}')
has_mc_client = any('net.minecraft:client' in l.get('name','') for l in our.get('libraries',[]) if isinstance(l,dict))
print(f'net.minecraft:client 在 libraries 中: {has_mc_client}')
