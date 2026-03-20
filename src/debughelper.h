#ifndef DEBUG_H
#define DEBUG_H

#include <QDebug>

//  this is soooooo ugly...
#define dbg() qDebug() << __FILE__ ":" << __LINE__

#endif // DEBUG_H
