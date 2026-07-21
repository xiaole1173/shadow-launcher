#include <QApplication>
#include <QQuickWindow>
#include <QDebug>
#include <cstdio>
#include <windows.h>

int main(int argc, char *argv[])
{
    OutputDebugStringA("MINIMAL TEST START\n");
    fprintf(stderr, "START\n");
    
    QApplication app(argc, argv);
    OutputDebugStringA("QAPP CREATED\n");
    fprintf(stderr, "QAPP OK\n");
    
    QQuickWindow window;
    window.setTitle("Test");
    window.resize(400, 300);
    window.show();
    fprintf(stderr, "WINDOW SHOWN\n");
    OutputDebugStringA("WINDOW SHOWN\n");
    
    int ret = app.exec();
    fprintf(stderr, "EXIT: %d\n", ret);
    return ret;
}
