#include "../src/MainWindow.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <memory>

// MainWindow drives the `minipro` CLI found on PATH. These tests put tests/fake_minipro.sh on PATH
// (as "minipro") so they are deterministic and need neither the real CLI nor a programmer.
// FAKE_MINIPRO_SOURCE is defined by tests/CMakeLists.txt.

// MainWindow's widgets live in a separate top-level QWidget (`window`) that it never deletes.
struct MainWindowDeleter {
  void operator()(MainWindow *mw) const {
    delete mw->window;
    delete mw;
  }
};

class MainWindowTest : public QObject {
 Q_OBJECT

 private slots:
  void initTestCase();
  void init();
  void cleanup();

  // Startup / discovery
  void detectsMinipro();
  void missingMiniproDisablesUi();
  void detectsProgrammer();
  void missingProgrammerDisablesUi();
  void deviceListIsSortedAndDeduplicated();
  void selectDeviceFillsDeviceInfo();
  void selectDeviceIgnoresEmptyName();

  // Command construction
  void parseCheckboxesNoneChecked();
  void parseCheckboxesEachFlag_data();
  void parseCheckboxesEachFlag();
  void parseCheckboxesAllChecked();
  void runCommandPassesDeviceAndFlags();
  void runProcessEchoesInputAndStripsEscapes();

  // Reading a device into the hex view
  void readDeviceFillsHexView();
  void readDevicePassesCheckboxFlags();
  void failedReadRestoresPlaceholder();
  void readWithNoDataShowsEmptyTable();

 private:
  using WindowPtr = std::unique_ptr<MainWindow, MainWindowDeleter>;
  WindowPtr makeWindow();
  QStringList loggedCalls() const;

  QTemporaryDir tempDir;
  QString fakeBinDir;
  QString emptyBinDir;
  QString logPath;
  QByteArray originalPath;
};

void MainWindowTest::initTestCase() {
  QVERIFY(tempDir.isValid());
  QDir root(tempDir.path());
  QVERIFY(root.mkpath("bin"));
  QVERIFY(root.mkpath("empty"));
  QVERIFY(root.mkpath("work"));
  fakeBinDir = root.filePath("bin");
  emptyBinDir = root.filePath("empty");
  logPath = root.filePath("calls.log");

  const QString fake = fakeBinDir + "/minipro";
  QVERIFY2(QFile::copy(FAKE_MINIPRO_SOURCE, fake), FAKE_MINIPRO_SOURCE);
  QVERIFY(QFile::setPermissions(fake, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

  originalPath = qgetenv("PATH");
  // MainWindow reads/writes temp.bin relative to the working directory.
  QVERIFY(QDir::setCurrent(root.filePath("work")));
}

void MainWindowTest::init() {
  qputenv("PATH", fakeBinDir.toLocal8Bit() + ":" + originalPath);
  qputenv("FAKE_MINIPRO_LOG", logPath.toLocal8Bit());
  qunsetenv("FAKE_MINIPRO_NO_PROGRAMMER");
  qunsetenv("FAKE_MINIPRO_READ_HEX");
  qunsetenv("FAKE_MINIPRO_READ_EXIT");
  QFile::remove(logPath);
  QFile::remove("temp.bin");
}

void MainWindowTest::cleanup() {
  qputenv("PATH", originalPath);
}

MainWindowTest::WindowPtr MainWindowTest::makeWindow() {
  return WindowPtr(new MainWindow());
}

QStringList MainWindowTest::loggedCalls() const {
  QFile log(logPath);
  if (!log.open(QFile::ReadOnly | QFile::Text)) return {};
  return QString::fromLocal8Bit(log.readAll()).split('\n', Qt::SkipEmptyParts);
}

void MainWindowTest::detectsMinipro() {
  auto mw = makeWindow();
  QVERIFY(mw->minipro_found);
  QCOMPARE(mw->window->windowTitle(),
           QString("minipro version 0.7.4     A free and open TL866 series programmer"));
  QVERIFY(loggedCalls().contains("--version"));
}

void MainWindowTest::missingMiniproDisablesUi() {
  qputenv("PATH", emptyBinDir.toLocal8Bit());
  auto mw = makeWindow();
  QVERIFY(!mw->minipro_found);
  QVERIFY(!mw->programmer_found);
  QCOMPARE(mw->window->windowTitle(), QString("minipro CLI not found!"));
  QVERIFY(!mw->button_read->isEnabled());
  QVERIFY(!mw->button_write->isEnabled());
  QVERIFY(!mw->button_run_command->isEnabled());
  QVERIFY(!mw->button_update->isEnabled());
  QVERIFY(!mw->combobox_device->isEnabled());
  QCOMPARE(mw->combobox_device->count(), 0);
}

void MainWindowTest::detectsProgrammer() {
  auto mw = makeWindow();
  QVERIFY(mw->programmer_found);
  QCOMPARE(mw->programmer, QString("TL866II+"));
  QCOMPARE(mw->combobox_programmer->itemText(0), QString("TL866II+"));
  QVERIFY(mw->button_read->isEnabled());
  QVERIFY(mw->button_write->isEnabled());
  QVERIFY(mw->button_run_command->isEnabled());
  QVERIFY(mw->button_update->isEnabled());
  QVERIFY(mw->combobox_device->isEnabled());
}

void MainWindowTest::missingProgrammerDisablesUi() {
  qputenv("FAKE_MINIPRO_NO_PROGRAMMER", "1");
  auto mw = makeWindow();
  QVERIFY(mw->minipro_found);
  QVERIFY(!mw->programmer_found);
  QCOMPARE(mw->combobox_programmer->placeholderText(), QString("No programmer found"));
  QVERIFY(!mw->button_read->isEnabled());
  QVERIFY(!mw->combobox_device->isEnabled());
  QCOMPARE(mw->combobox_device->count(), 0);
  QVERIFY(!loggedCalls().contains("-l"));
}

void MainWindowTest::deviceListIsSortedAndDeduplicated() {
  auto mw = makeWindow();
  const QStringList expected{"27C256", "AT28C256", "W27C512"};
  QCOMPARE(mw->devices_list, expected);
  QCOMPARE(mw->combobox_device->count(), expected.size());
  for (int i = 0; i < expected.size(); i++) {
    QCOMPARE(mw->combobox_device->itemText(i), expected.at(i));
  }
}

void MainWindowTest::selectDeviceFillsDeviceInfo() {
  auto mw = makeWindow();
  // Populating the combobox auto-selects the first device.
  QCOMPARE(mw->device, QString("27C256"));
  QCOMPARE(mw->device_name->text(), QString("27C256"));

  mw->combobox_device->setCurrentText("AT28C256");
  QCOMPARE(mw->device, QString("AT28C256"));
  QCOMPARE(mw->device_name->text(), QString("AT28C256"));
  QCOMPARE(mw->device_memory->text(), QString("32768 Bytes"));
  QCOMPARE(mw->device_package->text(), QString("DIP28"));
  QCOMPARE(mw->device_protocol->text(), QString("0x05"));
  QCOMPARE(mw->device_readbuffer->text(), QString("1024 Bytes"));
  QCOMPARE(mw->device_writebuffer->text(), QString("128 Bytes"));
  QVERIFY(loggedCalls().contains("-d AT28C256"));
}

void MainWindowTest::selectDeviceIgnoresEmptyName() {
  auto mw = makeWindow();
  const QString before = mw->device;
  const auto callsBefore = loggedCalls().size();
  mw->select_device("");
  QCOMPARE(mw->device, before);
  QCOMPARE(loggedCalls().size(), callsBefore);
}

void MainWindowTest::parseCheckboxesNoneChecked() {
  auto mw = makeWindow();
  QVERIFY(mw->parse_checkboxes().isEmpty());
}

void MainWindowTest::parseCheckboxesEachFlag_data() {
  QTest::addColumn<QString>("checkbox");
  QTest::addColumn<QString>("flag");
  QTest::newRow("no_id_error") << "no_id_error" << "--no_id_error";
  QTest::newRow("skip_id") << "skip_id" << "--skip_id";
  QTest::newRow("no_size_error") << "no_size_error" << "--no_size_error";
  QTest::newRow("skip_verify") << "skip_verify" << "--skip_verify";
  QTest::newRow("pin_check") << "pin_check" << "--pin_check";
  QTest::newRow("blank_check") << "blank_check" << "--blank_check";
  QTest::newRow("erase_device") << "erase_device" << "--erase";
  QTest::newRow("hardware_check") << "hardware_check" << "--hardware_check";
}

void MainWindowTest::parseCheckboxesEachFlag() {
  QFETCH(QString, checkbox);
  QFETCH(QString, flag);
  auto mw = makeWindow();
  const QMap<QString, QCheckBox *> boxes{
      {"no_id_error", mw->no_id_error}, {"skip_id", mw->skip_id},
      {"no_size_error", mw->no_size_error}, {"skip_verify", mw->skip_verify},
      {"pin_check", mw->pin_check}, {"blank_check", mw->blank_check},
      {"erase_device", mw->erase_device}, {"hardware_check", mw->hardware_check}};
  QVERIFY(boxes.contains(checkbox));
  boxes.value(checkbox)->setChecked(true);
  QCOMPARE(mw->parse_checkboxes(), QStringList{flag});
}

void MainWindowTest::parseCheckboxesAllChecked() {
  auto mw = makeWindow();
  for (auto *box : {mw->no_id_error, mw->skip_id, mw->no_size_error, mw->skip_verify,
                    mw->pin_check, mw->blank_check, mw->erase_device, mw->hardware_check}) {
    box->setChecked(true);
  }
  const QStringList expected{"--no_id_error", "--skip_id", "--no_size_error", "--skip_verify",
                             "--pin_check", "--blank_check", "--erase", "--hardware_check"};
  QCOMPARE(mw->parse_checkboxes(), expected);
}

void MainWindowTest::runCommandPassesDeviceAndFlags() {
  auto mw = makeWindow();
  mw->combobox_device->setCurrentText("AT28C256");
  mw->erase_device->setChecked(true);
  mw->blank_check->setChecked(true);
  mw->run_command();
  QCOMPARE(loggedCalls().last(), QString("-p AT28C256 --blank_check --erase"));
}

void MainWindowTest::runProcessEchoesInputAndStripsEscapes() {
  auto mw = makeWindow();
  QPlainTextEdit target;
  const QString output = MainWindow::run_process(target, {"--version"}, "stderr");
  QVERIFY(output.contains("minipro version 0.7.4"));
  QVERIFY(target.toPlainText().contains("[Input]: minipro --version"));
  QVERIFY(target.toPlainText().contains("[Output]: minipro version"));
  QVERIFY(!target.toPlainText().contains("\u001B[K"));
}

void MainWindowTest::readDeviceFillsHexView() {
  // 32 bytes: "Hello, minipro!!" followed by 00..0F
  QByteArray bytes("Hello, minipro!!");
  for (int i = 0; i < 16; i++) bytes.append(static_cast<char>(i));
  qputenv("FAKE_MINIPRO_READ_HEX", bytes.toHex());

  auto mw = makeWindow();
  mw->combobox_device->setCurrentText("AT28C256");
  mw->read_device();

  HexView &model = mw->hexViewModel;
  QTRY_COMPARE(model.rowCount(), 2);
  QCOMPARE(model.index(0, 0).data().toString(), QString("48"));
  QCOMPARE(model.index(0, 16).data().toString(), QString("Hello, minipro!!"));
  QCOMPARE(model.index(1, 15).data().toString(), QString("0F"));
  QCOMPARE(model.index(1, 16).data().toString(), QString("................"));
  QCOMPARE(model.headerData(1, Qt::Vertical, Qt::DisplayRole).toString(), QString("000010"));
  QVERIFY(loggedCalls().contains("-p AT28C256 -r temp.bin"));
  QVERIFY(mw->status_view->toPlainText().contains("[Input]: minipro -p AT28C256 -r temp.bin"));
}

void MainWindowTest::readDevicePassesCheckboxFlags() {
  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("00").repeated(16));
  auto mw = makeWindow();
  mw->combobox_device->setCurrentText("W27C512");
  mw->skip_verify->setChecked(true);
  mw->read_device();
  QTRY_COMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("00"));
  QVERIFY(loggedCalls().contains("-p W27C512 -r temp.bin --skip_verify"));
}

void MainWindowTest::failedReadRestoresPlaceholder() {
  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("ff").repeated(16));
  auto mw = makeWindow();
  mw->read_device();
  QTRY_COMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("FF"));

  qputenv("FAKE_MINIPRO_READ_EXIT", "1");
  mw->read_device();
  QTRY_COMPARE(mw->hexViewModel.rowCount(), 8);
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
}

void MainWindowTest::readWithNoDataShowsEmptyTable() {
  // Fake read succeeds but writes no temp.bin, so the hex view receives no bytes.
  // Regression: this used to crash in HexView::columnCount().
  auto mw = makeWindow();
  QCOMPARE(mw->hexViewModel.rowCount(), 8);
  mw->read_device();
  QTRY_COMPARE(mw->hexViewModel.rowCount(), 0);
  QCOMPARE(mw->hexViewModel.columnCount(), 17);
  QCOMPARE(mw->hexTableView->model()->columnCount(), 17);
}

QTEST_MAIN(MainWindowTest)
#include "MainWindowTest.moc"
