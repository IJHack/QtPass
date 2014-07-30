#include "mainwindow.h"
#include <QApplication>
#include <QFileSystemModel>
#include <QTreeView>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    QFileSystemModel model;
    model.setRootPath(QDir::homePath());
    QTreeView tree;
    tree.setModel(&model);

    // Demonstrating look and feel features
    tree.setAnimated(false);
    tree.setIndentation(20);
    tree.setSortingEnabled(true);

    tree.setWindowTitle(QObject::tr("Dir View"));
    tree.resize(640, 480);
    tree.show();

    return a.exec();
}
