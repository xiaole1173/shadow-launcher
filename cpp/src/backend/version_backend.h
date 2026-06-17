#pragma once
#include <QObject>
namespace ShadowLauncher { class VersionBackend : public QObject { Q_OBJECT public: explicit VersionBackend(QObject* p=nullptr); }; }
