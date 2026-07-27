import os

os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

with open('src/core/mod_loader_installer.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Find and remove the entire anonymous namespace block containing injectJarManifestAttributeAsync
start_marker = 'void injectJarManifestAttributeAsync(const QString& jarPath,'
end_marker = '} // anonymous namespace\n'

start_pos = content.find(start_marker)
end_pos = content.find(end_marker, start_pos)

if start_pos >= 0 and end_pos >= 0:
    # Find the line before to include blank lines
    line_start = content.rfind('\n', 0, start_pos - 1)
    if line_start >= 0:
        start_pos = line_start
    
    end_pos = end_pos + len(end_marker)
    content = content[:start_pos] + content[end_pos:]
    print(f'Removed injectJarManifestAttributeAsync and anonymous namespace, from byte {start_pos} to {end_pos}')
else:
    print(f'Markers not found: start={start_pos}, end={end_pos}')

with open('src/core/mod_loader_installer.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print(f'Done, new file length: {len(content)}')
