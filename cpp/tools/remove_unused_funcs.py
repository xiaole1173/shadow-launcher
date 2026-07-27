import os

os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

with open('src/core/mod_loader_installer.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Remove injectJarManifestAttributeAsync function
# Start: "void injectJarManifestAttributeAsync(const QString& jarPath,"
# End: "}" followed by blank line then "QString ModLoaderInstaller::computeSha1"
inj_start = content.find('void injectJarManifestAttributeAsync(const QString& jarPath,')
inj_end = content.find('}', inj_start)
inj_end = content.find('\n', inj_end + 1)  # after the closing brace
# Actually let me find the end more carefully - look for "QString ModLoaderInstaller::computeSha1"
next_func = content.find('QString ModLoaderInstaller::computeSha1(const QByteArray& data)')
# The inject function ends right before this. Let me work backwards.
# The forward decl for decompressLzma is at line 94.

# Remove decompressLzma forward declaration at line 94
fwd_decl = 'static QByteArray decompressLzma(const QByteArray& compressed);\n'
fwd_pos = content.find(fwd_decl)
if fwd_pos >= 0:
    content = content[:fwd_pos] + content[fwd_pos + len(fwd_decl):]
    print('Removed decompressLzma forward declaration')

# Remove decompressLzma implementation
impl_start = content.find('\n\nstatic QByteArray decompressLzma(const QByteArray& compressed)')
impl_end = content.find('\n}\n\nvoid ModLoaderInstaller::renameVersionFolder(', impl_start)
if impl_start >= 0 and impl_end >= 0:
    impl_end = content.find('\n}\n\n', impl_start)
    if impl_end > 0:
        impl_end += 3  # include the closing \n}
    else:
        impl_end = content.find('\n\nvoid ModLoaderInstaller::renameVersionFolder', impl_start)
    
    # Find the actual end by counting braces
    # Actually let me just find the function body's closing brace
    brace_count = 0
    i = impl_start
    started = False
    while i < len(content):
        if content[i] == '{':
            started = True
            brace_count += 1
        elif content[i] == '}':
            brace_count -= 1
            if started and brace_count == 0:
                impl_end = i + 1  # include the closing brace
                # Also skip trailing blank lines
                while impl_end < len(content) and content[impl_end] in '\n\r':
                    impl_end += 1
                break
        i += 1
    
    content = content[:impl_start] + '\n' + content[impl_end:]
    print(f'Removed decompressLzma implementation')

with open('src/core/mod_loader_installer.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print(f'Done, new file length: {len(content)}')
