#include "DisplayManager.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[]) {
  QCoreApplication::setOrganizationName("BlackledgeBuilds");
  QCoreApplication::setApplicationName("Display Manager");

  QApplication a(argc, argv);
  DisplayManager w;
  w.show();
  return a.exec();
}
