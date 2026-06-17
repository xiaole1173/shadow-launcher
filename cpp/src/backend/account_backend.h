#pragma once
#include <QObject>
namespace ShadowLauncher { class AccountBackend : public QObject { Q_OBJECT public: explicit AccountBackend(QObject* p=nullptr); }; }
