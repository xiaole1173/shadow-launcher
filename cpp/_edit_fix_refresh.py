#!/usr/bin/env python3
"""Fix: re-insert refreshInstalled() that was accidentally dropped in the success block"""
with open('src/backend/version_backend.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Find where the success block ended up incorrectly
# The success block should have refreshInstalled() before the } else if
# Currently lines around 2344-2347 show:
# 2345: \n
# 2346: \n
# 2347: } else if...

# Insert refreshInstalled() between the Pure MC block and the } else if
# The Pure MC block ends at line 2344 (content line), 2345-2346 are empty
# We need: Pure MC block -> refreshInstalled -> then } else if

# Find the first } else if after the Pure MC code
for i in range(2340, min(2360, len(lines))):
    if '} else if (m_userCancelledIds' in lines[i]:
        # Insert refreshInstalled() before this line
        insert_code = (
            '\n'
            '        refreshInstalled();\n'
        )
        new_lines = lines[:i] + [insert_code] + lines[i:]
        with open('src/backend/version_backend.cpp', 'w', encoding='utf-8') as f:
            f.writelines(new_lines)
        print(f'Fixed: inserted refreshInstalled() before line {i+1}')
        break
