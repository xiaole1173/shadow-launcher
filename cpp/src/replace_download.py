import os

root = r'D:\latest-code\cpp\src'
files_to_check = ['http_client.cpp', 'mod_loader_installer.cpp', 'mod_manager.cpp', 'version_downloader.cpp']

for d, _, fs in os.walk(root):
    for f in fs:
        if f in files_to_check:
            p = os.path.join(d, f)
            with open(p, 'r', encoding='utf-8-sig', errors='replace') as fh:
                content = fh.read()
            
            # Count before
            old_count = content.count('.download(')
            
            # Replace .download( → .downloadWithFallback(
            new_content = content.replace('.download(', '.downloadWithFallback(')
            
            # Fix: don't rename the HttpClient::download function definition
            new_content = new_content.replace('void HttpClient::downloadWithFallback', 'void HttpClient::download')
            
            # Fix: don't rename the HttpClient::downloadWithFallback function definition
            new_content = new_content.replace('void HttpClient::downloadWithFallbackWithFallback', 'void HttpClient::downloadWithFallback')
            
            # Fix: single-char replacements that got mangled
            # No double-replacement should happen since our original didn't have downloadWithFallback
            
            # Count after
            new_count = new_content.count('.downloadWithFallback(')
            remaining = new_content.count('.download(')
            
            if new_content != content:
                with open(p, 'w', encoding='utf-8-sig', newline='\n') as fh:
                    fh.write(new_content)
                print(f'{f}: {old_count} -> {new_count} downloadWithFallback, {remaining} download remaining')
            else:
                print(f'{f}: no changes needed')
