// Renders the README screenshot (res/screenshot.png) from the real UI.
// Usage: screenshot <output.png>     (run by tests/update_screenshot.sh)
//
// It runs the fake minipro (FAKE_MINIPRO_SOURCE, built from fake_minipro.cpp), never the real
// one: a programmer may be connected. It drives the window like a user would: it picks a device,
// clicks "Read from Device", waits for the hex view to fill and saves the window. It runs on the
// native platform (not offscreen) so the screenshot shows the system's own look.

#include "MainWindow.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTableView>
#include <QTemporaryDir>
#include <QThread>
#include <functional>
#include <iostream>

namespace {

// The bytes the fake "reads": a line of text, then erased (0xFF) bytes, 8 rows in all.
QByteArray sampleImage() {
  QByteArray image("minipro-gui: a GUI for the minipro CLI, for TL866II+, T48, T56 and T76 programmers.");
  image.append(QByteArray(128 - image.size(), '\xFF'));
  return image;
}

bool waitFor(const std::function<bool()> &done, int timeout_ms) {
  QElapsedTimer timer;
  timer.start();
  while (!done()) {
    if (timer.elapsed() > timeout_ms) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QThread::msleep(20);
  }
  return true;
}

int fail(const char *message) {
  std::cerr << "screenshot: " << message << std::endl;
  return 1;
}

}  // namespace

int main(int argc, char *argv[]) {
  // The read goes to <temp>/minipro-gui-XXXXXX/read.bin, which the Output pane shows. A short,
  // fixed temp folder and app name keep that line the same on every run (see below).
  qputenv("TMPDIR", "/tmp");
  QApplication application(argc, argv);
  QCoreApplication::setApplicationName("minipro-gui");
  if (argc != 2) return fail("usage: screenshot <output.png>");
  const QString output = QDir::current().absoluteFilePath(QString::fromLocal8Bit(argv[1]));

  // The fake minipro, first on PATH, in its own folder.
  QTemporaryDir bin;
  QTemporaryDir work;
  if (!bin.isValid() || !work.isValid()) return fail("could not create temporary folders");
#ifdef Q_OS_WIN
  const QString fake = bin.filePath("minipro.exe");
#else
  const QString fake = bin.filePath("minipro");
#endif
  if (!QFile::copy(FAKE_MINIPRO_SOURCE, fake) ||
      !QFile::setPermissions(fake, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
    return fail("could not copy the fake minipro");
  }
  qputenv("PATH", QDir::toNativeSeparators(bin.path()).toLocal8Bit() + QDir::listSeparator().toLatin1() +
                      qgetenv("PATH"));
  // Refuse to go on unless the app will find the fake: it would otherwise run the real minipro.
  if (QFileInfo(QStandardPaths::findExecutable("minipro")) != QFileInfo(fake)) {
    return fail("the fake minipro isn't the first one on PATH; refusing to run the real one");
  }
  qputenv("FAKE_MINIPRO_READ_HEX", sampleImage().toHex());
  QDir::setCurrent(work.path());

  MainWindow main_window;  // shows its window

  QWidget *window = nullptr;
  for (QWidget *widget : QApplication::topLevelWidgets()) {
    if (widget->isVisible() && widget != &main_window) window = widget;
  }
  if (window == nullptr) return fail("the app window didn't open");

  QComboBox *device = nullptr;
  for (QComboBox *box : window->findChildren<QComboBox *>()) {
    if (box->findText("AT28C256") >= 0) device = box;
  }
  QPushButton *read = nullptr;
  for (QPushButton *button : window->findChildren<QPushButton *>()) {
    if (button->text() == "Read from Device") read = button;
  }
  auto *hex_view = window->findChild<QTableView *>();
  if (device == nullptr || read == nullptr || hex_view == nullptr) {
    return fail("the device list, the Read button or the hex view is missing");
  }

  // Wide enough for the hex view's ASCII column without a scrollbar.
  window->resize(1100, 660);
  device->setCurrentText("AT28C256");
  if (!waitFor([read] { return read->isEnabled(); }, 5000)) return fail("Read never became enabled");
  read->click();
  const bool loaded = waitFor(
      [hex_view, read] {
        return read->isEnabled() && hex_view->model()->index(0, 0).data().toString() == "6D";
      },
      10000);
  if (!loaded) return fail("the read didn't fill the hex view");
  // QTemporaryDir's random suffix would change the image on every run; show the pattern instead.
  if (auto *output_pane = window->findChild<QPlainTextEdit *>(); output_pane != nullptr) {
    static const QRegularExpression random_suffix("(minipro-gui-)[A-Za-z0-9]{6}");
    output_pane->setPlainText(output_pane->toPlainText().replace(random_suffix, "\\1XXXXXX"));
    output_pane->moveCursor(QTextCursor::End);  // back to the read, as the app leaves it
    output_pane->ensureCursorVisible();
  }
  waitFor([] { return false; }, 300);  // let the last repaint land

  if (!window->grab().save(output)) return fail("could not save the screenshot");
  std::cout << "screenshot: saved " << output.toStdString() << std::endl;
  return 0;
}
