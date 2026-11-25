#include "../src/MainWindow.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <memory>

// MainWindow drives the `minipro` CLI found on PATH. These tests put tests/fake_minipro.sh on PATH
// (as "minipro") so they are deterministic and need neither the real CLI nor a programmer.
// FAKE_MINIPRO_SOURCE is defined by tests/CMakeLists.txt.

class MainWindowTest : public QObject {
 Q_OBJECT

 private slots:
  void initTestCase();
  void init();
  void cleanup();

  // Startup / discovery
  void detectsMinipro();
  void versionParsing_data();
  void versionParsing();
  void oddDeviceInfoOutput_data();
  void oddDeviceInfoOutput();
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
  void emptyDeviceListDisablesDeviceOperations();
  void operationsRefuseEmptyDevice();
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
  void readsGoToPrivateFolderNotWorkingDirectory();
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
  void tl866aFirmwareUpdateWhenConfirmed();
  void tl866aFirmwareUpdateWhenDeclined();

  // Cancelling a running operation
  void cancelIsOnlyEnabledWhileRunning();
  void cancelStopsARead();
  void cancelAnswersAnUnknownPrompt();
  void cancelDuringWriteAsksBeforeStopping();
  void cancelWhenOperationEndsDuringDialogIsSafe();
  void cancelDuringFirmwareFlashAsksBeforeStopping();
  void cancelHardwareCheckAsks();

  // Confirmations before changing the chip
  void writeAsksAndCanBeCancelled();
  void eraseAsksAndCanBeCancelled();
  void checksDoNotAsk();

 private:
  using WindowPtr = std::unique_ptr<MainWindow>;
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
  // Keep anything written to the working directory out of the source tree.
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
  qunsetenv("FAKE_MINIPRO_LIST_EMPTY");
  qunsetenv("FAKE_MINIPRO_TL866A");
  qunsetenv("FAKE_MINIPRO_FW_PROMPT_UNKNOWN");
  qunsetenv("FAKE_MINIPRO_VERSION_TEXT");
  qunsetenv("FAKE_MINIPRO_INFO_TEXT");
  MainWindow::sync_timeout_ms = 10000;
  qunsetenv("FAKE_MINIPRO_READ_HEX");
  qunsetenv("FAKE_MINIPRO_READ_EXIT");
  qunsetenv("FAKE_MINIPRO_READ_EMPTY");
  qunsetenv("FAKE_MINIPRO_CRASH");
  qunsetenv("FAKE_MINIPRO_DELAY");
  qunsetenv("FAKE_MINIPRO_DONE_LOG");
  QFile::remove(logPath);
}

void MainWindowTest::cleanup() {
  qputenv("PATH", originalPath);
}

MainWindowTest::WindowPtr MainWindowTest::makeWindow() {
  auto mw = std::make_unique<MainWindow>();
  // Never open a real dialog in tests; decline by default (the safe answer).
  mw->confirmations.firmware_update = [](const QString &) { return false; };
  // Write and erase tests exercise the operation itself; declining is tested separately.
  mw->confirmations.device_change = [](const QString &, const QString &) { return true; };
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
  QVERIFY(!mw->buttons.read->isEnabled());
  QVERIFY(!mw->buttons.write->isEnabled());
  QVERIFY(!mw->buttons.run_command->isEnabled());
  QVERIFY(!mw->buttons.update->isEnabled());
  QVERIFY(!mw->targets.device->isEnabled());
  QCOMPARE(mw->targets.device->count(), 0);
}

void MainWindowTest::detectsProgrammer() {
  auto mw = makeWindow();
  QVERIFY(mw->programmer_found);
  QCOMPARE(mw->programmer, QString("TL866II+"));
  QCOMPARE(mw->targets.programmer->itemText(0), QString("TL866II+"));
  QVERIFY(mw->buttons.read->isEnabled());
  QVERIFY(mw->buttons.write->isEnabled());
  QVERIFY(mw->buttons.run_command->isEnabled());
  QVERIFY(mw->buttons.update->isEnabled());
  QVERIFY(mw->targets.device->isEnabled());
}

void MainWindowTest::missingProgrammerDisablesUi() {
  qputenv("FAKE_MINIPRO_NO_PROGRAMMER", "1");
  auto mw = makeWindow();
  QVERIFY(mw->minipro_found);
  QVERIFY(!mw->programmer_found);
  QCOMPARE(mw->targets.programmer->placeholderText(), QString("No programmer found"));
  QVERIFY(!mw->buttons.read->isEnabled());
  QVERIFY(!mw->targets.device->isEnabled());
  QCOMPARE(mw->targets.device->count(), 0);
  QVERIFY(!loggedCalls().contains("-l"));
}

void MainWindowTest::deviceListIsSortedAndDeduplicated() {
  auto mw = makeWindow();
  const QStringList expected{"27C256", "AT28C256", "W27C512"};
  QCOMPARE(mw->devices_list, expected);
  QCOMPARE(mw->targets.device->count(), expected.size());
  for (int i = 0; i < expected.size(); i++) {
    QCOMPARE(mw->targets.device->itemText(i), expected.at(i));
  }
}

void MainWindowTest::selectDeviceFillsDeviceInfo() {
  auto mw = makeWindow();
  // Populating the combobox auto-selects the first device.
  QCOMPARE(mw->device, QString("27C256"));
  QCOMPARE(mw->device_info.name->text(), QString("27C256"));

  mw->targets.device->setCurrentText("AT28C256");
  QCOMPARE(mw->device, QString("AT28C256"));
  QCOMPARE(mw->device_info.name->text(), QString("AT28C256"));
  QCOMPARE(mw->device_info.memory->text(), QString("32768 Bytes"));
  QCOMPARE(mw->device_info.package->text(), QString("DIP28"));
  QCOMPARE(mw->device_info.protocol->text(), QString("0x05"));
  QCOMPARE(mw->device_info.read_buffer->text(), QString("1024 Bytes"));
  QCOMPARE(mw->device_info.write_buffer->text(), QString("128 Bytes"));
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
      {"no_id_error", mw->options.no_id_error}, {"skip_id", mw->options.skip_id},
      {"no_size_error", mw->options.no_size_error}, {"skip_verify", mw->options.skip_verify},
      {"pin_check", mw->options.pin_check}, {"blank_check", mw->actions.blank_check},
      {"erase_device", mw->actions.erase}, {"hardware_check", mw->actions.hardware_check}};
  QVERIFY(boxes.contains(checkbox));
  boxes.value(checkbox)->setChecked(true);
  const QStringList expected{flag};
  QCOMPARE(mw->parse_action_checkboxes(), isAction ? expected : QStringList{});
  QCOMPARE(mw->parse_option_checkboxes(), isAction ? QStringList{} : expected);
}

void MainWindowTest::parseCheckboxesAllChecked() {
  auto mw = makeWindow();
  checkAll({mw->options.no_id_error, mw->options.skip_id, mw->options.no_size_error, mw->options.skip_verify, mw->options.pin_check,
            mw->actions.blank_check, mw->actions.erase, mw->actions.hardware_check});
  const QStringList options{"--no_id_error", "--skip_id", "--no_size_error", "--skip_verify",
                            "--pin_check"};
  const QStringList actions{"--blank_check", "--erase", "--hardware_check"};
  QCOMPARE(mw->parse_option_checkboxes(), options);
  QCOMPARE(mw->parse_action_checkboxes(), actions);
}

void MainWindowTest::runCommandRunsOneActionWithOptions() {
  auto mw = makeWindow();
  mw->targets.device->setCurrentText("AT28C256");
  checkAll({mw->actions.erase, mw->options.pin_check});
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
  const QMap<QString, QCheckBox *> boxes{{"pin_check", mw->options.pin_check},
                                         {"blank_check", mw->actions.blank_check},
                                         {"erase_device", mw->actions.erase},
                                         {"hardware_check", mw->actions.hardware_check}};
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
  mw->targets.device->setCurrentText("AT28C256");
  checkAll({mw->actions.blank_check, mw->actions.erase, mw->actions.hardware_check, mw->options.no_size_error});
  mw->write_file_to_device("/images/fw.bin");
  QTRY_VERIFY(loggedCalls().contains("-p AT28C256 -w /images/fw.bin --no_size_error"));
  QTRY_VERIFY(mw->async_process == nullptr);
}

void MainWindowTest::updateFirmwarePassesNoCheckboxFlags() {
  auto mw = makeWindow();
  checkAll({mw->options.no_id_error, mw->options.skip_id, mw->options.no_size_error, mw->options.skip_verify, mw->options.pin_check,
            mw->actions.blank_check, mw->actions.erase, mw->actions.hardware_check});
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
  QVERIFY(!target.toPlainText().contains(QString(QChar(0x1B)) + "[K"));
}

void MainWindowTest::readDeviceFillsHexView() {
  // 32 bytes: "Hello, minipro!!" followed by 00..0F
  QByteArray bytes("Hello, minipro!!");
  for (int i = 0; i < 16; i++) bytes.append(static_cast<char>(i));
  qputenv("FAKE_MINIPRO_READ_HEX", bytes.toHex());

  auto mw = makeWindow();
  mw->targets.device->setCurrentText("AT28C256");
  mw->read_device();

  const HexView &model = mw->hexViewModel;
  QTRY_COMPARE(model.rowCount(), 2);
  QCOMPARE(model.index(0, 0).data().toString(), QString("48"));
  QCOMPARE(model.index(0, 16).data().toString(), QString("Hello, minipro!!"));
  QCOMPARE(model.index(1, 15).data().toString(), QString("0F"));
  QCOMPARE(model.index(1, 16).data().toString(), QString("................"));
  QCOMPARE(model.headerData(1, Qt::Vertical, Qt::DisplayRole).toString(), QString("000010"));
  const QString read = mw->read_file;
  QVERIFY(loggedCalls().contains("-p AT28C256 -r " + read));
  QVERIFY(mw->status_view->toPlainText().contains("[Input]: minipro -p AT28C256 -r " + read));
}

void MainWindowTest::readDevicePassesOnlyOptionFlags() {
  // Regression: --blank_check replaced -r, so the "read" showed the previous read.
  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("00").repeated(16));
  auto mw = makeWindow();
  mw->targets.device->setCurrentText("W27C512");
  checkAll({mw->options.skip_verify, mw->actions.blank_check, mw->actions.erase, mw->actions.hardware_check});
  mw->read_device();
  QTRY_COMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("00"));
  QVERIFY(loggedCalls().contains("-p W27C512 -r " + mw->read_file + " --skip_verify"));
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
  // Fake read succeeds and writes an empty file, so the hex view receives no bytes.
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
  // Regression: any exit code other than 1, or a crash, loaded the file from the previous read.
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
  // Regression: the read file was never removed, so a read that wrote nothing showed the last chip.
  auto mw = makeWindow();
  readFfRow(*mw);
  qunsetenv("FAKE_MINIPRO_READ_HEX");  // the next read succeeds but writes no file
  mw->read_device();
  QTRY_VERIFY(mw->status_view->toPlainText().contains("[Error]: could not open the read result"));
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
}

void MainWindowTest::readWithNoFileReportsError() {
  // Regression: QFile::open wasn't checked, so a missing file was shown as an empty chip.
  auto mw = makeWindow();
  mw->read_device();
  QTRY_VERIFY(mw->status_view->toPlainText().contains("[Error]: could not open the read result"));
  QCOMPARE(mw->hexViewModel.rowCount(), 8);
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
}

void MainWindowTest::largeImageLoadsQuickly() {
  // Regression: loading a 16 MiB image (a million rows) took over 10 s and more than 1 GB.
  auto mw = makeWindow();
  QFile image(mw->read_file);
  QVERIFY(image.open(QFile::WriteOnly));
  image.write(QByteArray(16 * 1024 * 1024, 'Z'));
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
  // Regression: finishing a write loaded the read file, showing the last read as the chip contents.
  auto mw = makeWindow();
  readFfRow(*mw);
  mw->write_file_to_device("/images/fw.bin");
  QTRY_COMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
  QVERIFY(QFile::exists(mw->read_file));  // the file was there to (wrongly) load
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
  for (const QWidget *control : std::initializer_list<QWidget *>{
           mw->buttons.read, mw->buttons.write, mw->buttons.update, mw->buttons.run_command,
           mw->targets.device, mw->targets.programmer}) {
    QVERIFY2(!control->isEnabled(), qPrintable(control->metaObject()->className()));
  }
  QTRY_VERIFY(mw->buttons.read->isEnabled());
  QVERIFY(mw->buttons.write->isEnabled());
  QVERIFY(mw->buttons.update->isEnabled());
  QVERIFY(mw->buttons.run_command->isEnabled());
  QVERIFY(mw->targets.device->isEnabled());
  QVERIFY(mw->targets.programmer->isEnabled());
}

void MainWindowTest::secondOperationIsRefused() {
  // Regression: a second click started another minipro on the same programmer mid-operation.
  qputenv("FAKE_MINIPRO_DELAY", "1");
  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("ab").repeated(16));
  auto mw = makeWindow();
  mw->targets.device->setCurrentText("AT28C256");
  mw->read_device();
  mw->write_file_to_device("/images/fw.bin");
  mw->update_firmware_from_file("/images/update.dat");
  mw->read_device();
  mw->actions.erase->setChecked(true);
  mw->run_command();
  QCOMPARE(mw->status_view->toPlainText().count("[Error]: another minipro operation is still running"), 4);

  QTRY_COMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("AB"));
  const QStringList deviceOps = loggedCalls().filter(QRegularExpression("^-(p|F) "));
  QCOMPARE(deviceOps, QStringList{"-p AT28C256 -r " + mw->read_file});
}

void MainWindowTest::controlsReenabledAfterFailedStart() {
  auto mw = makeWindow();
  qputenv("PATH", emptyBinDir.toLocal8Bit());
  mw->read_device();
  QTRY_VERIFY(mw->status_view->toPlainText().contains("[Error]: could not start minipro"));
  QVERIFY(mw->buttons.read->isEnabled());
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
  mw->targets.device->setCurrentText("AT28C256");
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
  QVERIFY(mw->buttons.update->isEnabled());
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
  QCOMPARE(mw->buttons.read->isEnabled(), !programmer.isEmpty());
  if (!programmer.isEmpty()) {
    QCOMPARE(mw->programmer, programmer);
    QCOMPARE(mw->targets.programmer->itemText(0), programmer);
  }
}

void MainWindowTest::presenceCheckDoesNotDuplicateProgrammer() {
  auto mw = makeWindow();
  mw->check_for_programmer();
  mw->check_for_programmer();
  QCOMPARE(mw->targets.programmer->count(), 1);
}

void MainWindowTest::queryPromptDoesNotHang() {
  // Regression: without a programmer, -d asks "Which database...?" on stdin, which was left
  // open, so selecting a device froze the UI for 30 s and left minipro running.
  auto mw = makeWindow();
  qputenv("FAKE_MINIPRO_INFO_PROMPT", "1");
  QElapsedTimer timer;
  timer.start();
  mw->targets.device->setCurrentText("AT28C256");
  QVERIFY2(timer.elapsed() < 3000, qPrintable(QString("took %1 ms").arg(timer.elapsed())));
  QVERIFY(mw->status_view->toPlainText().contains("Aborted."));
}

void MainWindowTest::slowQueryIsStopped() {
  MainWindow::sync_timeout_ms = 500;
  auto mw = makeWindow();
  qputenv("FAKE_MINIPRO_INFO_SLEEP", "5");
  QElapsedTimer timer;
  timer.start();
  mw->targets.device->setCurrentText("AT28C256");
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
  mw->actions.blank_check->setChecked(true);
  QElapsedTimer timer;
  timer.start();
  mw->run_command();
  QVERIFY2(timer.elapsed() < 500, qPrintable(QString("took %1 ms").arg(timer.elapsed())));
  QVERIFY(mw->async_process != nullptr);
  QTRY_VERIFY(mw->async_process == nullptr);
  // A blank check doesn't change the chip, so the last read stays.
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("FF"));

  mw->actions.blank_check->setChecked(false);
  mw->actions.erase->setChecked(true);
  mw->run_command();
  QTRY_VERIFY(mw->async_process == nullptr);
  // An erase does, so the view goes back to the placeholder.
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
}

void MainWindowTest::firmwareUpdateWhenConfirmed() {
  // Regression: nothing ever answered minipro's y/n prompt, so Update Firmware never ran.
  auto mw = makeWindow();
  QStringList asked;
  mw->confirmations.firmware_update = [&asked](const QString &text) {
    asked << text;
    return true;
  };
  mw->update_firmware_from_file("/images/update.dat");
  QTRY_VERIFY(mw->async_process == nullptr);
  QCOMPARE(asked.size(), 1);
  // The dialog shows the versions minipro printed before its prompt, not the prompt itself.
  QVERIFY(asked.first().contains("/images/update.dat contains firmware version 04.2.132 (newer)"));
  QVERIFY(!asked.first().contains("Do you want to continue"));
  QVERIFY(mw->status_view->toPlainText().contains("[Input]: y"));
  QVERIFY(mw->status_view->toPlainText().contains("Firmware update completed."));
  QVERIFY(!mw->status_view->toPlainText().contains("[Error]"));
}

void MainWindowTest::firmwareUpdateWhenDeclined() {
  auto mw = makeWindow();
  int asked = 0;
  mw->confirmations.firmware_update = [&asked](const QString &) {
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
  mw->targets.device->setCurrentText("AT28C256");
  QCOMPARE(mw->device_info.memory->text(), QString("32768 Bytes"));

  qputenv("FAKE_MINIPRO_INFO_FAIL", "1");
  mw->targets.device->setCurrentText("W27C512");
  QCOMPARE(mw->device, QString("W27C512"));
  for (const QLineEdit *field : {mw->device_info.name, mw->device_info.memory, mw->device_info.package,
                           mw->device_info.protocol, mw->device_info.read_buffer, mw->device_info.write_buffer}) {
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
  mw->targets.device->setCurrentText("AT28C256");
  QVERIFY(loggedCalls().contains("-d AT28C256"));
  QCOMPARE(mw->device_info.name->text(), QString("AT28C256"));
  QCOMPARE(mw->device_info.write_buffer->text(), QString("128 Bytes"));
}

void MainWindowTest::readsGoToPrivateFolderNotWorkingDirectory() {
  // Regression: reads went to ./temp.bin, overwriting (and before each read, deleting) a user's
  // own file of that name, and failing when the working directory isn't writable.
  QFile own("temp.bin");
  QVERIFY(own.open(QFile::WriteOnly));
  own.write("my own file");
  own.close();

  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("ab").repeated(16));
  auto mw = makeWindow();
  mw->read_device();
  QTRY_COMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("AB"));

  QVERIFY(own.open(QFile::ReadOnly));
  QCOMPARE(own.readAll(), QByteArray("my own file"));
  own.close();
  QVERIFY(QFile::remove("temp.bin"));

  const QFileInfo read(mw->read_file);
  QVERIFY(read.absolutePath() != QDir::currentPath());
  QCOMPARE(QFileInfo(read.absolutePath()).permissions() & (QFile::ReadGroup | QFile::ReadOther |
                                                           QFile::WriteGroup | QFile::WriteOther),
           QFile::Permissions());

  // The folder goes away with the window.
  const QString folder = read.absolutePath();
  mw.reset();
  QVERIFY(!QFileInfo::exists(folder));
}

void MainWindowTest::versionParsing_data() {
  QTest::addColumn<QString>("output");
  QTest::addColumn<QString>("title");  // empty: minipro not detected
  QTest::newRow("real output")
      << "Supported programmers: TL866A/CS, TL866II+, T48, T56\\nminipro version 0.7.4     A free "
         "and open TL866 series programmer\\nCommit date:\\t2024-01-01\\n"
      << "minipro version 0.7.4     A free and open TL866 series programmer";
  // Regression: the version line was only found when a newline followed it.
  QTest::newRow("version line last, no newline") << "minipro version 0.7.5" << "minipro version 0.7.5";
  QTest::newRow("windows line endings") << "minipro version 0.7.5\\r\\n" << "minipro version 0.7.5";
  QTest::newRow("another program") << "someprog version 1.0\\n" << "";
  QTest::newRow("mentioned mid-line") << "error: needs minipro version 0.8\\n" << "";
  QTest::newRow("empty") << "" << "";
}

void MainWindowTest::versionParsing() {
  QFETCH(QString, output);
  QFETCH(QString, title);
  qputenv("FAKE_MINIPRO_VERSION_TEXT", output.isEmpty() ? QByteArray(" ") : output.toLocal8Bit());
  auto mw = makeWindow();
  QCOMPARE(mw->minipro_found, !title.isEmpty());
  QCOMPARE(mw->window->windowTitle(), title.isEmpty() ? QString("minipro CLI not found!") : title);
}

void MainWindowTest::oddDeviceInfoOutput_data() {
  QTest::addColumn<QString>("output");
  QTest::addColumn<QString>("name");    // expected Name field; empty: no device information
  QTest::addColumn<QString>("memory");
  QTest::newRow("fields missing") << "Name: X28C64\\nPackage: DIP28\\n" << "X28C64" << "";
  QTest::newRow("unknown lines and order")
      << "Something new: 1\\nMemory: 8192 Bytes\\nName: X28C64\\nVPP: 12V\\n" << "X28C64" << "8192 Bytes";
  QTest::newRow("no fields") << "Unexpected output\\n" << "" << "";
  QTest::newRow("escape sequences")
      << "\\033[1mWarning\\033[0m\\nName: X28C64\\033[K\\nMemory: 8192 Bytes\\n" << "X28C64" << "8192 Bytes";
  QTest::newRow("very long line")
      << "Name: X28C64\\nMemory: " + QString(100000, 'A') + "\\n" << "X28C64" << QString(100000, 'A');
}

void MainWindowTest::oddDeviceInfoOutput() {
  // minipro's output changes between versions; parsing must never crash or show stale data.
  QFETCH(QString, output);
  QFETCH(QString, name);
  QFETCH(QString, memory);
  auto mw = makeWindow();
  qputenv("FAKE_MINIPRO_INFO_TEXT", output.toLocal8Bit());
  mw->targets.device->setCurrentText("AT28C256");
  QCOMPARE(mw->device_info.name->text(), name);
  // QLineEdit keeps at most maxLength() (32767) characters.
  QCOMPARE(mw->device_info.memory->text(), memory.left(mw->device_info.memory->maxLength()));
  QCOMPARE(mw->status_view->toPlainText().contains("[Error]: no device information for AT28C256"),
           name.isEmpty());
}

void MainWindowTest::writeAsksAndCanBeCancelled() {
  auto mw = makeWindow();
  mw->targets.device->setCurrentText("AT28C256");
  QStringList asked;
  mw->confirmations.device_change = [&asked](const QString &title, const QString &text) {
    asked << title << text;
    return false;
  };
  mw->write_file_to_device("/images/fw.bin");
  QCOMPARE(asked.size(), 2);
  QCOMPARE(asked.at(0), QString("Write to AT28C256?"));
  QVERIFY(asked.at(1).contains(QDir::toNativeSeparators("/images/fw.bin")));
  QVERIFY(mw->async_process == nullptr);
  QVERIFY(loggedCalls().filter(" -w ").isEmpty());
  QVERIFY(mw->status_view->toPlainText().contains("[Cancelled]: write to AT28C256"));
  QVERIFY(mw->buttons.write->isEnabled());
}

void MainWindowTest::eraseAsksAndCanBeCancelled() {
  auto mw = makeWindow();
  mw->targets.device->setCurrentText("AT28C256");
  QString title;
  mw->confirmations.device_change = [&title](const QString &t, const QString &) {
    title = t;
    return false;
  };
  mw->actions.erase->setChecked(true);
  mw->run_command();
  QCOMPARE(title, QString("Erase AT28C256?"));
  QVERIFY(mw->async_process == nullptr);
  QVERIFY(loggedCalls().filter("--erase").isEmpty());
  QVERIFY(mw->status_view->toPlainText().contains("[Cancelled]: erase AT28C256"));
}

void MainWindowTest::checksDoNotAsk() {
  // Blank, pin and hardware checks don't change the chip.
  auto mw = makeWindow();
  int asked = 0;
  mw->confirmations.device_change = [&asked](const QString &, const QString &) {
    asked++;
    return false;
  };
  mw->actions.blank_check->setChecked(true);
  mw->run_command();
  QTRY_VERIFY(mw->async_process == nullptr);
  QCOMPARE(asked, 0);
  QVERIFY(loggedCalls().last().contains("--blank_check"));
}

void MainWindowTest::emptyDeviceListDisablesDeviceOperations() {
  // Regression: the buttons were enabled as soon as a programmer was found, before -l was parsed,
  // so an empty device list left Read, Write and Run Command enabled with no device.
  qputenv("FAKE_MINIPRO_LIST_EMPTY", "1");
  auto mw = makeWindow();
  QVERIFY(mw->programmer_found);
  QCOMPARE(mw->device, QString());
  QVERIFY(!mw->targets.device->isEnabled());
  QVERIFY(!mw->buttons.read->isEnabled());
  QVERIFY(!mw->buttons.write->isEnabled());
  QVERIFY(!mw->buttons.run_command->isEnabled());
  QVERIFY(mw->buttons.update->isEnabled());  // firmware updates need no device

  // The same holds after an operation finishes and the controls are restored.
  mw->update_firmware_from_file("/images/update.dat");
  QTRY_VERIFY(mw->async_process == nullptr);
  QVERIFY(!mw->buttons.read->isEnabled());
  QVERIFY(mw->buttons.update->isEnabled());
}

void MainWindowTest::operationsRefuseEmptyDevice() {
  qputenv("FAKE_MINIPRO_LIST_EMPTY", "1");
  auto mw = makeWindow();
  int asked = 0;
  mw->confirmations.device_change = [&asked](const QString &, const QString &) {
    asked++;
    return true;
  };
  mw->read_device();
  mw->write_file_to_device("/images/fw.bin");
  mw->actions.erase->setChecked(true);
  mw->run_command();
  QCOMPARE(mw->status_view->toPlainText().count("[Error]: no device selected"), 3);
  QCOMPARE(asked, 0);  // refused before asking
  QVERIFY(mw->async_process == nullptr);
  QVERIFY(loggedCalls().filter(QRegularExpression("^-p ")).isEmpty());
}

void MainWindowTest::tl866aFirmwareUpdateWhenConfirmed() {
  // Regression: a TL866A/CS asks "Which firmware version do you want to reflash?" with a
  // numbered menu; only the y/n prompt was recognised, so Update Firmware hung.
  qputenv("FAKE_MINIPRO_TL866A", "1");
  auto mw = makeWindow();
  QStringList asked;
  mw->confirmations.firmware_update = [&asked](const QString &text) {
    asked << text;
    return true;
  };
  mw->update_firmware_from_file("/images/update.dat");
  QTRY_VERIFY(mw->async_process == nullptr);
  QCOMPARE(asked.size(), 1);
  QVERIFY(asked.first().contains("/images/update.dat contains firmware version 03.2.86 (newer)"));
  QVERIFY(!asked.first().contains("Which firmware version"));
  QVERIFY(!asked.first().contains("2) A"));
  // "1" reflashes the programmer's own variant; switching A <-> CS ("2") is never sent.
  QVERIFY(mw->status_view->toPlainText().contains("[Input]: 1"));
  QVERIFY(mw->status_view->toPlainText().contains("Reflashing TL866CS firmware... OK"));
  QVERIFY(!mw->status_view->toPlainText().contains("[Error]"));
}

void MainWindowTest::tl866aFirmwareUpdateWhenDeclined() {
  qputenv("FAKE_MINIPRO_TL866A", "1");
  auto mw = makeWindow();
  mw->update_firmware_from_file("/images/update.dat");  // makeWindow() declines by default
  QTRY_VERIFY(mw->async_process == nullptr);
  QVERIFY(mw->status_view->toPlainText().contains("[Input]: 3"));
  QVERIFY(mw->status_view->toPlainText().contains("Firmware update aborted."));
  QVERIFY(!mw->status_view->toPlainText().contains("Reflashing"));
  QVERIFY(mw->buttons.update->isEnabled());
}

void MainWindowTest::cancelIsOnlyEnabledWhileRunning() {
  qputenv("FAKE_MINIPRO_DELAY", "1");
  auto mw = makeWindow();
  QVERIFY(!mw->buttons.cancel->isEnabled());
  mw->read_device();
  QVERIFY(mw->buttons.cancel->isEnabled());
  QTRY_VERIFY(mw->async_process == nullptr);
  QVERIFY(!mw->buttons.cancel->isEnabled());
}

void MainWindowTest::cancelStopsARead() {
  // A read changes nothing on the chip, so one click stops it at once.
  qputenv("FAKE_MINIPRO_DELAY", "10");
  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("ff").repeated(16));
  auto mw = makeWindow();
  mw->read_device();
  QElapsedTimer timer;
  timer.start();
  mw->buttons.cancel->click();
  QTRY_VERIFY(mw->async_process == nullptr);
  QVERIFY2(timer.elapsed() < 3000, qPrintable(QString("took %1 ms").arg(timer.elapsed())));
  QVERIFY(mw->status_view->toPlainText().contains("[Cancelled]: minipro was stopped"));
  QVERIFY(!mw->status_view->toPlainText().contains("[Error]: minipro crashed"));
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
  QVERIFY(mw->buttons.read->isEnabled());
}

void MainWindowTest::cancelAnswersAnUnknownPrompt() {
  // A firmware update waiting at a question the app doesn't recognise: Cancel answers it with
  // end-of-input (minipro treats that as "no") rather than killing minipro.
  qputenv("FAKE_MINIPRO_FW_PROMPT_UNKNOWN", "1");
  auto mw = makeWindow();
  int asked = 0;
  mw->confirmations.firmware_update = [&asked](const QString &) {
    asked++;
    return true;
  };
  mw->update_firmware_from_file("/images/update.dat");
  QTRY_VERIFY(mw->status_view->toPlainText().contains("Really flash the programmer?"));
  QTest::qWait(200);
  QVERIFY(mw->async_process != nullptr);  // waiting at the prompt
  mw->buttons.cancel->click();
  QTRY_VERIFY(mw->async_process == nullptr);
  QCOMPARE(asked, 0);
  // The fake read end-of-input and aborted by itself: it wasn't stopped. Its own exit code is
  // reported, not "[Cancelled]".
  QVERIFY(mw->status_view->toPlainText().contains("Firmware update aborted."));
  QVERIFY(!mw->status_view->toPlainText().contains("[Cancel]: stopping minipro"));
  QVERIFY(mw->status_view->toPlainText().contains("[Error]: minipro exited with code 1"));
  QVERIFY(!mw->status_view->toPlainText().contains("Firmware update completed."));
}

void MainWindowTest::cancelDuringWriteAsksBeforeStopping() {
  // A write's input is already closed, so Cancel can only stop minipro, and that needs a
  // confirmed warning every time.
  qputenv("FAKE_MINIPRO_DELAY", "10");
  auto mw = makeWindow();
  mw->targets.device->setCurrentText("AT28C256");
  QList<bool> answers{true, false, true};  // write: yes; first force-stop: no; second: yes
  QStringList titles;
  mw->confirmations.device_change = [&answers, &titles](const QString &title, const QString &) {
    titles << title;
    return answers.takeFirst();
  };
  mw->write_file_to_device("/images/fw.bin");
  QVERIFY(mw->async_process != nullptr);

  mw->buttons.cancel->click();  // declined
  QCOMPARE(titles.size(), 2);
  QCOMPARE(titles.at(1), QString("Force-stop minipro?"));
  QTest::qWait(300);
  QVERIFY(mw->async_process != nullptr);
  QVERIFY(!mw->status_view->toPlainText().contains("[Cancel]"));

  QElapsedTimer timer;
  timer.start();
  mw->buttons.cancel->click();  // confirmed
  QTRY_VERIFY(mw->async_process == nullptr);
  QVERIFY2(timer.elapsed() < 3000, qPrintable(QString("took %1 ms").arg(timer.elapsed())));
  QVERIFY(mw->status_view->toPlainText().contains("[Cancel]: stopping minipro"));
  QVERIFY(mw->status_view->toPlainText().contains("[Cancelled]: minipro was stopped"));
}

void MainWindowTest::cancelWhenOperationEndsDuringDialogIsSafe() {
  // Regression (code review): if minipro finished while the force-stop dialog was open, Yes
  // called kill() on the deleted process.
  qputenv("FAKE_MINIPRO_DELAY", "1");
  auto mw = makeWindow();
  mw->targets.device->setCurrentText("AT28C256");
  int asked = 0;
  mw->confirmations.device_change = [&asked, &mw](const QString &, const QString &) {
    if (++asked == 1) return true;  // the write itself
    // Keep the "dialog" open until the write has finished, then answer Yes.
    QElapsedTimer waited;
    waited.start();
    while (mw->async_process != nullptr && waited.elapsed() < 5000) QTest::qWait(50);
    return true;
  };
  mw->write_file_to_device("/images/fw.bin");
  mw->buttons.cancel->click();
  QCOMPARE(asked, 2);
  QVERIFY(mw->async_process == nullptr);
  QVERIFY(!mw->status_view->toPlainText().contains("[Cancel]: stopping minipro"));
  QVERIFY(!mw->status_view->toPlainText().contains("[Cancelled]"));  // it finished normally
}

void MainWindowTest::cancelDuringFirmwareFlashAsksBeforeStopping() {
  // Once the user has confirmed the update, minipro may be flashing: Cancel must ask.
  qputenv("FAKE_MINIPRO_DELAY", "10");
  auto mw = makeWindow();
  mw->confirmations.firmware_update = [](const QString &) { return true; };
  int asked = 0;
  bool stop = false;
  mw->confirmations.device_change = [&asked, &stop](const QString &, const QString &) {
    asked++;
    return stop;
  };
  mw->update_firmware_from_file("/images/update.dat");
  QTRY_VERIFY(mw->status_view->toPlainText().contains("[Input]: y"));
  mw->buttons.cancel->click();
  QCOMPARE(asked, 1);
  QTest::qWait(300);
  QVERIFY(mw->async_process != nullptr);

  stop = true;  // confirm the force-stop this time
  mw->buttons.cancel->click();
  QCOMPARE(asked, 2);
  QTRY_VERIFY(mw->async_process == nullptr);
  QVERIFY(mw->status_view->toPlainText().contains("[Cancelled]: minipro was stopped"));
}

void MainWindowTest::cancelHardwareCheckAsks() {
  // A hardware check drives the programmer's pins, so it isn't stopped without asking.
  qputenv("FAKE_MINIPRO_DELAY", "10");
  auto mw = makeWindow();
  int asked = 0;
  bool stop = false;
  mw->confirmations.device_change = [&asked, &stop](const QString &, const QString &) {
    asked++;
    return stop;
  };
  mw->actions.hardware_check->setChecked(true);
  mw->run_command();
  mw->buttons.cancel->click();
  QCOMPARE(asked, 1);
  QTest::qWait(300);
  QVERIFY(mw->async_process != nullptr);

  stop = true;  // confirm the force-stop this time
  mw->buttons.cancel->click();
  QCOMPARE(asked, 2);
  QTRY_VERIFY(mw->async_process == nullptr);
  QVERIFY(mw->status_view->toPlainText().contains("[Cancelled]: minipro was stopped"));
}

QTEST_MAIN(MainWindowTest)
#include "MainWindowTest.moc"
