#include "../src/MainWindow.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QScreen>
#include <QMessageBox>
#include <QFileDialog>
#include <QTimer>
#include <functional>
#include <csignal>
#include <memory>

// MainWindow drives the `minipro` CLI found on PATH. These tests put the fake_minipro program
// (tests/fake_minipro.cpp) on PATH as "minipro", so they are deterministic and need neither the
// real CLI nor a programmer. FAKE_MINIPRO_SOURCE is defined by tests/CMakeLists.txt.

namespace {
// The exit code QProcess reports when kill() stopped the process: the signal number on Unix,
// and on Windows the code Qt's TerminateProcess() call uses.
#ifdef Q_OS_WIN
constexpr int killedExitCode = 0xf291;
constexpr auto fakeFileName = "minipro.exe";
#else
constexpr int killedExitCode = SIGKILL;
constexpr auto fakeFileName = "minipro";
#endif

// Runs handle(dialog) once a modal dialog is open, from inside the dialog's own event loop: the
// code under test blocks in exec() until handle closes it. Gives up after 10 s, so a dialog that
// never opens can't leave a handler waiting for the next test's.
void whenModalOpens(const std::function<void(QWidget *)> &handle) {
  auto *timer = new QTimer;
  auto *waited = new QElapsedTimer;
  waited->start();
  QObject::connect(timer, &QTimer::timeout, [timer, waited, handle] {
    QWidget *modal = QApplication::activeModalWidget();
    const bool open = modal && modal->isVisible();
    if (!open && waited->elapsed() < 10000) return;
    timer->stop();
    timer->deleteLater();
    delete waited;
    if (open) handle(modal);
  });
  timer->start(10);
}

// Presses a QMessageBox's button: its default one if which is NoButton. Records the box's text
// and whether No was its default, then closes anything else.
void answerMessageBox(QWidget *dialog, QMessageBox::StandardButton which, QString *text, bool *defaultIsNo) {
  auto *box = qobject_cast<QMessageBox *>(dialog);
  if (!box) {
    dialog->close();
    return;
  }
  *text = box->text();  // not the title: on macOS, QMessageBox shows none
  *defaultIsNo = box->defaultButton() == box->button(QMessageBox::No);
  QAbstractButton *button = which == QMessageBox::NoButton ? box->defaultButton() : box->button(which);
  if (button) {
    button->click();
  } else {
    box->reject();
  }
}

// Picks file in a QFileDialog, or cancels it if file is empty. Types the path into the dialog's
// file name field, as a user can: selectFile() does nothing until the dialog has listed the folder.
void answerFileDialog(QWidget *dialog, const QString &file, bool *opened) {
  auto *fileDialog = qobject_cast<QFileDialog *>(dialog);
  auto *nameField = fileDialog ? fileDialog->findChild<QLineEdit *>("fileNameEdit") : nullptr;
  *opened = nameField != nullptr;
  if (nameField && !file.isEmpty()) {
    nameField->setText(QDir::toNativeSeparators(file));
    // Through QDialog: QFileDialog::accept() is protected, but it is the override that runs.
    static_cast<QDialog *>(fileDialog)->accept();
  } else if (auto *any = qobject_cast<QDialog *>(dialog)) {
    any->reject();
  } else {
    dialog->close();
  }
}
}  // namespace

class MainWindowTest : public QObject {
 Q_OBJECT

 private slots:
  void initTestCase();
  void init();
  void cleanup();

  // Startup / discovery
  void detectsMinipro();
  void windowHasAppIcon();
  void defaultSizeFitsTheScreen();
  void versionParsing_data();
  void versionParsing();
  void oddDeviceInfoOutput_data();
  void oddDeviceInfoOutput();
  void missingMiniproDisablesUi();
  void miniproInWorkingDirectoryIsNotRun();
  void miniproFoundInFallbackFolder();
  void detectsProgrammer();
  void missingProgrammerDisablesUi();
  void presenceCheckParsing_data();
  void presenceCheckParsing();
  void presenceCheckDoesNotDuplicateProgrammer();
  void queryPromptDoesNotHang();
  void slowQueryIsStopped();
  void startFailureIsReportedNotParsed();
  void queryReportsMiniproThatCannotRun();
  void deviceListSkipsSerialCodeBanner();
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
  void readReportsMiniproThatCannotRun();
  void readWithoutTempFolderIsRefused();

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
  void cancelStopsARunningRead();
  void cancelKillsMiniproThatIgnoresTerm();
  void cancelAnswersAnUnknownPrompt();
  void cancelDuringWriteAsksBeforeStopping();
  void cancelWhenOperationEndsDuringDialogIsSafe();
  void cancelDuringFirmwareFlashAsksBeforeStopping();
  void cancelHardwareCheckAsks();

  // Confirmations before changing the chip
  void writeAsksAndCanBeCancelled();
  void eraseAsksAndCanBeCancelled();
  void checksDoNotAsk();
  void firmwareDialogDefaultsToNo();
  void deviceChangeDialogDefaultsToNo();
  void writeAsksForAFile();
  void updateFirmwareAsksForAFile();

 private:
  using WindowPtr = std::unique_ptr<MainWindow>;
  WindowPtr makeWindow();
  QStringList loggedCalls() const;
  static void readFfRow(MainWindow &mw);

  QTemporaryDir tempDir;
  QString fakeBinDir;
  QString emptyBinDir;
  QString brokenBinDir;  // a "minipro" that is executable but not a program
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
  QVERIFY(root.mkpath("broken"));
  brokenBinDir = root.filePath("broken");
  QFile broken(brokenBinDir + "/" + fakeFileName);
  QVERIFY(broken.open(QFile::WriteOnly));
  broken.write("not a program\n");
  broken.close();
  QVERIFY(broken.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
  logPath = root.filePath("calls.log");

  const QString fake = fakeBinDir + "/" + fakeFileName;
  QVERIFY2(QFile::copy(FAKE_MINIPRO_SOURCE, fake), FAKE_MINIPRO_SOURCE);
  QVERIFY(QFile::setPermissions(fake, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

  originalPath = qgetenv("PATH");
  // Keep anything written to the working directory out of the source tree.
  QVERIFY(QDir::setCurrent(root.filePath("work")));
}

void MainWindowTest::init() {
  qputenv("PATH", QDir::toNativeSeparators(fakeBinDir).toLocal8Bit() + QDir::listSeparator().toLatin1() +
                      originalPath);
  qputenv("FAKE_MINIPRO_LOG", logPath.toLocal8Bit());
  qunsetenv("FAKE_MINIPRO_NO_PROGRAMMER");
  qunsetenv("FAKE_MINIPRO_PRESENCE");
  qunsetenv("FAKE_MINIPRO_INFO_PROMPT");
  qunsetenv("FAKE_MINIPRO_INFO_SLEEP");
  qunsetenv("FAKE_MINIPRO_INFO_FAIL");
  qunsetenv("FAKE_MINIPRO_CRLF");
  qunsetenv("FAKE_MINIPRO_LIST_EMPTY");
  qunsetenv("FAKE_MINIPRO_LIST_BANNER");
  qunsetenv("FAKE_MINIPRO_TL866A");
  qunsetenv("FAKE_MINIPRO_FW_PROMPT_UNKNOWN");
  qunsetenv("FAKE_MINIPRO_IGNORE_TERM");
  qunsetenv("FAKE_MINIPRO_VERSION_TEXT");
  qunsetenv("FAKE_MINIPRO_INFO_FILE");
  MainWindow::sync_timeout_ms = 10000;
  // Never fall back to the folders a real minipro is installed in.
  MainWindow::minipro_fallback_dirs.clear();
  qunsetenv("FAKE_MINIPRO_READ_HEX");
  qunsetenv("FAKE_MINIPRO_READ_EXIT");
  qunsetenv("FAKE_MINIPRO_READ_EMPTY");
  qunsetenv("FAKE_MINIPRO_READ_SIZE");
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

void MainWindowTest::miniproInWorkingDirectoryIsNotRun() {
  // Regression (security audit): started by bare name, Windows searched the current directory
  // before PATH, so a minipro.exe planted there ran at startup and for every operation. Only
  // PATH may be searched. (Unix never searched the current directory; this guards Windows.)
  auto mw = makeWindow();  // the fake on PATH: a device is selected and Read is enabled
  const QString planted = QDir::current().filePath(fakeFileName);
  QVERIFY(QFile::copy(FAKE_MINIPRO_SOURCE, planted));
  QVERIFY(QFile::setPermissions(planted, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
  qputenv("PATH", emptyBinDir.toLocal8Bit());
  const auto callsBefore = loggedCalls().size();

  mw->read_device();
  QVERIFY(mw->status_view->toPlainText().contains("[Error]: could not start minipro: it was not found (searched PATH)"));
  QVERIFY(mw->async_process == nullptr);
  QVERIFY(mw->buttons.read->isEnabled());

  auto second = makeWindow();  // startup: --version
  QVERIFY(!second->minipro_found);
  QCOMPARE(second->window->windowTitle(), QString("minipro CLI not found!"));

  QVERIFY(QFile::remove(planted));
  QCOMPARE(loggedCalls().size(), callsBefore);  // the planted copy never ran
}

void MainWindowTest::miniproFoundInFallbackFolder() {
  // Regression: opened from the Finder, the app gets launchd's PATH without /opt/homebrew/bin, so
  // a Homebrew minipro wasn't found. The fake's folder stands in for Homebrew's here.
  qputenv("PATH", emptyBinDir.toLocal8Bit());
  MainWindow::minipro_fallback_dirs = {emptyBinDir, fakeBinDir};
  auto mw = makeWindow();
  QVERIFY(mw->minipro_found);
  QVERIFY(loggedCalls().contains("--version"));

  // Still searched only after PATH, and never the current directory.
  MainWindow::minipro_fallback_dirs = {emptyBinDir};
  auto missing = makeWindow();
  QVERIFY(!missing->minipro_found);
  QVERIFY(missing->status_view->toPlainText().contains(
      "[Error]: could not start minipro: it was not found (searched PATH, " + emptyBinDir + ")"));
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

void MainWindowTest::readReportsMiniproThatCannotRun() {
#ifdef Q_OS_WIN
  // On the Windows runner, starting this non-program hung until QtTest's 300 s watchdog; the
  // cause isn't confirmed (probably a system dialog about an unsupported program). The code
  // under test is the same on every platform.
  QSKIP("Windows can block on starting an executable that isn't a program");
#endif
  // Found on PATH, but the system can't run it: QProcess reports FailedToStart, not finished().
  auto mw = makeWindow();
  qputenv("PATH", QDir::toNativeSeparators(brokenBinDir).toLocal8Bit());
  mw->read_device();
  QTRY_VERIFY(mw->async_process == nullptr);
  QVERIFY(mw->status_view->toPlainText().contains("[Error]: could not start minipro: "));
  QVERIFY(!mw->status_view->toPlainText().contains("it was not found"));
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
  QVERIFY(mw->buttons.read->isEnabled());
}

void MainWindowTest::readWithoutTempFolderIsRefused() {
  auto mw = makeWindow();
  // A parent folder that doesn't exist: the private folder for the read can't be created.
  mw->read_dir = QTemporaryDir(QDir(emptyBinDir).filePath("missing/read-XXXXXX"));
  QVERIFY(!mw->read_dir.isValid());
  mw->read_device();
  QVERIFY(mw->status_view->toPlainText().contains("[Error]: no temporary folder for the read: "));
  QVERIFY(mw->async_process == nullptr);
  QVERIFY(loggedCalls().filter(" -r ").isEmpty());
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

void MainWindowTest::queryReportsMiniproThatCannotRun() {
#ifdef Q_OS_WIN
  // On the Windows runner, starting this non-program hung until QtTest's 300 s watchdog; the
  // cause isn't confirmed (probably a system dialog about an unsupported program). The code
  // under test is the same on every platform.
  QSKIP("Windows can block on starting an executable that isn't a program");
#endif
  auto mw = makeWindow();
  qputenv("PATH", QDir::toNativeSeparators(brokenBinDir).toLocal8Bit());
  QPlainTextEdit pane;
  QCOMPARE(MainWindow::run_process(pane, {"--version"}, "stderr"), QString());
  QVERIFY(pane.toPlainText().contains("[Error]: could not start minipro: "));
  QVERIFY(!pane.toPlainText().contains("it was not found"));
}

void MainWindowTest::deviceListSkipsSerialCodeBanner() {
  // minipro prints its programmer and serial code lines to stderr, but if they reach stdout they
  // must not become device names, and \e[K must not end up in one.
  qputenv("FAKE_MINIPRO_LIST_BANNER", "1");
  auto mw = makeWindow();
  QCOMPARE(mw->devices_list, (QStringList{"27C256", "AT28C256", "W27C512"}));
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
#ifndef Q_OS_WIN
  // On Windows, Qt doesn't read ACLs by default and reports these bits from the read-only flag;
  // there the folder is private because it's in the user's own temp folder.
  QCOMPARE(QFileInfo(read.absolutePath()).permissions() & (QFile::ReadGroup | QFile::ReadOther |
                                                           QFile::WriteGroup | QFile::WriteOther),
           QFile::Permissions());
#endif

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
  QTest::newRow("fields missing") << "Name: X28C64\nPackage: DIP28\n" << "X28C64" << "";
  QTest::newRow("unknown lines and order")
      << "Something new: 1\nMemory: 8192 Bytes\nName: X28C64\nVPP: 12V\n" << "X28C64" << "8192 Bytes";
  QTest::newRow("no fields") << "Unexpected output\n" << "" << "";
  const QString esc(QChar(0x1b));
  QTest::newRow("escape sequences")
      << esc + "[1mWarning" + esc + "[0m\nName: X28C64" + esc + "[K\nMemory: 8192 Bytes\n" << "X28C64" << "8192 Bytes";
  QTest::newRow("very long line")
      << "Name: X28C64\nMemory: " + QString(100000, 'A') + "\n" << "X28C64" << QString(100000, 'A');
}

void MainWindowTest::oddDeviceInfoOutput() {
  // minipro's output changes between versions; parsing must never crash or show stale data.
  QFETCH(QString, output);
  QFETCH(QString, name);
  QFETCH(QString, memory);
  auto mw = makeWindow();
  // Through a file: Windows limits an environment variable to 32,767 characters.
  const QString infoFile = QDir(tempDir.path()).filePath("info.txt");
  QFile info(infoFile);
  QVERIFY(info.open(QFile::WriteOnly | QFile::Truncate));
  info.write(output.toUtf8());
  info.close();
  qputenv("FAKE_MINIPRO_INFO_FILE", infoFile.toLocal8Bit());
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

void MainWindowTest::firmwareDialogDefaultsToNo() {
  MainWindow mw;  // the real confirmations, not makeWindow()'s
  QString shown;
  bool defaultIsNo = false;
  whenModalOpens([&](QWidget *dialog) { answerMessageBox(dialog, QMessageBox::NoButton, &shown, &defaultIsNo); });
  QVERIFY(!mw.confirmations.firmware_update("Found TL866II+ 04.2.132 (0x284)\n"));
  QVERIFY(defaultIsNo);
  QVERIFY2(shown.contains("Found TL866II+ 04.2.132 (0x284)"), qPrintable(shown));

  whenModalOpens([&](QWidget *dialog) { answerMessageBox(dialog, QMessageBox::Yes, &shown, &defaultIsNo); });
  QVERIFY(mw.confirmations.firmware_update("Found TL866II+ 04.2.132 (0x284)\n"));
}

void MainWindowTest::deviceChangeDialogDefaultsToNo() {
  MainWindow mw;  // the real confirmations, not makeWindow()'s
  QString shown;
  bool defaultIsNo = false;
  whenModalOpens([&](QWidget *dialog) { answerMessageBox(dialog, QMessageBox::NoButton, &shown, &defaultIsNo); });
  QVERIFY(!mw.confirmations.device_change("Erase AT28C256?", "This erases the AT28C256."));
  QVERIFY(defaultIsNo);
  QCOMPARE(shown, QString("This erases the AT28C256."));

  whenModalOpens([&](QWidget *dialog) { answerMessageBox(dialog, QMessageBox::Yes, &shown, &defaultIsNo); });
  QVERIFY(mw.confirmations.device_change("Erase AT28C256?", "This erases the AT28C256."));
}

void MainWindowTest::writeAsksForAFile() {
  auto mw = makeWindow();
  mw->targets.device->setCurrentText("AT28C256");
  int asked = 0;
  mw->confirmations.device_change = [&asked](const QString &, const QString &) {
    asked++;
    return true;
  };
  // Cancelling the file dialog writes nothing and asks nothing.
  bool opened = false;
  whenModalOpens([&](QWidget *dialog) { answerFileDialog(dialog, {}, &opened); });
  mw->write_device();
  QVERIFY(opened);
  QCOMPARE(asked, 0);
  QVERIFY(mw->async_process == nullptr);
  QVERIFY(loggedCalls().filter(" -w ").isEmpty());

  // Choosing a file writes it, once confirmed.
  const QString image = QDir(tempDir.path()).filePath("image.dat");
  QFile file(image);
  QVERIFY(file.open(QFile::WriteOnly));
  file.close();
  opened = false;
  whenModalOpens([&](QWidget *dialog) { answerFileDialog(dialog, image, &opened); });
  mw->write_device();
  QVERIFY(opened);
  QCOMPARE(asked, 1);
  QTRY_VERIFY(mw->async_process == nullptr);
  const QStringList writes = loggedCalls().filter(" -w ");
  QCOMPARE(writes.size(), 1);
  QVERIFY2(writes.first().startsWith("-p AT28C256 -w "), qPrintable(writes.first()));
  // The dialog may name the file by another path to it (macOS's /var is /private/var).
  QCOMPARE(QFileInfo(writes.first().mid(QString("-p AT28C256 -w ").size())).canonicalFilePath(),
           QFileInfo(image).canonicalFilePath());
}

void MainWindowTest::updateFirmwareAsksForAFile() {
  auto mw = makeWindow();
  // Cancelling the file dialog starts nothing.
  bool opened = false;
  whenModalOpens([&](QWidget *dialog) { answerFileDialog(dialog, {}, &opened); });
  mw->update_firmware();
  QVERIFY(opened);
  QVERIFY(mw->async_process == nullptr);
  QVERIFY(loggedCalls().filter(QRegularExpression("^-F ")).isEmpty());

  // Choosing a file starts minipro -F with it; makeWindow() declines the flash at its prompt.
  const QString update = QDir(tempDir.path()).filePath("update.dat");
  QFile file(update);
  QVERIFY(file.open(QFile::WriteOnly));
  file.close();
  opened = false;
  whenModalOpens([&](QWidget *dialog) { answerFileDialog(dialog, update, &opened); });
  mw->update_firmware();
  QVERIFY(opened);
  QTRY_VERIFY(mw->async_process == nullptr);
  const QStringList updates = loggedCalls().filter(QRegularExpression("^-F "));
  QCOMPARE(updates.size(), 1);
  QCOMPARE(QFileInfo(updates.first().mid(3)).canonicalFilePath(), QFileInfo(update).canonicalFilePath());
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
  // A read changes nothing on the chip, so one click stops it at once. This click comes while
  // the process is still starting. Regression (CI): a SIGTERM sent then could be lost before
  // the child had become minipro, and only the kill() 3 s later stopped it.
  qputenv("FAKE_MINIPRO_DELAY", "10");
  qputenv("FAKE_MINIPRO_READ_HEX", QByteArray("ff").repeated(16));
  auto mw = makeWindow();
  mw->read_device();
#ifndef Q_OS_WIN
  // (On Windows, QProcess::start() returns with the process already running.)
  QCOMPARE(mw->async_process->state(), QProcess::Starting);
#endif
  QSignalSpy finished(mw->async_process, &QProcess::finished);
  mw->buttons.cancel->click();
  QTRY_VERIFY(mw->async_process == nullptr);
  // Stopped with kill(), which can't be lost; Qt reports SIGKILL as the exit code. (This
  // replaces a time limit, which the 3 s fallback kill() could meet by chance under load.)
  QCOMPARE(finished.count(), 1);
  QCOMPARE(finished.at(0).at(0).toInt(), killedExitCode);
  QVERIFY(mw->status_view->toPlainText().contains("[Cancelled]: minipro was stopped"));
  QVERIFY(!mw->status_view->toPlainText().contains("[Error]: minipro crashed"));
  QCOMPARE(mw->hexViewModel.index(0, 0).data().toString(), QString("--"));
  QVERIFY(mw->buttons.read->isEnabled());
}

void MainWindowTest::cancelStopsARunningRead() {
  // Once minipro is running, Cancel asks it to stop with SIGTERM before any kill(). On Windows,
  // where a console program can't be asked, it is killed at once.
  qputenv("FAKE_MINIPRO_DELAY", "10");
  auto mw = makeWindow();
  mw->read_device();
  QTRY_COMPARE(mw->async_process->state(), QProcess::Running);
  QSignalSpy finished(mw->async_process, &QProcess::finished);
  mw->buttons.cancel->click();
  QTRY_VERIFY(mw->async_process == nullptr);
  QCOMPARE(finished.count(), 1);
#ifdef Q_OS_WIN
  QCOMPARE(finished.at(0).at(0).toInt(), killedExitCode);
#else
  QCOMPARE(finished.at(0).at(0).toInt(), SIGTERM);  // asked politely, no kill() needed
#endif
  QVERIFY(mw->status_view->toPlainText().contains("[Cancelled]: minipro was stopped"));
}

void MainWindowTest::cancelKillsMiniproThatIgnoresTerm() {
  // If minipro ignores SIGTERM, Cancel still stops it: kill() follows after 3 s.
#ifdef Q_OS_WIN
  QSKIP("Windows has no SIGTERM: Cancel kills at once (cancelStopsARunningRead)");
#endif
  qputenv("FAKE_MINIPRO_DELAY", "30");
  qputenv("FAKE_MINIPRO_IGNORE_TERM", "1");
  auto mw = makeWindow();
  mw->read_device();
  QTRY_COMPARE(mw->async_process->state(), QProcess::Running);
  QTest::qWait(200);  // let the fake reach its trap
  QSignalSpy finished(mw->async_process, &QProcess::finished);
  mw->buttons.cancel->click();
  QTest::qWait(1000);
  QVERIFY(mw->async_process != nullptr);  // SIGTERM was ignored
  QTRY_VERIFY_WITH_TIMEOUT(mw->async_process == nullptr, 5000);
  // SIGKILL proves the kill() fallback stopped it. (No lower bound on the time: Qt's coarse
  // timers may fire up to 5% early.)
  QCOMPARE(finished.count(), 1);
  QCOMPARE(finished.at(0).at(0).toInt(), killedExitCode);
  QVERIFY(mw->status_view->toPlainText().contains("[Cancelled]: minipro was stopped"));
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
  QList answers{true, false, true};  // write: yes; first force-stop: no; second: yes
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

void MainWindowTest::windowHasAppIcon() {
  // Regression: ":/res/AppIcon.png" was used but no resource file was compiled in, so the
  // window had no icon.
  QVERIFY(QFile::exists(":/res/AppIcon.png"));
  auto mw = makeWindow();
  QVERIFY(!mw->window->windowIcon().pixmap(64, 64).isNull());
}

void MainWindowTest::defaultSizeFitsTheScreen() {
  // Regression (code review): the window opened at 1100x660 even on smaller screens, with its
  // right-hand panels off-screen. The offscreen platform's screen is 800x600.
  auto mw = makeWindow();
  const QSize available = mw->window->screen()->availableGeometry().size();
  QVERIFY2(mw->window->width() <= available.width() && mw->window->height() <= available.height(),
           qPrintable(QString("window %1x%2, screen %3x%4")
                          .arg(mw->window->width()).arg(mw->window->height())
                          .arg(available.width()).arg(available.height())));
}

QTEST_MAIN(MainWindowTest)
#include "MainWindowTest.moc"
