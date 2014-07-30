#include "mainwindow.h"
#include <QApplication>
#include <QFileSystemModel>
#include <QTreeView>
#include <QDirModel>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

        QDirModel model;
        QTreeView tree;

        tree.setModel(&model);

        tree.setRootIndex(model.index(QDir::homePath()));
        tree.setColumnHidden( 1, true );
        tree.setColumnHidden( 2, true );
        tree.setColumnHidden( 3, true );

        tree.setWindowTitle(QObject::tr("Dir View:")+QDir::homePath());
        tree.resize(640, 480);
        tree.show();

        return app.exec();
}
