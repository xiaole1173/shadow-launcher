with open('src/core/mod_loader_installer.cpp', 'r', encoding='utf-8') as f:
    c = f.read()

old = '''        // Copy the correct JAR to version folder
        // Forge: client.jar from libraries/ (patched by installer), fallback universal.jar
        // NeoForge: vanilla MC client from versions/{mcVersion}/{mcVersion}.jar
        //           (the patched client is in libraries/net/neoforged/minecraft-client-patched/)
        QString targetDir = versionsDir() + QStringLiteral("/") + m_installName;
        QString jarPathV = targetDir + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
        if (!QFile::exists(jarPathV)) {
            bool copied = false;
            if (!isNeo) {
                const QString clientJar = m_gameDir + QStringLiteral("/libraries/") + loaderGroup
                    + QStringLiteral("/") + ver + QStringLiteral("/")
                    + filePrefix + QStringLiteral("-") + ver + QStringLiteral("-client.jar");
                const QString universalJar = m_gameDir + QStringLiteral("/libraries/") + loaderGroup
                    + QStringLiteral("/") + ver + QStringLiteral("/")
                    + filePrefix + QStringLiteral("-") + ver + QStringLiteral("-universal.jar");
                if (QFile::exists(clientJar) && QFile::copy(clientJar, jarPathV)) {
                    qCInfo(logLoader) << QStringLiteral("已复制 Forge client JAR 到 %1").arg(jarPathV);
                    copied = true;
                } else if (QFile::exists(universalJar) && QFile::copy(universalJar, jarPathV)) {
                    qCInfo(logLoader) << QStringLiteral("已复制 Forge universal JAR 到 %1").arg(jarPathV);
                    copied = true;
                }
            } else {
                // NeoForge: copy vanilla MC client (flatten removed inheritsFrom, so version folder needs its own jar)
                const QString mcClientPath = m_gameDir + QStringLiteral("/versions/") + m_mcVersion
                    + QStringLiteral("/") + m_mcVersion + QStringLiteral(".jar");
                if (QFile::exists(mcClientPath) && QFile::copy(mcClientPath, jarPathV)) {
                    qCInfo(logLoader) << QStringLiteral("已复制原版 MC client JAR 到 %1").arg(jarPathV);
                    copied = true;
                }
            }
            if (!copied) {
                qCWarning(logLoader) << QStringLiteral("无法复制版本 JAR 文件");
            }
        }
    }'''

new = '''        // Copy the correct JAR to version folder
        // Forge: -client.jar (installer-patched), fallback -universal.jar
        // NeoForge: minecraft-client-patched-{ver}.jar from libraries/net/neoforged/minecraft-client-patched/
        QString targetDir = versionsDir() + QStringLiteral("/") + m_installName;
        QString jarPathV = targetDir + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
        if (!QFile::exists(jarPathV)) {
            bool copied = false;
            if (!isNeo) {
                const QString clientJar = m_gameDir + QStringLiteral("/libraries/") + loaderGroup
                    + QStringLiteral("/") + ver + QStringLiteral("/")
                    + filePrefix + QStringLiteral("-") + ver + QStringLiteral("-client.jar");
                const QString universalJar = m_gameDir + QStringLiteral("/libraries/") + loaderGroup
                    + QStringLiteral("/") + ver + QStringLiteral("/")
                    + filePrefix + QStringLiteral("-") + ver + QStringLiteral("-universal.jar");
                if (QFile::exists(clientJar) && QFile::copy(clientJar, jarPathV)) {
                    qCInfo(logLoader) << QStringLiteral("已复制 Forge client JAR 到 %1").arg(jarPathV);
                    copied = true;
                } else if (QFile::exists(universalJar) && QFile::copy(universalJar, jarPathV)) {
                    qCInfo(logLoader) << QStringLiteral("已复制 Forge universal JAR 到 %1").arg(jarPathV);
                    copied = true;
                }
            } else {
                // NeoForge: copy the processor-patched client JAR
                // Processor output: libraries/net/neoforged/minecraft-client-patched/{loaderVer}/minecraft-client-patched-{loaderVer}.jar
                const QString patchedDir = m_gameDir + QStringLiteral("/libraries/net/neoforged/minecraft-client-patched/")
                    + m_loaderVersion;
                const QString patchedJar = patchedDir + QStringLiteral("/minecraft-client-patched-")
                    + m_loaderVersion + QStringLiteral(".jar");
                if (QFile::exists(patchedJar) && QFile::copy(patchedJar, jarPathV)) {
                    qCInfo(logLoader) << QStringLiteral("已复制 NeoForge patched client JAR 到 %1").arg(jarPathV);
                    copied = true;
                } else {
                    qCWarning(logLoader) << QStringLiteral("未找到 NeoForge patched client JAR: %1").arg(patchedJar);
                }
            }
            if (!copied) {
                qCWarning(logLoader) << QStringLiteral("无法复制版本 JAR 文件");
            }
        }
    }'''

c = c.replace(old, new)
open('src/core/mod_loader_installer.cpp', 'w', encoding='utf-8').write(c)
print('Fixed')
