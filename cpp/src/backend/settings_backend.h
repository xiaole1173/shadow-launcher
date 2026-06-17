#pragma once
#include <QObject>
namespace ShadowLauncher { class SettingsBackend : public QObject { Q_OBJECT public: explicit SettingsBackend(QObject* p=nullptr); }; }
