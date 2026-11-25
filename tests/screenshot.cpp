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
#include <QScrollBar>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>

namespace {

// The start of what the fake "reads": a line of text. FAKE_MINIPRO_READ_SIZE pads it with erased
// (0xFF) bytes to the AT28C256's 32 KiB, so the hex view has a real chip's 2048 rows and its
// vertical scrollbar, as after a real read.
QByteArray sampleText() {
  return "minipro-gui: a GUI for the minipro CLI, for TL866II+, T48, T56 and T76 programmers.";
}
constexpr int kChipSize = 32768;

// The README image is 2x (Retina). grab() renders at the screen's own ratio, so another one would
// give a different PNG; the tool refuses rather than scaling. (Rendering into a 2x QPixmap of our
// own instead draws macOS text fields with white frames.)
constexpr qreal kPixelRatio = 2.0;

template <typename Predicate>
bool waitFor(Predicate done, int timeout_ms) {
  QElapsedTimer timer;
  timer.start();
  while (!done()) {
    if (timer.elapsed() > timeout_ms) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QThread::msleep(20);
  }
  return true;
}

// Delivers everything pending (layout, scrolling, repaints) before the window is rendered.
void settle() {
  for (int pass = 0; pass < 5; pass++) {
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
  }
}

int fail(const char *message) {
  std::cerr << "screenshot: " << message << std::endl;
  return 1;
}

int fail(const QString &message) { return fail(message.toLocal8Bit().constData()); }

// The app's folder in its temp dir is the working directory while it runs; this puts the original
// back before that folder is removed (Windows can't remove a working directory).
struct RestoreCurrentDir {
  QString path = QDir::currentPath();
  RestoreCurrentDir() = default;
  RestoreCurrentDir(const RestoreCurrentDir &) = delete;
  RestoreCurrentDir &operator=(const RestoreCurrentDir &) = delete;
  RestoreCurrentDir(RestoreCurrentDir &&) = delete;
  RestoreCurrentDir &operator=(RestoreCurrentDir &&) = delete;
  ~RestoreCurrentDir() { QDir::setCurrent(path); }
};

// Copies the fake minipro into `bin` and puts it first on PATH. Returns an error, or empty.
QString installFakeMinipro(const QTemporaryDir &bin) {
#ifdef Q_OS_WIN
  const QString fake = bin.filePath("minipro.exe");
#else
  const QString fake = bin.filePath("minipro");
#endif
  if (!QFile::copy(FAKE_MINIPRO_SOURCE, fake) ||
      !QFile::setPermissions(fake, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
    return "could not copy the fake minipro";
  }
  qputenv("PATH", QDir::toNativeSeparators(bin.path()).toLocal8Bit() + QDir::listSeparator().toLatin1() +
                      qgetenv("PATH"));
  // Refuse to go on unless the app will find the fake: it would otherwise run the real minipro.
  if (QFileInfo(QStandardPaths::findExecutable("minipro")) != QFileInfo(fake)) {
    return "the fake minipro isn't the first one on PATH; refusing to run the real one";
  }
  return {};
}

// The app's own window (MainWindow itself is never shown).
QWidget *findAppWindow(const QWidget *main_window) {
  QWidget *window = nullptr;
  for (QWidget *widget : QApplication::topLevelWidgets()) {
    if (widget->isVisible() && widget != main_window) window = widget;
  }
  return window;
}

// At the app's default size, a whole hex row (bytes and ASCII) must fit. Checked here because this
// runs on the native platform; the tests' offscreen fonts are narrower and can't show it. macOS's
// overlay scrollbars take no room, but with "Show scroll bars: Always" (and on other systems) the
// vertical one does, so leave room for it either way.
struct RowFit {
  int needed = 0;
  int available = 0;
  bool fits = false;
};

RowFit measureHexRow(const QTableView *hex_view) {
  RowFit fit;
  // A full row's ASCII text, measured as the view's delegate draws it.
  QStyleOptionViewItem ascii_option;
  ascii_option.initFrom(hex_view);
  fit.needed = hex_view->itemDelegate()->sizeHint(ascii_option, hex_view->model()->index(0, 16)).width();
  for (int column = 0; column < 16; column++) fit.needed += hex_view->columnWidth(column);
  fit.available = hex_view->viewport()->width();
  if (hex_view->style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, hex_view) != 0) {
    fit.available -= hex_view->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, hex_view);
  }
  fit.fits = !hex_view->horizontalScrollBar()->isVisible() && fit.needed <= fit.available;
  return fit;
}

// The temp folder differs per machine and QTemporaryDir's suffix per run, and the folder can name
// the user: show them as $TMPDIR and XXXXXX, so the image is the same everywhere and every time.
void maskReadFolder(QPlainTextEdit *output_pane) {
  static const QRegularExpression read_folder(R"((?:/[^\s/]+)+/minipro-gui-[A-Za-z0-9]{6}/)");
  output_pane->setPlainText(output_pane->toPlainText().replace(read_folder, "$TMPDIR/minipro-gui-XXXXXX/"));
  output_pane->moveCursor(QTextCursor::End);  // back to the read, as the app leaves it
  output_pane->ensureCursorVisible();
}

}  // namespace

int main(int argc, char *argv[]) {
  QApplication application(argc, argv);
  // The read goes to <temp>/minipro-gui-<random>/read.bin, which the Output pane shows (see below).
  QCoreApplication::setApplicationName("minipro-gui");
  if (argc != 2) return fail("usage: screenshot <output.png>");
  const QString output = QDir::current().absoluteFilePath(QString::fromLocal8Bit(argv[1]));

  // The fake minipro, first on PATH, in its own folder.
  QTemporaryDir bin;
  QTemporaryDir work;
  if (!bin.isValid() || !work.isValid()) return fail("could not create temporary folders");
  if (const QString error = installFakeMinipro(bin); !error.isEmpty()) return fail(error);
  qputenv("FAKE_MINIPRO_READ_HEX", sampleText().toHex());
  qputenv("FAKE_MINIPRO_READ_SIZE", QByteArray::number(kChipSize));
  const RestoreCurrentDir restore_current_dir;  // declared after `work`, so it runs first
  QDir::setCurrent(work.path());

  MainWindow main_window;  // shows its window

  QWidget *window = findAppWindow(&main_window);
  if (window == nullptr) return fail("the app window didn't open");
  // An inactive window is drawn in other colors, so a run that left the terminal in front would
  // give a different image. Bring the window to the front, and refuse to save one that isn't.
  window->raise();
  window->activateWindow();
  if (!waitFor([window] { return window->isActiveWindow(); }, 3000)) {
    return fail("the window didn't become the active one; run it again without switching away");
  }

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

  device->setCurrentText("AT28C256");
  if (!waitFor([read] { return read->isEnabled(); }, 5000)) return fail("Read never became enabled");
  read->click();
  const bool loaded = waitFor(
      [hex_view, read] {
        return read->isEnabled() && hex_view->model()->index(0, 0).data().toString() == "6D";
      },
      10000);
  if (!loaded) return fail("the read didn't fill the hex view");
  settle();
  if (hex_view->model()->rowCount() != kChipSize / 16) return fail("the hex view doesn't show the whole chip");
  if (const RowFit fit = measureHexRow(hex_view); !fit.fits) {
    return fail(QString("at the default window size a hex row doesn't fit: it needs %1 px, the view "
                        "has %2 px with room for a vertical scrollbar")
                    .arg(fit.needed)
                    .arg(fit.available));
  }
  if (auto *output_pane = window->findChild<QPlainTextEdit *>(); output_pane != nullptr) {
    maskReadFolder(output_pane);
  }
  settle();

  const QPixmap image = window->grab();
  if (image.devicePixelRatio() != kPixelRatio) {
    return fail(QString("the window is on a %1x display; move it to a Retina (2x) one and run again")
                    .arg(image.devicePixelRatio()));
  }
  if (!image.save(output)) return fail("could not save the screenshot");
  std::cout << "screenshot: saved " << output.toStdString() << std::endl;
  return 0;
}
