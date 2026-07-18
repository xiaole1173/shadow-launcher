import pathlib, re

root = pathlib.Path(r'D:/latest-code/cpp/src')
head = pathlib.Path(r'D:/latest-code/cpp/src')

replacements = [
    # ── Function / method / variable names ──
    ('forgeStep3_PCLinstall', 'forgeStep3_install'),

    # ── Comments with PCL/主流启动器 ──
    ('// 主流启动器-style format: "HH:mm:ss.zzz L [Module] Message"',
     '// Log format: "HH:mm:ss.zzz L [Module] Message"'),
    ('// ── Module categories (主流启动器-style: short, readable module names) ──',
     '// ── Module categories (short, readable names) ──'),
    ('// 主流启动器-aligned per-file download engine (v8).',
     '// Per-file download engine (v8).'),
    ('// 主流启动器-style cooperative cancellation: don\'t abort the reply.',
     '// Cooperative cancellation: do not abort the reply.'),
    ('// 主流启动器-style error suppression: each operation wrapped independently',
     '// Error suppression: each operation wrapped independently'),
    ('// 主流启动器-style: delete the entire version folder on cancel/fail.',
     '// On cancel/fail: delete the entire version folder'),
    ('// 主流启动器-style: delete the entire version folder on cancel/fail,',
     '// On cancel/fail: delete the entire version folder,'),
    ('// User cancelled — 主流启动器-style: delete entire version folder',
     '// User cancelled — delete entire version folder'),
    ('// PCL-style: hat layer RENDERED LARGER than face layer (56 vs 48 in 64 grid)',
     '// Hat layer rendered larger than face layer (56 vs 48 in 64 grid)'),
    ('// PCL ratios: face 48/64 = 3/4, hat 56/64 = 7/8',
     '// Ratios: face 48/64 = 3/4, hat 56/64 = 7/8'),
    ('// Strategy (matches PCL):',
     '// Strategy:'),
    ('// PCL uses registry for precision; directory scan is supplementary',
     '// Registry for precision; directory scan is supplementary'),
    ('// Specific April Fools version IDs (主流启动器 reference)',
     '// Specific April Fools version IDs'),
    ('// Any snapshot released on April 1st (主流启动器: 4/1 auto-detection)',
     '// Any snapshot released on April 1st (auto-detection)'),
    ('// PCL-style Forge install — extracts version.json from installer',
     '// Forge install — extracts version.json from installer'),
    ('// ── Step A: Try PCL extraction (install_profile.json) ──',
     '// ── Step A: Extract from install_profile.json ──'),
    ('// ── Step B: Synthetic version JSON (PCL-style, no Java needed) ──',
     '// ── Step B: Synthetic version JSON (no Java needed) ──'),
    ('// Temp .minecraft isolation (PCL-style)',
     '// Temp .minecraft isolation'),
    ('// Speed tracking (主流启动器-aligned)',
     '// Speed tracking'),
    ('// ── Record instant speed (主流启动器: SpeedLast) ──',
     '// ── Record instant speed ──'),
    ('// ── Display speed: recency-weighted average (主流启动器: Speed) ──',
     '// ── Display speed: recency-weighted average ──'),
    ('// ── Speed floor: weighted speed × 85% (主流启动器-style, matches log display) ──',
     '// ── Speed floor: weighted speed × 85% ──'),
    ('// ── Saturation check (主流启动器: If Speed >= NetTaskSpeedLimitLow) ──',
     '// ── Saturation check ──'),
    ('// 主流启动器-aligned per-file download engine (v8).',
     '// Per-file download engine (v8).'),
    ('//   - Speed floor: weighted average × 85% (主流启动器-aligned)',
     '//   - Speed floor: weighted average × 85%'),
    ('// ── 主流启动器-style speed records (v7) ──',
     '// ── Speed records (v7) ──'),
    ('static constexpr int kConcurrencyIntervalMs = 200; // ≈100ms effective (主流启动器-aligned)',
     'static constexpr int kConcurrencyIntervalMs = 200; // ≈100 ms effective'),
    ('void installOptifineSynthetic(const QByteArray& jarData);  // PCL: construct from type+patch',
     'void installOptifineSynthetic(const QByteArray& jarData);  // construct from type+patch'),
    ('                          const QByteArray& jarData);  // PCL: extract from profile',
     '                          const QByteArray& jarData);  // extract from profile'),
    ('// Continue after verify-only: start PCL install phase',
     '// Continue after verify-only: start install phase'),
    ('// PCL-style install (extract version.json → download libraries → write config)',
     '// Extract version.json → download libraries → write config'),

    # ── qDebug / log messages ──
    ('qDebug() << "[ModLoader] Continuing Forge PCL install";',
     'qDebug() << "[ModLoader] Continuing Forge install";'),
    ('qDebug() << "[ModLoader] OptiFine PCL extracted" << copied << "bundled JARs";',
     'qDebug() << "[ModLoader] OptiFine extracted" << copied << "bundled JARs";'),
    ('qDebug() << "[ModLoader] OptiFine PCL install (profile) complete',
     'qDebug() << "[ModLoader] OptiFine install (profile) complete'),
    ('qDebug() << "[ModLoader] OptiFine PCL extract failed, falling back to javaw installer";',
     'qDebug() << "[ModLoader] OptiFine extract failed, falling back to javaw installer";'),
]

def process_file(path):
    content = path.read_text('utf-8')
    original = content
    for old, new in replacements:
        content = content.replace(old, new)
    # Also catch any remaining "主流启动器" references that weren't matched
    # content = re.sub(r'// PCL\d?-?\w*:', '//', content)
    if content != original:
        path.write_text(content, 'utf-8')
        return True
    return False

count = 0
for f in root.rglob('*.cpp'):
    if process_file(f):
        count += 1
for f in root.rglob('*.h'):
    if process_file(f):
        count += 1
print(f'Updated {count} files')
