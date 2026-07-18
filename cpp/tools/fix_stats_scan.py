import pathlib

p = pathlib.Path(r'D:/latest-code/cpp/src/backend/stats_backend.cpp')
c = p.read_text('utf-8')

# Find and replace the scanSaveDir function body
old_marker = 'QDir statsDir(savesDir + QStringLiteral("/") + world + QStringLiteral("/stats"));'
new_block = '''        // Stats dir moved from saves/<world>/stats/ to saves/<world>/players/stats/ in MC 1.13+
        QStringList statDirs;
        statDirs << (savesDir + QStringLiteral("/") + world + QStringLiteral("/stats"));
        statDirs << (savesDir + QStringLiteral("/") + world + QStringLiteral("/players/stats"));

        for (const QString& statsPath : statDirs) {
            QDir statsDir(statsPath);'''

c = c.replace(old_marker, new_block)

# Fix the closing: add one more } before the return
# Find the pattern: after stats_dir.exists()) continue; ... } } return
# We added a for loop, so we need an extra closing brace
old_closing = '''            if (ticks > 0) {
                totalHours += ticks / 72000.0;
            }
        }
    }

    return totalHours;'''

new_closing = '''            // MC renamed play_one_minute to play_time in 1.13+
                qint64 ticks = (qint64)custom.value(QStringLiteral("minecraft:play_time")).toDouble();
                if (ticks <= 0)
                    ticks = (qint64)custom.value(QStringLiteral("minecraft:play_one_minute")).toDouble();

                if (ticks > 0)
                    totalHours += ticks / 72000.0;
            }
        }
    }

    return totalHours;'''

c = c.replace(old_closing, new_closing)

p.write_text(c, 'utf-8')
print('OK')
