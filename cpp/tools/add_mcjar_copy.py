with open('src/core/mod_loader_installer.cpp', 'r', encoding='utf-8') as f:
    c = f.read()

# Add vanilla MC client JAR copy before emit finished(true)
old = '''    }

    emit finished(true, QString());
    m_running = false;
}
void ModLoaderInstaller::fabricStep1_downloadProfile()'''

# Find the right location - it should be after the JAR copy section closing brace
# The current structure in finalizeBootstrapperInstall ends with:
#   } // closes if(!m_postJsonPath.isEmpty())
#   emit finished(true);
#   m_running = false;
# }
# 
# But we also have the universal JAR injection code block that ends before emit.
# Let me find the exact pattern.

# Actually, let me find the emit in finalizeBootstrapperInstall by looking for the
# specific pattern around line 2503
idx = c.find('}\n\n    emit finished(true, QString());\n    m_running = false;\n}\nvoid ModLoaderInstaller::fabricStep1_downloadProfile')
if idx < 0:
    # Try without the extra newline
    idx = c.find('\n    emit finished(true, QString());\n    m_running = false;\n}\nvoid ModLoaderInstaller::fabricStep1_downloadProfile')
    
if idx >= 0:
    new = '''    }

        // 主流启动器: copy vanilla MC client JAR to version folder (needed by launcher for classpath)
        // The bootstrapper doesn't create a JAR in the version folder; 主流启动器's MC download + MergeJson does.
        // Source: versions/{mcVer}/{mcVer}.jar → Target: versions/{installName}/{installName}.jar
        QString mcClientSrc = m_gameDir + QStringLiteral("/versions/") + m_mcVersion
            + QStringLiteral("/") + m_mcVersion + QStringLiteral(".jar");
        QString mcClientDst = versionsDir() + QStringLiteral("/") + m_installName
            + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
        if (QFile::exists(mcClientSrc) && !QFile::exists(mcClientDst)) {
            QDir().mkpath(QFileInfo(mcClientDst).absolutePath());
            if (QFile::copy(mcClientSrc, mcClientDst))
                qCInfo(logLoader) << QStringLiteral("已复制原版客户端 JAR 到版本文件夹: %1").arg(mcClientDst);
            else
                qCWarning(logLoader) << QStringLiteral("复制原版客户端 JAR 失败: %1 -> %2").arg(mcClientSrc, mcClientDst);
        }

    emit finished(true, QString());
    m_running = false;
}
void ModLoaderInstaller::fabricStep1_downloadProfile()'''
    c = c[:idx] + new + c[idx + len(old):]
    print('Added JAR copy')
else:
    print('Pattern not found')
    # Debug: find the emit in finalizeBootstrapperInstall
    idx2 = c.find('emit finished(true, QString());\n    m_running = false;\n}\nvoid ModLoaderInstaller::fabric')
    if idx2 >= 0:
        print(f'Found at {idx2}')
        print(repr(c[idx2-50:idx2+100]))

with open('src/core/mod_loader_installer.cpp', 'w', encoding='utf-8') as f:
    f.write(c)
