#pragma once
#include <QObject>
namespace ShadowLauncher { class ResourceBackend : public QObject { Q_OBJECT public: explicit ResourceBackend(QObject* p=nullptr); }; }
