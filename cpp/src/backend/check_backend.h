#pragma once
#include <QObject>
namespace ShadowLauncher {
class CheckBackend : public QObject {
    Q_OBJECT
public:
    explicit CheckBackend(QObject* p = nullptr);
};
}
