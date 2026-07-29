#!/usr/bin/env python3
"""Remove the hasPendingLoader section from onVersionDownloadFinished precisely."""

with open('src/backend/version_backend.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# The section starts with this comment
pending_start_marker = '\n    // Check ALL sessions for pending loaders waiting for this MC version\n'

# The section ends just before the closing brace of onVersionDownloadFinished
# Find the closing brace by looking for "qCDebug(logLaunch) << \"[DOWNLOAD] finished=\" "
# which is the LAST line before the brace in the original code

start = content.index(pending_start_marker)

# Find the closing brace of onVersionDownloadFinished
# The next function definition after it is finishOptifineMerged
next_func = '\n\n\nvoid VersionBackend::finishOptifineMerged'
end = content.index(next_func, start)

# Go back from next_func to find where the closing brace } of onVersionDownloadFinished is
# Look for the last \n} before next_func
closing_brace_pos = content.rfind('\n}\n', start, end)
if closing_brace_pos >= 0:
    # The section to remove starts at 'start' and goes UP TO (not including) the closing brace
    # We want to keep the closing brace
    section_end = closing_brace_pos + 2  # Keep the \n}\n (closing brace)
    old_section = content[start:closing_brace_pos]
    # Verify it contains hasPendingLoader
    if 'hasPendingLoader' in old_section:
        content = content[:start] + content[closing_brace_pos:]
        print(f"Removed {len(old_section)} chars successfully")
    else:
        print("ERROR: section doesn't contain hasPendingLoader")
        # Print what we found 
        print(old_section[:200])
        exit(1)
else:
    print("ERROR: could not find closing brace")
    exit(1)

with open('src/backend/version_backend.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("Done")
