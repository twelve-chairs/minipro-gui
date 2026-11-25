#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
  QApplication application(argc, argv);
  QGuiApplication::setWindowIcon(QIcon(":/res/AppIcon.png"));
  QCoreApplication::setOrganizationName("Twelve Chairs");
  QCoreApplication::setOrganizationDomain("twelve-chairs");
  QCoreApplication::setApplicationName("minipro-gui");
  // The Wayland app ID: panels use it to find minipro-gui.desktop and its icon (see CMakeLists).
  QGuiApplication::setDesktopFileName("minipro-gui");

  MainWindow window;

  return QApplication::exec();
}