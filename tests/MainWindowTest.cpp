#include "../src/MainWindow.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QElapsedTimer>
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
  void presenceCheckParsing_data();
  void presenceCheckParsing();
  void presenceCheckDoesNotDuplicateProgrammer();
  void queryPromptDoesNotHang();
  void slowQueryIsStopped();
  void startFailureIsReportedNotParsed();
  void runCommandDoesNotBlockAndKeepsOrResetsView();
  void deviceListIsSortedAndDeduplicated();
  void selectDeviceFillsDeviceInfo();
  void selectDeviceIgnoresEmptyName();
  void failedDeviceInfoClearsPreviousDevice();
  void windowsLineEndingsAreStripped();

  // Command construction
  void parseCheckboxesNoneChecked();
  void parseCheckboxesEachFlag_data();
  void parseCheckboxesEachFlag();
  void parseCheckboxesAllChecked();
  void runCommandRunsOneActionWithOptions();
  void runCommandRefusesZeroOrSeveralActions_data();
  void runCommandRefusesZeroOrSeveralActions();
  void writePassesOnlyOptionFlags();
  void updateFirmwarePassesNoCheckboxFlags();
  void runProcessEchoesInputAndStripsEscapes();

  // Reading a device into the hex view
  void readDeviceFillsHexView();
  void readDevicePassesOnlyOptionFlags();
  void failedReadRestoresPlaceholder();
  void readWithNoDataShowsEmptyTable();
  void failedReadNeverShowsPreviousData_data();
  void failedReadNeverShowsPreviousData();
  void readThatWritesNoFileDoesNotShowPreviousRead();
  void readWithNoFileReportsError();
  void largeImageLoadsQuickly();
  void writeDoesNotLoadPreviousRead();
  void readReportsMiniproFailingToStart();

  // One device operation at a time
  void controlsDisabledWhileOperationRuns();
  void secondOperationIsRefused();
  void controlsReenabledAfterFailedStart();
  void finishedProcessesAreDeleted();
  void closingWindowLetsRunningWriteFinish();
  void firmwarePromptDoesNotHang();
  void firmwareUpdateWhenConfirmed();
  void firmwareUpdateWhenDeclined();
  void closingWindowAtFirmwarePromptAborts();

 private:
  using WindowPtr = std::unique_ptr<MainWindow, MainWindowDeleter>;
  WindowPtr makeWindow();
  QStringList loggedCalls() const;
  static void readFfRow(MainWindow &mw);

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
  qunsetenv("FAKE_MINIPRO_PRESENCE");
  qunsetenv("FAKE_MINIPRO_INFO_PROMPT");
  qunsetenv("FAKE_MINIPRO_INFO_SLEEP");
  qunsetenv("FAKE_MINIPRO_INFO_FAIL");
  qunsetenv("FAKE_MINIPRO_CRLF");
  MainWindow::sync_timeout_ms = 10000;
  qunsetenv("FAKE_MINIPRO_READ_HEX");
  qunsetenv("FAKE_MINIPRO_READ_EXIT");
  qunsetenv("FAKE_MINIPRO_READ_EMPTY");
  qunsetenv("FAKE_MINIPRO_CRASH");
  qunsetenv("FAKE_MINIPRO_DELAY");
  qunsetenv("FAKE_MINIPRO_DONE_LOG");
  QFile::remove(logPath);
  QFile::remove("temp.bin");
}

void MainWindowTest::cleanup() {
  qputenv("PATH", originalPath);
}

MainWindowTest::WindowPtr MainWindowTest::makeWindow() {
  WindowPtr mw(new MainWindow());
  // Never open a real dialog in tests; decline by default (the safe answer).
  mw->confirm_firmware_update = [](const QString &) { return false; };
  return mw;
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

namespace {
void checkAll(std::initializer_list<QCheckBox *> boxes) {
  for (auto *box : boxes) box->setChecked(true);
}
}  // namespace

void MainWindowTest::parseCheckboxesNoneChecked() {
  auto mw = makeWindow();
  QVERIFY(mw->parse_option_checkboxes().isEmpty());
  QVERIFY(mw->parse_action_checkboxes().isEmpty());
}

void MainWindowTest::parseCheckboxesEachFlag_data() {
  QTest::addColumn<QString>("checkbox");
  QTest::addColumn<QString>("flag");
  QTest::addColumn<bool>("isAction");
  QTest::newRow("no_id_error") << "no_id_error" << "--no_id_error" << false;
  QTest::newRow("skip_id") << "skip_id" << "--skip_id" << false;
  QTest::newRow("no_size_error") << "no_size_error" << "--no_size_error" << false;
  QTest::newRow("skip_verify") << "skip_verify" << "--skip_verify" << false;
  QTest::newRow("pin_check") << "pin_check" << "--pin_check" << false;
  QTest::newRow("blank_check") << "blank_check" << "--blank_check" << true;
  QTest::newRow("erase_device") << "erase_device" << "--erase" << true;
  QTest::newRow("hardware_check") << "hardware_check" << "--hardware_check" << true;
}

void MainWindowTest::parseCheckboxesEachFlag() {
  QFETCH(QString, checkbox);
  QFETCH(QString, flag);
  QFETCH(bool, isAction);
  auto mw = makeWindow();
  const QMap<QString, QCheckBox *> boxes{
      {"no_id_error", mw->no_id_error}, {"skip_id", mw->skip_id},
      {"no_size_error", mw->no_size_error}, {"skip_verify", mw->skip_verify},
      {"pin_check", mw->pin_check}, {"blank_check", mw->blank_check},
      {"erase_device", mw->erase_device}, {"hardware_check", mw->hardware_check}};
  QVERIFY(boxes.contains(checkbox));
  boxes.value(checkbox)->setChecked(true);
  const QStringList expected{flag};
  QCOMPARE(mw->parse_action_checkboxes(), isAction ? expected : QStringList{});
  QCOMPARE(mw->parse_option_checkboxes(), isAction ? QStringList{} : expected);
}

void MainWindowTest::parseCheckboxesAllChecked() {
  auto mw = makeWindow();
  checkAll({mw->no_id_error, mw->skip_id, mw->no_size_error, mw->skip_verify, mw->pin_check,
            mw->blank_check, mw->erase_device, mw->hardware_check});
  const QStringList options{"--no_id_error", "--skip_id", "--no_size_error", "--skip_verify",
                            "--pin_check"};
  const QStringList actions{"--blank_check", "--erase", "--hardware_check"};
  QCOMPARE(mw->parse_option_checkboxes(), options);
  QCOMPARE(mw->parse_action_checkboxes(), actions);
}

void MainWindowTest::runCommandRunsOneActionWithOptions() {
  auto mw = makeWindow();
  mw->combobox_device->setCurrentText("AT28C256");
  checkAll({mw->erase_device, mw->pin_check});
  mw->run_command();
  QTRY_VERIFY(mw->async_process == nullptr);
  QCOMPARE(loggedCalls().last(), QString("-p AT28C256 --erase --pin_check"));
}

void MainWindowTest::runCommandRefusesZeroOrSeveralActions_data() {
  QTest::addColumn<QStringList>("checked");
  QTest::newRow("no action") << QStringList{"pin_check"};
  QTest::newRow("blank + erase") << QStringList{"blank_check", "erase_device"};
  QTest::newRow("all three") << QStringList{"blank_check", "erase_device", "hardware_check"};
}

void MainWindowTest::runCommandRefusesZeroOrSeveralActions() {
  // minipro runs a single action; given several it silently picks one.
  QFETCH(QStringList, checked);
  auto mw = makeWindow();
  const QMap<QString, QCheckBox *> boxes{{"pin_check", mw->pin_check},
                                         {"blank_check", mw->blank_check},
                                         {"erase_device", mw->erase_device},
                                         {"hardware_check", mw->hardware_check}};
  for (const auto &name : checked) boxes.value(name)->setChecked(true);
  const auto callsBefore = loggedCalls().size();
  mw->run_command();
  QCOMPARE(loggedCalls().size(), callsBefore);
  QVERIFY(mw->status_view->toPlainText().contains("[Error]: Run Command needs exactly one of"));
}

void MainWindowTest::writePassesOnlyOptionFlags() {
  // Regression: --blank_check replaced -w (nothing written, exit 0) and --hardware_check ran
  // instead of the write.
  auto mw = makeWindow();
  mw->combobox_device->setCurrentText("AT28C256");
  checkAll({mw->blank_check, mw->erase_device, mw->hardware_check, mw->no_size_error});
  mw->write_file_to_device("/images/fw.bin");
  QTRY_VERIFY(loggedCalls().contains("-p AT28C256 -w /images/fw.bin --no_size_error"));
  QTRY_VERIFY(mw->async_process == nullptr);
}

void MainWindowTest::updateFirmwarePassesNoCheckboxFlags() {
  auto mw = makeWindow();
  checkAll({mw->no_id_error, mw->skip_id, mw->no_size_error, mw->skip_verify, mw->pin_check,
            mw->blank_check, mw->erase_device, mw->hardware_check});
  mw->update_firmware_from_file("/images/update.dat");
  QTRY_VERIFY(loggedCalls().contains("-F /images/update.dat"));
  QTRY_VERIFY(mw->async_process == nullptr);
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

void MainWindowTest::readDevicePassesOnlyOptionFlags() {
  // Regression: --blank_check replaced -r, so the "read" showed the previous temp.bin.
  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("00").repeated(16));
  auto mw = makeWindow();
  mw->combobox_device->setCurrentText("W27C512");
  checkAll({mw->skip_verify, mw->blank_check, mw->erase_device, mw->hardware_check});
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
  // read_device() resets the view as it starts, so wait for the result, not just the placeholder.
  QTRY_VERIFY(mw->status_view->toPlainText().contains("[Error]: minipro exited with code 1"));
  QCOMPARE(mw->hexViewModel.rowCount(), 8);
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
}

void MainWindowTest::readWithNoDataShowsEmptyTable() {
  // Fake read succeeds and writes an empty temp.bin, so the hex view receives no bytes.
  // Regression: this used to crash in HexView::columnCount().
  qputenv("FAKE_MINIPRO_READ_EMPTY", "1");
  auto mw = makeWindow();
  QCOMPARE(mw->hexViewModel.rowCount(), 8);
  mw->read_device();
  QTRY_COMPARE(mw->hexViewModel.rowCount(), 0);
  QCOMPARE(mw->hexViewModel.columnCount(), 17);
  QCOMPARE(mw->hexTableView->model()->columnCount(), 17);
}

// Reads 16 bytes of 0xFF so later tests can tell old data apart from the placeholder.
void MainWindowTest::readFfRow(MainWindow &mw) {
  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("ff").repeated(16));
  mw.read_device();
  QTRY_COMPARE(mw.hexViewModel.index(0, 0).data().toString(), QString("FF"));
}

void MainWindowTest::failedReadNeverShowsPreviousData_data() {
  QTest::addColumn<QByteArray>("exitCode");
  QTest::addColumn<bool>("crash");
  QTest::addColumn<QString>("message");
  QTest::newRow("exit 1") << QByteArray("1") << false << "[Error]: minipro exited with code 1";
  QTest::newRow("exit 2") << QByteArray("2") << false << "[Error]: minipro exited with code 2";
  QTest::newRow("crash") << QByteArray() << true << "[Error]: minipro crashed";
}

void MainWindowTest::failedReadNeverShowsPreviousData() {
  // Regression: any exit code other than 1, or a crash, loaded temp.bin from the previous read.
  QFETCH(QByteArray, exitCode);
  QFETCH(bool, crash);
  QFETCH(QString, message);
  auto mw = makeWindow();
  readFfRow(*mw);

  if (crash) qputenv("FAKE_MINIPRO_CRASH", "1");
  else qputenv("FAKE_MINIPRO_READ_EXIT", exitCode);
  mw->read_device();
  QTRY_VERIFY(mw->status_view->toPlainText().contains(message));
  QCOMPARE(mw->hexViewModel.rowCount(), 8);
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
}

void MainWindowTest::readThatWritesNoFileDoesNotShowPreviousRead() {
  // Regression: temp.bin was never removed, so a read that wrote nothing showed the last chip.
  auto mw = makeWindow();
  readFfRow(*mw);
  qunsetenv("FAKE_MINIPRO_READ_HEX");  // the next read succeeds but writes no file
  mw->read_device();
  QTRY_VERIFY(mw->status_view->toPlainText().contains("[Error]: could not open temp.bin"));
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
}

void MainWindowTest::readWithNoFileReportsError() {
  // Regression: QFile::open wasn't checked, so a missing file was shown as an empty chip.
  auto mw = makeWindow();
  mw->read_device();
  QTRY_VERIFY(mw->status_view->toPlainText().contains("[Error]: could not open temp.bin"));
  QCOMPARE(mw->hexViewModel.rowCount(), 8);
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
}

void MainWindowTest::largeImageLoadsQuickly() {
  // Regression: loading a 16 MiB image (a million rows) took over 10 s and more than 1 GB.
  auto mw = makeWindow();
  QFile image("temp.bin");
  QVERIFY(image.open(QFile::WriteOnly));
  image.write(QByteArray(16 * 1024 * 1024, '\x5A'));
  image.close();

  QElapsedTimer timer;
  timer.start();
  mw->build_formatted_hex_output();
  const qint64 elapsedMs = timer.elapsed();
  qInfo("loaded and laid out 16 MiB in %lld ms", elapsedMs);
  QCOMPARE(mw->hexViewModel.rowCount(), 1024 * 1024);
  QCOMPARE(mw->hexViewModel.index(1024 * 1024 - 1, 16).data().toString(), QString("ZZZZZZZZZZZZZZZZ"));
  QVERIFY2(elapsedMs < 2000, qPrintable(QString("took %1 ms").arg(elapsedMs)));
}

void MainWindowTest::writeDoesNotLoadPreviousRead() {
  // Regression: finishing a write loaded temp.bin, showing the last read as the chip contents.
  auto mw = makeWindow();
  readFfRow(*mw);
  mw->write_file_to_device("/images/fw.bin");
  QTRY_COMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
  QVERIFY(QFile::exists("temp.bin"));  // the file was there to (wrongly) load
}

void MainWindowTest::readReportsMiniproFailingToStart() {
  auto mw = makeWindow();
  qputenv("PATH", emptyBinDir.toLocal8Bit());
  mw->read_device();
  QTRY_VERIFY(mw->status_view->toPlainText().contains("[Error]: could not start minipro"));
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
}

void MainWindowTest::controlsDisabledWhileOperationRuns() {
  qputenv("FAKE_MINIPRO_DELAY", "1");
  auto mw = makeWindow();
  mw->read_device();
  for (QWidget *control : std::initializer_list<QWidget *>{
           mw->button_read, mw->button_write, mw->button_update, mw->button_run_command,
           mw->combobox_device, mw->combobox_programmer}) {
    QVERIFY2(!control->isEnabled(), qPrintable(control->metaObject()->className()));
  }
  QTRY_VERIFY(mw->button_read->isEnabled());
  QVERIFY(mw->button_write->isEnabled());
  QVERIFY(mw->button_update->isEnabled());
  QVERIFY(mw->button_run_command->isEnabled());
  QVERIFY(mw->combobox_device->isEnabled());
  QVERIFY(mw->combobox_programmer->isEnabled());
}

void MainWindowTest::secondOperationIsRefused() {
  // Regression: a second click started another minipro on the same programmer mid-operation.
  qputenv("FAKE_MINIPRO_DELAY", "1");
  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("ab").repeated(16));
  auto mw = makeWindow();
  mw->combobox_device->setCurrentText("AT28C256");
  mw->read_device();
  mw->write_file_to_device("/images/fw.bin");
  mw->update_firmware_from_file("/images/update.dat");
  mw->read_device();
  mw->erase_device->setChecked(true);
  mw->run_command();
  QCOMPARE(mw->status_view->toPlainText().count("[Error]: another minipro operation is still running"), 4);

  QTRY_COMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("AB"));
  const QStringList deviceOps = loggedCalls().filter(QRegularExpression("^-(p|F) "));
  QCOMPARE(deviceOps, QStringList{"-p AT28C256 -r temp.bin"});
}

void MainWindowTest::controlsReenabledAfterFailedStart() {
  auto mw = makeWindow();
  qputenv("PATH", emptyBinDir.toLocal8Bit());
  mw->read_device();
  QTRY_VERIFY(mw->status_view->toPlainText().contains("[Error]: could not start minipro"));
  QVERIFY(mw->button_read->isEnabled());
  QVERIFY(mw->async_process == nullptr);
}

void MainWindowTest::finishedProcessesAreDeleted() {
  // Regression: every operation leaked a parentless QProcess.
  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("00").repeated(16));
  auto mw = makeWindow();
  for (int i = 0; i < 3; i++) {
    mw->read_device();
    QTRY_VERIFY(mw->async_process == nullptr);
  }
  QTRY_COMPARE(mw->findChildren<QProcess *>().size(), 0);
}

void MainWindowTest::closingWindowLetsRunningWriteFinish() {
  // Destroying the window must not kill minipro in the middle of a write.
  const QString doneLog = QDir(tempDir.path()).filePath("done.log");
  QFile::remove(doneLog);
  qputenv("FAKE_MINIPRO_DONE_LOG", doneLog.toLocal8Bit());
  qputenv("FAKE_MINIPRO_DELAY", "1");
  auto mw = makeWindow();
  mw->combobox_device->setCurrentText("AT28C256");
  mw->write_file_to_device("/images/fw.bin");
  QTRY_VERIFY(loggedCalls().contains("-p AT28C256 -w /images/fw.bin"));
  mw.reset();
  QFile done(doneLog);
  QVERIFY(done.open(QFile::ReadOnly | QFile::Text));
  QVERIFY(QString::fromLocal8Bit(done.readAll()).contains("-p AT28C256 -w /images/fw.bin"));
}

void MainWindowTest::firmwarePromptDoesNotHang() {
  // Regression: minipro waits for y/n on stdin, which the GUI left open, so Update Firmware
  // never finished. With end-of-input it aborts and the controls come back.
  auto mw = makeWindow();
  mw->update_firmware_from_file("/images/update.dat");
  QTRY_VERIFY(mw->async_process == nullptr);
  QVERIFY(mw->status_view->toPlainText().contains("Firmware update aborted."));
  QVERIFY(mw->status_view->toPlainText().contains("[Error]: minipro exited with code 1"));
  QVERIFY(mw->button_update->isEnabled());
}

void MainWindowTest::presenceCheckParsing_data() {
  QTest::addColumn<QString>("output");
  QTest::addColumn<QString>("programmer");  // empty: no programmer
  QTest::newRow("TL866II+") << "tl866ii: TL866II+" << "TL866II+";
  QTest::newRow("TL866A") << "tl866a: TL866A" << "TL866A";
  QTest::newRow("T48 with warning first")
      << "Warning: T48 support is not yet complete!\nt48: T48" << "T48";
  QTest::newRow("no programmer") << "[No programmer found]" << "";
  QTest::newRow("unknown version") << "[Unknown programmer version]" << "";
  // Regression: anything after the first ": " on the last line was taken as the programmer.
  QTest::newRow("error line") << "Error: USB device not found" << "";
  QTest::newRow("warning line") << "Warning: firmware: too old" << "";
  QTest::newRow("empty") << "" << "";
}

void MainWindowTest::presenceCheckParsing() {
  QFETCH(QString, output);
  QFETCH(QString, programmer);
  if (output.isEmpty()) qputenv("FAKE_MINIPRO_PRESENCE", " ");
  else qputenv("FAKE_MINIPRO_PRESENCE", output.toLocal8Bit());
  auto mw = makeWindow();
  QCOMPARE(mw->programmer_found, !programmer.isEmpty());
  QCOMPARE(mw->button_read->isEnabled(), !programmer.isEmpty());
  if (!programmer.isEmpty()) {
    QCOMPARE(mw->programmer, programmer);
    QCOMPARE(mw->combobox_programmer->itemText(0), programmer);
  }
}

void MainWindowTest::presenceCheckDoesNotDuplicateProgrammer() {
  auto mw = makeWindow();
  mw->check_for_programmer();
  mw->check_for_programmer();
  QCOMPARE(mw->combobox_programmer->count(), 1);
}

void MainWindowTest::queryPromptDoesNotHang() {
  // Regression: without a programmer, -d asks "Which database...?" on stdin, which was left
  // open, so selecting a device froze the UI for 30 s and left minipro running.
  auto mw = makeWindow();
  qputenv("FAKE_MINIPRO_INFO_PROMPT", "1");
  QElapsedTimer timer;
  timer.start();
  mw->combobox_device->setCurrentText("AT28C256");
  QVERIFY2(timer.elapsed() < 3000, qPrintable(QString("took %1 ms").arg(timer.elapsed())));
  QVERIFY(mw->status_view->toPlainText().contains("Aborted."));
}

void MainWindowTest::slowQueryIsStopped() {
  MainWindow::sync_timeout_ms = 500;
  auto mw = makeWindow();
  qputenv("FAKE_MINIPRO_INFO_SLEEP", "5");
  QElapsedTimer timer;
  timer.start();
  mw->combobox_device->setCurrentText("AT28C256");
  QVERIFY2(timer.elapsed() < 3000, qPrintable(QString("took %1 ms").arg(timer.elapsed())));
  QVERIFY(mw->status_view->toPlainText().contains(
      "[Error]: minipro didn't finish within 500 ms and was stopped"));
}

void MainWindowTest::startFailureIsReportedNotParsed() {
  // Regression: "Start Error" and "Finished Error" were glued onto minipro's output and parsed
  // with it (e.g. into the device list).
  auto mw = makeWindow();
  qputenv("PATH", emptyBinDir.toLocal8Bit());
  QPlainTextEdit pane;
  QCOMPARE(MainWindow::run_process(pane, {"-l"}, "stdout"), QString());
  QVERIFY(pane.toPlainText().contains("[Error]: could not start minipro"));
  QVERIFY(!pane.toPlainText().contains("Start Error"));
  QVERIFY(!pane.toPlainText().contains("Finished Error"));
}

void MainWindowTest::runCommandDoesNotBlockAndKeepsOrResetsView() {
  // Regression: Run Command used the synchronous path, so a long erase froze the UI and was
  // cut off after 30 s.
  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("ff").repeated(16));
  auto mw = makeWindow();
  readFfRow(*mw);

  qputenv("FAKE_MINIPRO_DELAY", "1");
  mw->blank_check->setChecked(true);
  QElapsedTimer timer;
  timer.start();
  mw->run_command();
  QVERIFY2(timer.elapsed() < 500, qPrintable(QString("took %1 ms").arg(timer.elapsed())));
  QVERIFY(mw->async_process != nullptr);
  QTRY_VERIFY(mw->async_process == nullptr);
  // A blank check doesn't change the chip, so the last read stays.
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("FF"));

  mw->blank_check->setChecked(false);
  mw->erase_device->setChecked(true);
  mw->run_command();
  QTRY_VERIFY(mw->async_process == nullptr);
  // An erase does, so the view goes back to the placeholder.
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
}

void MainWindowTest::firmwareUpdateWhenConfirmed() {
  // Regression: nothing ever answered minipro's y/n prompt, so Update Firmware never ran.
  auto mw = makeWindow();
  QStringList asked;
  mw->confirm_firmware_update = [&asked](const QString &text) {
    asked << text;
    return true;
  };
  mw->update_firmware_from_file("/images/update.dat");
  QTRY_VERIFY(mw->async_process == nullptr);
  QCOMPARE(asked.size(), 1);
  // The dialog shows the versions minipro printed before its prompt.
  QVERIFY(asked.first().contains("/images/update.dat contains firmware version 04.2.132 (newer)"));
  QVERIFY(mw->status_view->toPlainText().contains("[Input]: y"));
  QVERIFY(mw->status_view->toPlainText().contains("Firmware update completed."));
  QVERIFY(!mw->status_view->toPlainText().contains("[Error]"));
}

void MainWindowTest::firmwareUpdateWhenDeclined() {
  auto mw = makeWindow();
  int asked = 0;
  mw->confirm_firmware_update = [&asked](const QString &) {
    asked++;
    return false;
  };
  mw->update_firmware_from_file("/images/update.dat");
  QTRY_VERIFY(mw->async_process == nullptr);
  QCOMPARE(asked, 1);
  QVERIFY(mw->status_view->toPlainText().contains("[Input]: n"));
  QVERIFY(mw->status_view->toPlainText().contains("Firmware update aborted."));
  QVERIFY(!mw->status_view->toPlainText().contains("Firmware update completed."));
}

void MainWindowTest::closingWindowAtFirmwarePromptAborts() {
  // The window waits for a running operation when it closes. A firmware update waiting at its
  // prompt must not make that wait forever, and must not go ahead.
  const QString doneLog = QDir(tempDir.path()).filePath("done.log");
  QFile::remove(doneLog);
  qputenv("FAKE_MINIPRO_DONE_LOG", doneLog.toLocal8Bit());
  auto mw = makeWindow();
  mw->update_firmware_from_file("/images/update.dat");
  QTRY_VERIFY(loggedCalls().contains("-F /images/update.dat"));
  QElapsedTimer timer;
  timer.start();
  mw.reset();
  QVERIFY2(timer.elapsed() < 3000, qPrintable(QString("took %1 ms").arg(timer.elapsed())));
  QVERIFY(!QFile::exists(doneLog));  // the fake exits at the prompt, before the end of the update
}

void MainWindowTest::failedDeviceInfoClearsPreviousDevice() {
  // Regression: when -d failed, the previous device's details stayed next to the new name.
  auto mw = makeWindow();
  mw->combobox_device->setCurrentText("AT28C256");
  QCOMPARE(mw->device_memory->text(), QString("32768 Bytes"));

  qputenv("FAKE_MINIPRO_INFO_FAIL", "1");
  mw->combobox_device->setCurrentText("W27C512");
  QCOMPARE(mw->device, QString("W27C512"));
  for (QLineEdit *field : {mw->device_name, mw->device_memory, mw->device_package,
                           mw->device_protocol, mw->device_readbuffer, mw->device_writebuffer}) {
    QCOMPARE(field->text(), QString());
  }
  QVERIFY(mw->status_view->toPlainText().contains("[Error]: no device information for W27C512"));
}

void MainWindowTest::windowsLineEndingsAreStripped() {
  // Regression: splitting on \n alone left \r in device names and info fields.
  qputenv("FAKE_MINIPRO_CRLF", "1");
  auto mw = makeWindow();
  const QStringList expected{"27C256", "AT28C256", "W27C512"};
  QCOMPARE(mw->devices_list, expected);
  mw->combobox_device->setCurrentText("AT28C256");
  QVERIFY(loggedCalls().contains("-d AT28C256"));
  QCOMPARE(mw->device_name->text(), QString("AT28C256"));
  QCOMPARE(mw->device_writebuffer->text(), QString("128 Bytes"));
}

QTEST_MAIN(MainWindowTest)
#include "MainWindowTest.moc"
