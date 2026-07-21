#include <QApplication>
#include <QQmlApplicationEngine>
#include <QDebug>
#include <QElapsedTimer>
#include <cstdio>

int main(int argc, char *argv[])
{
    fprintf(stderr, "Starting...\n");
    QElapsedTimer timer;
    timer.start();
    
    QApplication app(argc, argv);
    fprintf(stderr, "QApp created in %lldms\n", timer.elapsed());
    
    QQmlApplicationEngine engine;
    fprintf(stderr, "Engine created in %lldms\n", timer.elapsed());
    
    // Load a minimal QML
    engine.loadFromModule("ShadowLauncher", "MainWindow");
    fprintf(stderr, "Load completed in %lldms\n", timer.elapsed());
    
    if (engine.rootObjects().isEmpty()) {
        fprintf(stderr, "No root objects!\n");
        return -1;
    }
    
    fprintf(stderr, "Root objects: %d\n", engine.rootObjects().size());
    fprintf(stderr, "Entering event loop...\n");
    return app.exec();
}
