# BUILD CLEAN — SAFE (whitelist only, never recursive delete)
# NEVER run Remove-Item build -Recurse -Force

# Instead, use this:
cmake --build build --target clean

# Or if full clean needed: delete only known build artifacts
Get-ChildItem build\Release -File -Include *.obj,*.pdb,*.ilk,*.lib,*.exp,*.tlog,*.log -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force
