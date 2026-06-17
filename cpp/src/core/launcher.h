#pragma once
#include <QObject>
namespace ShadowLauncher { class Launcher : public QObject { Q_OBJECT public: explicit Launcher(QObject* p=nullptr); }; }
