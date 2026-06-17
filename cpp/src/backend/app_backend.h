#pragma once
#include <QObject>
namespace ShadowLauncher { class AppBackend : public QObject { Q_OBJECT public: explicit AppBackend(QObject* p=nullptr); }; }
