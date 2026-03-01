#include "link_forge.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    link_forge window;
    window.show();
    return app.exec();
}
