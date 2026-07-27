with open('src/core/mod_loader_installer.cpp', 'r', encoding='utf-8') as f:
    c = f.read()

old = '''                // Inject MC client as library if missing (for standalone version)
                QJsonArray mergedLibs = flattened.value(QStringLiteral("libraries")).toArray();
                QString mcClientName = QStringLiteral("net.minecraft:client:") + m_mcVersion;
                bool hasMcClient = false;
                for (const auto& lib : mergedLibs) {
                    if (lib.toObject().value(QStringLiteral("name")).toString() == mcClientName) {
                        hasMcClient = true; break;
                    }
                }
                if (!hasMcClient) {'''

new = '''                // Inject MC client as library if missing (for standalone version)
                // NeoForge: skip this! The patched client is in the version folder JAR, not as a library.
                // Injecting net.minecraft:client adds the VANILLA client to classpath, which makes
                // NeoForge think the patched client is missing (it sees the unpatched vanilla one).
                if (!isNeo) {
                QJsonArray mergedLibs = flattened.value(QStringLiteral("libraries")).toArray();
                QString mcClientName = QStringLiteral("net.minecraft:client:") + m_mcVersion;
                bool hasMcClient = false;
                for (const auto& lib : mergedLibs) {
                    if (lib.toObject().value(QStringLiteral("name")).toString() == mcClientName) {
                        hasMcClient = true; break;
                    }
                }
                if (!hasMcClient) {'''

c = c.replace(old, new)

# Need to also close the if(!isNeo) block. The closing of the matchClient block is:
#     }
# And the closing of the if(!hasMcClient) block is:
#     }
# These are right before "// Copy the correct JAR", so I need to find the matching
# closing braces and add another } there.

c = c.replace('''        // Copy the correct JAR to version folder (always overwrite in case of stale file)''',
              '''        } // end if (!isNeo) for MC client injection
        // Copy the correct JAR to version folder (always overwrite in case of stale file)''')

with open('src/core/mod_loader_installer.cpp', 'w', encoding='utf-8') as f:
    f.write(c)
print('Fixed')
