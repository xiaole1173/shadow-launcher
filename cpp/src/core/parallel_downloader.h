#pragma once
#include <QObject>
namespace ShadowLauncher { class ParallelDownloader : public QObject { Q_OBJECT public: explicit ParallelDownloader(QObject* p=nullptr); }; }
