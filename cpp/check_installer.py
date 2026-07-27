import zipfile, json, os, tempfile, glob

# Check temp dir for downloaded NeoForge installer
temp = tempfile.gettempdir()
installers = [f for f in os.listdir(temp) if 'neoforge-installer' in f and f.endswith('.jar')]
if installers:
    print(f'Found installer: {installers[0]}')
    path = os.path.join(temp, installers[0])
    with zipfile.ZipFile(path) as z:
        names = z.namelist()
        if 'version.json' in names:
            vj = json.loads(z.read('version.json'))
            libs = vj.get('libraries', [])
            print(f'\n=== version.json libraries ({len(libs)} entries) ===')
            for lib in libs:
                name = lib.get('name', '') if isinstance(lib, dict) else str(lib)
                if 'neoforge' in name.lower() or 'forge' in name.lower():
                    print(f'  {name}')
            # Check specifically for neoForge universal
            has_universal = any(
                'neoforge' in l.get('name','') and 'universal' in l.get('name','')
                for l in libs if isinstance(l, dict)
            )
            print(f'\nHas universal entry: {has_universal}')
        else:
            print('No version.json in installer')
else:
    print('No installer found in temp')
    # Check libraries directory
    jars = list(glob.glob(r'D:\latest-code\cpp\build\Release\.minecraft\libraries\net\neoforged\neoforge\*\*.jar'))
    if jars:
        print(f'Found NeoForge JAR: {jars[0]}')
