#!/usr/bin/env python3
"""Remove the hasPendingLoader section from onVersionDownloadFinished."""

with open('src/backend/version_backend.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# The hasPendingLoader section starts with this comment
pending_marker = '    // Check ALL sessions for pending loaders waiting for this MC version'
# And ends just before the closing brace of onVersionDownloadFinished
# Find the next function definition after this marker
closing_marker = '\nvoid VersionBackend::finishOptifineMerged'

if pending_marker not in content:
    print("marker not found, already removed?")
    exit(0)

pending_start = content.index(pending_marker)
# Find where this section ends - it's before the next function
function_transition = content.index(closing_marker, pending_start)
# Go back to the last blank line before the function
section_end = content.rfind('\n\n', pending_start, function_transition) + 1

# Extract the section to remove
old_section = content[pending_start:section_end]

# Verify it contains hasPendingLoader reference
if 'hasPendingLoader' not in old_section:
    print(f"WARNING: Section doesn't contain hasPendingLoader. Found: {old_section[:100]}")
    exit(1)

content = content.replace(old_section, '', 1)
print(f"Removed {len(old_section)} chars of hasPendingLoader code")

with open('src/backend/version_backend.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("Done")
