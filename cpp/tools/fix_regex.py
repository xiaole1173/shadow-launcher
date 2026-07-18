path = r'D:\latest-code\cpp\src\core\version_downloader.cpp'
with open(path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Fix line 215: remove QRegularExpression call, simplify to manual loop
# The block from line 213 to 224 needs replacement

target_start = '            if (preferOfficial) {\n'
target_end_block = '                sources.append(t.url);  // BMCLAPI as fallback\n'

replacement = '''            if (preferOfficial) {
                bool mojangAdded = false;
                for (const auto& m : t.mirrors) {
                    if (!mojangAdded && (m.contains("mojang.com") || m.contains("minecraft.net"))) {
                        sources.append(m);
                        mojangAdded = true;
                    }
                }
                if (!t.url.contains("mojang.com") && !t.url.contains("minecraft.net"))
'''

with open(path, 'w', encoding='utf-8', newline='') as f:
    in_block = False
    block_end = False
    out_lines = []
    for i, line in enumerate(lines):
        if line == target_start and not in_block:
            in_block = True
            out_lines.append(replacement)
            continue
        if in_block:
            if line == target_end_block:
                block_end = True
                in_block = False
                out_lines.append(line)  # keep the sources.append line
            # skip lines in between
            continue
        out_lines.append(line)

    f.writelines(out_lines)

print('done')
