with open('src/core/mod_loader_installer.cpp', 'r', encoding='utf-8') as f:
    c = f.read()

old = '''                bool inheritsLeft = flattened.contains(QStringLiteral("inheritsFrom"));
                if (inheritsLeft)
                    flattened.remove(QStringLiteral("inheritsFrom"));

                // Inject MC client as library if missing (for standalone version)'''

new = '''                bool inheritsLeft = flattened.contains(QStringLiteral("inheritsFrom"));
                if (inheritsLeft)
                    flattened.remove(QStringLiteral("inheritsFrom"));

                // NeoForge: ensure net.neoforged:neoforge:{ver}:universal is in libraries
                // FMLLoader scans classpath for NeoForgeMod.class - it's in the universal JAR.
                // The bootstrapper's version.json may omit this entry; flatten doesn't add it.
                if (isNeo) {
                    QString nfName = QStringLiteral("net.neoforged:neoforge:%1:universal").arg(m_loaderVersion);
                    QJsonArray nfLibs = flattened.value(QStringLiteral("libraries")).toArray();
                    bool hasNf = false;
                    for (const auto& lib : nfLibs) {
                        if (lib.toObject().value(QStringLiteral("name")).toString() == nfName) {
                            hasNf = true; break;
                        }
                    }
                    if (!hasNf) {
                        QString nfPath = QStringLiteral("net/neoforged/neoforge/%1/neoforge-%1-universal.jar").arg(m_loaderVersion);
                        QJsonObject artifact;
                        artifact[QStringLiteral("path")] = nfPath;
                        QJsonObject dlObj;
                        dlObj[QStringLiteral("artifact")] = artifact;
                        QJsonObject nfEntry;
                        nfEntry[QStringLiteral("name")] = nfName;
                        nfEntry[QStringLiteral("downloads")] = dlObj;
                        nfLibs.append(nfEntry);
                        flattened[QStringLiteral("libraries")] = nfLibs;
                        qCInfo(logLoader) << QStringLiteral("已添加 NeoForge universal JAR 到 libraries: %1").arg(nfName);
                    }
                }

                // Inject MC client as library if missing (for standalone version)'''

c = c.replace(old, new)
with open('src/core/mod_loader_installer.cpp', 'w', encoding='utf-8') as f:
    f.write(c)
print('Fixed')
