import re

path = r"D:\latest-code\cpp\src\backend\version_backend.cpp"
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

# Find the finished handler for the primary forge download
# The pattern starts with "connect(reply, &QNetworkReply::finished, this,"
# and the handler has a fallback block

# Let me find the exact lines by reading the file
lines = content.split('\n')

# Find the connect(reply, &QNetworkReply::finished line
start = None
end = None
depth = 0
for i, line in enumerate(lines):
    if 'connect(reply, &QNetworkReply::finished, this,' in line:
        start = i
        # Skip until we find the opening brace of the lambda
        for j in range(i, min(i+5, len(lines))):
            if '{' in lines[j]:
                depth = lines[j].count('{') - lines[j].count('}')
                continue
        break

if start is None:
    print("Could not find connect(reply, finished)")
    exit(1)

# Find the end of this connect statement
# Track brace depth through the lambda
lambda_start = None
brace_depth = 0
in_connect = False

for i in range(start, len(lines)):
    line = lines[i]
    if 'connect(reply, &QNetworkReply::finished, this,' in line:
        in_connect = True
        continue
    
    if in_connect:
        if '[' in line:
            lambda_start = i
            continue
        
        if lambda_start and i == lambda_start:
            # Find the opening {
            if '{' in line:
                brace_depth += line.count('{') - line.count('}')
                continue
        elif lambda_start and '{' in line:
            brace_depth += line.count('{') - line.count('}')
        elif lambda_start and '}' in line:
            brace_depth += 0  # will be handled below
            brace_depth += line.count('{') - line.count('}')
        
        # After lambda start, track braces
        if lambda_start is not None and i > lambda_start:
            brace_depth += line.count('{') - line.count('}')
            if brace_depth <= 0:
                end = i
                break

print(f"Primary connect block: lines {start+1}-{end+1} ({end-start+1} lines)")

# Now find the BODY of the lambda (after the capture list)
# The current code at this point should be:
"""
                    connect(reply, &QNetworkReply::finished, this,

                            [this, nam, reply, installName, loaderType, loaderVersion, mcVersion, forgeInstallerBranch, loaderDlStepIdx]() {

                        reply->deleteLater();

                        if (reply->error() != QNetworkReply::NoError) {

                            // Phase 2: try official fallback
                            ... lots of code ...
                        } else {
                            // primary success
                            ... lots of code ...
                        }
                    });
"""

# Let me print the key lines around the fallback section
for i in range(start, min(end+1, start+80)):
    print(f"{i+1:6d}: {lines[i][:130]}")
