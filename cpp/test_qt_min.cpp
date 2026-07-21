#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <cstdio>

int main(int argc, char *argv[])
{
    fprintf(stderr, "Hello from test\n");
    QCoreApplication app(argc, argv);
    fprintf(stderr, "QApp created\n");
    qDebug() << "Qt debug output works";
    fprintf(stderr, "Done\n");
    return 0;
}
