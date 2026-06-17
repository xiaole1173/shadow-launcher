#pragma once
#include <QObject>
namespace ShadowLauncher { class LaunchBackend : public QObject { Q_OBJECT public: explicit LaunchBackend(QObject* p=nullptr); }; }
