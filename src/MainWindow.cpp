#include "MainWindow.h"
#include <QStandardPaths>
#include <QMessageBox>
#include <QTimer>

namespace {
// minipro's stdin prompts for -F, and the answers for "go ahead" and "cancel".
struct FirmwarePromptKind {
  const char *text;
  const char *go_ahead;
  const char *cancel;
};
const std::array<FirmwarePromptKind, 2> firmware_prompts{{
    // TL866II+, T48, T56, T76 (tl866iiplus.c, t48.c, t56.c, t76.c): y/n.
    {"Do you want to continue with firmware update?", "y", "n"},
    // TL866A/CS (tl866a.c): "1) Device default 2) <the other variant> 3) Exit". Going ahead
    // reflashes the programmer's own variant; switching between A and CS is never offered.
    {"Which firmware version do you want to reflash?", "1", "3"},
}};
// The "erase to end of line" terminal code minipro prints with its progress output.
const QString erase_to_end_of_line = QString(QChar(0x1B)) + "[K";
}  // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  initializer();
  auto *layout = new QGridLayout(window.get());

  auto *groupBox = new QGroupBox(tr("Targets"));
  groupBox->setAlignment(Qt::AlignTop);
  auto *vbox = new QGridLayout;
  vbox->addWidget(targets.programmer, 0, 0, Qt::AlignTop);
  connect(targets.programmer, SIGNAL (highlighted(int)), this, SLOT (check_for_programmer()));
  vbox->addWidget(buttons.update, 0, 1);
  connect(buttons.update, SIGNAL (released()), this, SLOT (update_firmware()));
  vbox->addWidget(targets.device, 1, 0, 1, 0);
  connect(targets.device, SIGNAL (currentTextChanged(QString)), this, SLOT (select_device(QString)));
  groupBox->setLayout(vbox);
  layout->addWidget(groupBox, 0, 0);

  auto *groupBox3 = new QGroupBox(tr("Actions"));
  groupBox3->setAlignment(Qt::AlignTop);
  auto *vbox3 = new QGridLayout;
  vbox3->addWidget(options.pin_check, 0, 0, Qt::AlignTop);
  vbox3->addWidget(actions.blank_check, 0, 1);
  vbox3->addWidget(actions.erase, 1, 0, Qt::AlignTop);
  vbox3->addWidget(actions.hardware_check, 1, 1);
  vbox3->addWidget(buttons.run_command, 2, 0, 1, 0);
  connect(buttons.run_command, SIGNAL (released()), this, SLOT (run_command()));
  vbox3->addWidget(options.no_id_error, 4, 0);
  vbox3->addWidget(options.skip_id, 5, 0);
  vbox3->addWidget(options.no_size_error, 4, 1);
  vbox3->addWidget(options.skip_verify, 5, 1);
  vbox3->addWidget(buttons.read, 6, 0);
  connect(buttons.read, SIGNAL (released()), this, SLOT (read_device()));
  vbox3->addWidget(buttons.write, 6, 1);
  connect(buttons.write, SIGNAL (released()), this, SLOT (write_device()));
  vbox3->addWidget(buttons.cancel, 7, 0, 1, 0);
  connect(buttons.cancel, SIGNAL (released()), this, SLOT (cancel_operation()));
  groupBox3->setLayout(vbox3);
  layout->addWidget(groupBox3, 2, 0);

  auto *groupBox4 = new QGroupBox(tr("Device Info"));
  auto *vbox4 = new QGridLayout;
  vbox4->addWidget(new QLabel(tr("Name")), 0, 0);
  vbox4->addWidget(device_info.name, 0, 1);
  vbox4->addWidget(new QLabel(tr("Memory")), 1, 0);
  vbox4->addWidget(device_info.memory, 1, 1);
  vbox4->addWidget(new QLabel(tr("Package")), 2, 0);
  vbox4->addWidget(device_info.package, 2, 1);

  vbox4->addWidget(new QLabel(tr("Protocol")), 0, 2);
  vbox4->addWidget(device_info.protocol, 0, 3);
  vbox4->addWidget(new QLabel(tr("Read Buffer")), 1, 2);
  vbox4->addWidget(device_info.read_buffer, 1, 3);
  vbox4->addWidget(new QLabel(tr("Write Buffer")), 2, 2);
  vbox4->addWidget(device_info.write_buffer, 2, 3);
  groupBox4->setLayout(vbox4);
  layout->addWidget(groupBox4, 0, 1);

  auto *groupBox5 = new QGroupBox(tr("Hex Viewer"));
  auto *vbox5 = new QVBoxLayout;

  hexTableView->setModel(&hexViewModel);
  hexTableView->show();
  vbox5->addWidget(hexTableView);
  vbox5->addStretch(1);
  groupBox5->setLayout(vbox5);
  layout->addWidget(groupBox5, 2, 1);

  auto *groupBox6 = new QGroupBox(tr("Output"));
  auto *vbox6 = new QVBoxLayout;
  vbox6->addWidget(status_view);
  vbox6->addStretch(1);
  groupBox6->setLayout(vbox6);
  layout->addWidget(groupBox6, 3, 0, 1, 0);

  status_view->setReadOnly(true);

  update_controls();
  check_for_minipro();
  build_default_hex_output();

  window->setWindowIcon(QIcon(":/res/AppIcon.png"));
  window->show();
}

MainWindow::~MainWindow() {
  // Destroying a running QProcess kills minipro, which could leave a chip half-written or the
  // programmer mid-update. Let the operation finish first. Its input is already closed, so a
  // minipro prompt can't make this wait forever.
  if (async_process != nullptr && async_process->state() != QProcess::NotRunning) {
    std::cerr << "minipro-gui: waiting for the running minipro operation to finish" << std::endl;
    // The widgets may already be gone, so don't let finished() reach the slots that update them.
    async_process->disconnect(this);
    // If a firmware update is still waiting at its y/n prompt, end-of-input answers "no".
    async_process->closeWriteChannel();
    async_process->waitForFinished(-1);
  }
}

void MainWindow::initializer() {
  confirmations.firmware_update = [this](const QString &minipro_output) {
    return QMessageBox::question(window.get(), tr("Update programmer firmware?"),
                                 tr("minipro is ready to update the programmer's firmware:\n\n%1\n\n"
                                    "Don't unplug the programmer until the update has finished.")
                                     .arg(minipro_output.trimmed()),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
  };
  confirmations.device_change = [this](const QString &title, const QString &text) {
    return QMessageBox::warning(window.get(), title, text, QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No) == QMessageBox::Yes;
  };
  minipro_found = false;
  programmer_found = false;

  window = std::make_unique<QWidget>();
  window->setWindowTitle("minipro CLI not found!");
  window->resize(1000, 600);
  window->setMinimumSize(800, 480);

  targets.programmer = new QComboBox(window.get());
  targets.device = new QComboBox(window.get());

  buttons.run_command = new QPushButton("Run Command", window.get());
  buttons.write = new QPushButton("Write to Device", window.get());
  buttons.read = new QPushButton("Read from Device", window.get());
  buttons.update = new QPushButton("Update Firmware", window.get());
  buttons.cancel = new QPushButton("Cancel", window.get());

  options.no_id_error = new QCheckBox("Ignore ID Error", window.get());
  options.skip_id = new QCheckBox("Skip ID Check", window.get());
  options.no_size_error = new QCheckBox("Ignore Size Error", window.get());
  options.skip_verify = new QCheckBox("Skip Verification", window.get());
  options.pin_check = new QCheckBox("Pin Check", window.get());
  actions.blank_check = new QCheckBox("Blank Check", window.get());
  actions.erase = new QCheckBox("Erase Device", window.get());
  actions.hardware_check = new QCheckBox("Hardware Check", window.get());

  device_info.name = new QLineEdit(window.get());
  device_info.name->setReadOnly(true);
  device_info.memory = new QLineEdit(window.get());
  device_info.memory->setReadOnly(true);
  device_info.package = new QLineEdit(window.get());
  device_info.package->setReadOnly(true);
  device_info.protocol = new QLineEdit(window.get());
  device_info.protocol->setReadOnly(true);
  device_info.read_buffer = new QLineEdit(window.get());
  device_info.read_buffer->setReadOnly(true);
  device_info.write_buffer = new QLineEdit(window.get());
  device_info.write_buffer->setReadOnly(true);

  hexTableView = new QTableView(window.get());
  status_view = new QPlainTextEdit(window.get());

  QFont monospace_font("Courier New");
  monospace_font.setStyleHint(QFont::Monospace);
  status_view->setFont(monospace_font);
}

// The minipro on PATH as an absolute path, or empty if there is none. Started by bare name,
// Windows would search the current directory before PATH, so a minipro.exe planted in the folder
// the app was started from would run instead.
QString MainWindow::minipro_path() {
  return QStandardPaths::findExecutable("minipro");
}

void MainWindow::run_async_process(const QStringList &process_arguments,
                                   AsyncOperation operation) {
  if (refuse_if_operation_running()) return;
  const QString program = minipro_path();
  if (program.isEmpty()) {
    status_view->appendPlainText("[Error]: could not start minipro: it is not on PATH");
    status_view->ensureCursorVisible();
    build_default_hex_output();
    return;
  }
  QString process_arguments_string = "";
  running = RunningOperation{};
  running.kind = operation;
  async_process = new QProcess(this);
  for (auto const &each : process_arguments) {
    process_arguments_string += each + " ";
  }

  status_view->appendPlainText("[Input]: minipro " + process_arguments_string);
  // minipro reports progress and errors on stderr.
  connect(async_process, SIGNAL(readyReadStandardError()), this, SLOT(async_process_err_output()));
  connect(async_process,
          SIGNAL(finished(int,QProcess::ExitStatus)),
          this,
          SLOT(async_process_finished(int,QProcess::ExitStatus)));
  connect(async_process,
          SIGNAL(errorOccurred(QProcess::ProcessError)),
          this,
          SLOT(async_process_error(QProcess::ProcessError)));

  status_view->appendPlainText("[Output]: ");
  status_view->ensureCursorVisible();
  update_controls();  // async_process is set: disables everything that starts minipro
  async_process->start(program, process_arguments);
  // On Windows, start() reports FailedToStart before it returns, and async_process_error() has
  // then already ended the operation and cleared async_process.
  if (async_process == nullptr) return;
  // minipro asks some questions on stdin. A firmware update's y/n prompt is answered from the
  // GUI (answer_firmware_prompt_if_asked). Nothing answers the others, such as which database to
  // use without a programmer, so give them end-of-input: they treat it as "no" and exit.
  if (operation != AsyncOperation::FirmwareUpdate) {
    async_process->closeWriteChannel();
  }
}

void MainWindow::answer_firmware_prompt_if_asked(const QString &new_output) {
  if (running.kind != AsyncOperation::FirmwareUpdate || running.prompt_answered) return;
  running.prompt_output += new_output;
  const FirmwarePromptKind *prompt = nullptr;
  qsizetype prompt_start = -1;
  for (const FirmwarePromptKind &kind : firmware_prompts) {
    if (const qsizetype at = running.prompt_output.indexOf(kind.text); at >= 0) {
      prompt = &kind;
      prompt_start = at;
      break;
    }
  }
  if (prompt == nullptr) return;
  // Set before asking: the dialog runs an event loop, and more output may arrive meanwhile.
  running.prompt_answered = true;
  QProcess *process = async_process;
  // Show what minipro printed before its question (the installed and new versions), not the
  // question or its numbered menu, which don't fit a Yes/No dialog.
  const bool go_ahead = confirmations.firmware_update(running.prompt_output.left(prompt_start).trimmed());
  if (process != async_process) return;  // the operation ended while the dialog was open
  const QString answer = go_ahead ? prompt->go_ahead : prompt->cancel;
  status_view->appendPlainText("[Input]: " + answer);
  process->write((answer + "\n").toLatin1());
  process->closeWriteChannel();
}

// Only one minipro may talk to the programmer at a time. The UI disables every control that
// starts one while an operation runs; this is the backstop for any path that gets through.
bool MainWindow::refuse_if_operation_running() const {
  if (async_process == nullptr) return false;
  status_view->appendPlainText("[Error]: another minipro operation is still running");
  status_view->ensureCursorVisible();
  return true;
}

void MainWindow::end_async_process() {
  async_process->deleteLater();
  async_process = nullptr;
  update_controls();
}

void MainWindow::async_process_err_output() {
  const QString output = QString::fromLocal8Bit(async_process->readAllStandardError());
  status_view->appendPlainText(QString(output).trimmed().replace(erase_to_end_of_line, ""));
  status_view->ensureCursorVisible();
  answer_firmware_prompt_if_asked(output);
}

// Options modify an operation; they are safe to add to Read, Write and Run Command.
QStringList MainWindow::parse_option_checkboxes() const {
  QStringList arguments;
  if (options.no_id_error->isChecked()) arguments.append("--no_id_error");
  if (options.skip_id->isChecked()) arguments.append("--skip_id");
  if (options.no_size_error->isChecked()) arguments.append("--no_size_error");
  if (options.skip_verify->isChecked()) arguments.append("--skip_verify");
  if (options.pin_check->isChecked()) arguments.append("--pin_check");
  return arguments;
}

// Actions are operations of their own in minipro: --blank_check replaces an earlier -r/-w,
// and --hardware_check runs as soon as it is parsed. They are only used by Run Command, never
// added to Read, Write or Update Firmware.
QStringList MainWindow::parse_action_checkboxes() const {
  QStringList arguments;
  if (actions.blank_check->isChecked()) arguments.append("--blank_check");
  if (actions.erase->isChecked()) arguments.append("--erase");
  if (actions.hardware_check->isChecked()) arguments.append("--hardware_check");
  return arguments;
}

int MainWindow::sync_timeout_ms = 10000;

// Runs a quick query and returns its stdout or stderr. Blocks the UI, so only for commands that
// finish in well under a second; device operations go through run_async_process().
QString MainWindow::run_process(QPlainTextEdit &target_plain_text_edit,
                                const QStringList &process_arguments,
                                const QString &type = "stderr") {
  QString process_arguments_string = "";
  for (auto const &each : process_arguments) {
    process_arguments_string += each + " ";
  }
  target_plain_text_edit.appendPlainText("[Input]: minipro " + process_arguments_string);

  const QString program = minipro_path();
  if (program.isEmpty()) {
    target_plain_text_edit.appendPlainText("[Error]: could not start minipro: it is not on PATH");
    target_plain_text_edit.ensureCursorVisible();
    return {};
  }
  QProcess process;
  process.start(program, process_arguments);
  // Without a programmer, minipro asks "Which database do you want to display?" on stdin.
  // End-of-input makes it abort instead of blocking until the timeout.
  process.closeWriteChannel();
  if (!process.waitForStarted(sync_timeout_ms)) {
    target_plain_text_edit.appendPlainText("[Error]: could not start minipro: " + process.errorString());
    target_plain_text_edit.ensureCursorVisible();
    return {};
  }
  if (!process.waitForFinished(sync_timeout_ms)) {
    process.kill();
    process.waitForFinished(1000);
    target_plain_text_edit.appendPlainText(
        QString("[Error]: minipro didn't finish within %1 ms and was stopped").arg(sync_timeout_ms));
    target_plain_text_edit.ensureCursorVisible();
    return {};
  }

  static const QRegularExpression serial_code(R"(Serial code:.*\n([\s\S]*))");
  QString output;
  if (type == "stderr") {
    output = process.readAllStandardError();
    if (QRegularExpressionMatch match = serial_code.match(output); match.hasMatch()) {
      output = match.captured(1).trimmed();
    }
    target_plain_text_edit.appendPlainText("[Output]: " + output.replace(erase_to_end_of_line, ""));
  } else if (type == "stdout") {
    output = process.readAllStandardOutput();
    if (QRegularExpressionMatch match = serial_code.match(output); match.hasMatch()) {
      output = match.captured(1).trimmed().replace(erase_to_end_of_line, "");
    }
  }
  target_plain_text_edit.ensureCursorVisible();
  return output;
}

void MainWindow::check_for_minipro() {
  QStringList arguments;
  arguments << "--version";
  const QString output = run_process(*status_view, arguments, "stderr");
  // minipro's sign-on line, e.g. "minipro version 0.7.4     A free and open TL866 series
  // programmer". Anchored to the start of a line, and it doesn't need a newline after it.
  static const QRegularExpression re("^minipro version .*$", QRegularExpression::MultilineOption);
  if (!output.isEmpty()) {
    QRegularExpressionMatch match = re.match(output);
    if (match.hasMatch()) {
      window->setWindowTitle(match.captured(0).trimmed());
      minipro_found = true;
      check_for_programmer();
      get_devices();
    }
  }
}

void MainWindow::check_for_programmer() {
  if (minipro_found) {
    QStringList arguments;
    arguments << "--presence_check";

    // minipro prints one line, "<id>: <name>" (e.g. "tl866ii: TL866II+", "t48: T48"), or
    // "[No programmer found]" / "[Unknown programmer version]". Accept only that shape: a
    // lowercase id, so "Error: ..." or "Warning: ..." lines are never taken for a programmer.
    static const QRegularExpression re(R"(^[a-z][a-z0-9]*: (\S.*)$)",
                                       QRegularExpression::MultilineOption);
    QRegularExpressionMatch match = re.match(run_process(*status_view, arguments, "stderr"));
    // This runs on every highlight, so refill rather than add duplicates. Block signals while
    // refilling: changing the current item emits highlighted(), which would call this again.
    const QSignalBlocker blocker(targets.programmer);
    targets.programmer->clear();
    if (match.hasMatch()) {
      programmer = match.captured(1).trimmed();
      programmer_found = true;
      targets.programmer->addItem(programmer);
      update_controls();
    } else {
      targets.programmer->setPlaceholderText("No programmer found");
      programmer_found = false;
      update_controls();
    }
  }
}

// Which controls are usable follows from the current state; every change goes through here.
void MainWindow::update_controls() {
  const bool idle = async_process == nullptr;
  // Selecting a programmer or device also runs minipro, so nothing that does is usable while an
  // operation runs.
  targets.programmer->setDisabled(!idle || !minipro_found);
  const bool can_use_programmer = idle && programmer_found;
  // Device operations need a device from minipro's list. If -l returned nothing, `device` is
  // empty and they would send "-p ''". Updating the programmer's firmware needs no device.
  const bool can_use_device = can_use_programmer && device_selected();
  targets.device->setDisabled(!can_use_programmer || devices_list.isEmpty());
  buttons.run_command->setDisabled(!can_use_device);
  buttons.read->setDisabled(!can_use_device);
  buttons.write->setDisabled(!can_use_device);
  buttons.update->setDisabled(!can_use_programmer);
  buttons.cancel->setDisabled(idle);
}

bool MainWindow::device_selected() const {
  return !device.isEmpty() && devices_list.contains(device);
}

// Backstop for the device operations, which the UI disables while no device is selected.
bool MainWindow::refuse_if_no_device() const {
  if (device_selected()) return false;
  status_view->appendPlainText("[Error]: no device selected");
  status_view->ensureCursorVisible();
  return true;
}

void MainWindow::get_devices() {
  if (programmer_found) {
    QStringList arguments;
    arguments << "-l";

    devices_list.clear();
    static const QRegularExpression line_break(R"(\r?\n)");
    for (const QString &line : run_process(*status_view, arguments, "stdout").split(line_break)) {
      // A stray \r or space would become part of the name passed to minipro -p.
      if (const QString name = line.trimmed(); !name.isEmpty()) devices_list << name;
    }
    devices_list.sort();
    devices_list.removeDuplicates();

    targets.device->addItems(devices_list);  // selects the first device, if any
    update_controls();
  }
}

namespace {
// The value of a "Field: value" line in minipro -d output, or an empty string.
QString device_field(const QString &info, const QString &field) {
  const QRegularExpression re("^" + QRegularExpression::escape(field) + R"(: (.*)$)",
                              QRegularExpression::MultilineOption);
  return re.match(info).captured(1).trimmed();
}
}  // namespace

void MainWindow::select_device(const QString &selected_device) {
  if (minipro_found && selected_device != "") {
    device = selected_device;
    // Clear first: if -d fails, the previous device's details must not stay next to the new name.
    for (QLineEdit *field : {device_info.name, device_info.memory, device_info.package, device_info.protocol,
                             device_info.read_buffer, device_info.write_buffer}) {
      field->clear();
    }

    QStringList arguments;
    arguments << "-d" << device;
    const QString info = run_process(*status_view, arguments, "stderr");
    if (device_field(info, "Name").isEmpty()) {
      status_view->appendPlainText("[Error]: no device information for " + device);
      status_view->ensureCursorVisible();
      return;
    }
    device_info.name->setText(device_field(info, "Name"));
    device_info.memory->setText(device_field(info, "Memory"));
    device_info.package->setText(device_field(info, "Package"));
    device_info.protocol->setText(device_field(info, "Protocol"));
    device_info.read_buffer->setText(device_field(info, "Read buffer size"));
    device_info.write_buffer->setText(device_field(info, "Write buffer size"));
    build_default_hex_output();
  }
}

void MainWindow::run_command() {
  if (minipro_found && !refuse_if_operation_running() && !refuse_if_no_device()) {
    const QStringList chosen_actions = parse_action_checkboxes();
    if (chosen_actions.size() != 1) {
      // minipro runs only one action; with several it would silently pick one.
      status_view->appendPlainText(
          "[Error]: Run Command needs exactly one of Blank Check, Erase Device or "
          "Hardware Check to be checked.");
      status_view->ensureCursorVisible();
      return;
    }
    if (chosen_actions.first() == "--erase" &&
        !confirmations.device_change(tr("Erase %1?").arg(device),
                               tr("This erases everything on the %1 in the programmer.").arg(device))) {
      status_view->appendPlainText("[Cancelled]: erase " + device);
      return;
    }
    QStringList arguments;
    arguments << "-p" << device << chosen_actions << parse_option_checkboxes();
    // Asynchronous: an erase or blank check can take minutes on a large chip.
    const QString &action = chosen_actions.first();
    AsyncOperation operation = AsyncOperation::Check;  // blank check
    if (action == "--erase") {
      operation = AsyncOperation::Erase;
    } else if (action == "--hardware_check") {
      operation = AsyncOperation::HardwareCheck;
    }
    run_async_process(arguments, operation);
  }
}

void MainWindow::read_device() {
  if (refuse_if_operation_running() || refuse_if_no_device()) return;
  QStringList arguments;
  if (!read_dir.isValid()) {
    status_view->appendPlainText("[Error]: no temporary folder for the read: " + read_dir.errorString());
    status_view->ensureCursorVisible();
    return;
  }
  arguments << "-p" << device << "-r" << read_file << parse_option_checkboxes();
  // Remove the previous read first, so a read that writes nothing can't show old data.
  QFile::remove(read_file);
  build_default_hex_output();
  run_async_process(arguments, AsyncOperation::Read);
}

void MainWindow::async_process_finished(int code, QProcess::ExitStatus status) {
  const bool succeeded = status == QProcess::NormalExit && code == 0;
  if (!succeeded) {
    if (running.stopped_by_cancel) {
      status_view->appendPlainText("[Cancelled]: minipro was stopped");
    } else {
      status_view->appendPlainText(status == QProcess::CrashExit
                                       ? QString("[Error]: minipro crashed")
                                       : QString("[Error]: minipro exited with code %1").arg(code));
    }
    status_view->ensureCursorVisible();
  }
  if (running.kind == AsyncOperation::Read && succeeded) {
    hexViewModel.clearHexTable();
    build_formatted_hex_output();
  } else if (running.kind == AsyncOperation::Check || running.kind == AsyncOperation::HardwareCheck) {
    // Blank, pin and hardware checks don't change the chip; keep showing the last read.
  } else {
    // A failed read has no data to show, and after a write, erase or firmware update the last
    // read no longer matches the chip.
    build_default_hex_output();
  }
  end_async_process();
}

void MainWindow::async_process_error(QProcess::ProcessError error) {
  // FailedToStart never emits finished(), so report it here; other errors are followed by
  // finished() and reported there.
  if (error == QProcess::FailedToStart) {
    status_view->appendPlainText("[Error]: could not start minipro: " + async_process->errorString());
    status_view->ensureCursorVisible();
    build_default_hex_output();
    end_async_process();
  }
}

// Cancel the running operation.
// - A read or blank check changes nothing, so it is stopped at once.
// - A firmware update waiting at its question is answered "no" first: its input is the only one
//   left open, and closing it is always safe, including for prompts this app doesn't recognise.
// - Otherwise minipro may be changing the chip or the programmer (write, erase, hardware check,
//   or a firmware update already under way), so stopping it needs a confirmed warning.
// Stopping sends SIGTERM first, so minipro can clean up if it handles it, then kills it if it is
// still running after a few seconds. A process that hasn't started running yet is killed at once.
// On Windows it is always killed at once: terminate() only posts WM_CLOSE, which a console
// program like minipro ignores, so waiting would only delay the kill().
void MainWindow::cancel_operation() {
  QProcess *process = async_process;
  if (process == nullptr) return;
  const auto stop = [this, process] {
    running.stopped_by_cancel = true;
    status_view->appendPlainText("[Cancel]: stopping minipro");
    status_view->ensureCursorVisible();
#ifdef Q_OS_WIN
    process->kill();
#else
    if (process->state() == QProcess::Starting) {
      // minipro hasn't started running yet, so there is nothing to clean up. A SIGTERM sent
      // now can reach the child before it has become minipro and be lost, so stop it outright.
      process->kill();
      return;
    }
    process->terminate();
    QTimer::singleShot(3000, process, [process] {
      if (process->state() != QProcess::NotRunning) process->kill();
    });
#endif
  };
  if (running.kind == AsyncOperation::Read || running.kind == AsyncOperation::Check) {
    stop();
    return;
  }
  if (running.kind == AsyncOperation::FirmwareUpdate && !running.prompt_answered) {
    running.prompt_answered = true;
    process->closeWriteChannel();
    status_view->appendPlainText(
        "[Cancel]: answered \"no\" to any question minipro is waiting at. Press Cancel again to "
        "force it to stop.");
    status_view->ensureCursorVisible();
    return;
  }
  const bool confirmed = confirmations.device_change(
      tr("Force-stop minipro?"),
      tr("minipro may be in the middle of changing the chip or the programmer. Stopping it now "
         "can leave the chip partly written or the programmer needing recovery."));
  // The dialog runs an event loop: the operation may have ended (and been deleted) meanwhile.
  if (confirmed && process == async_process) stop();
}

void MainWindow::write_device() {
  if (const QString fileName = QFileDialog::getOpenFileName(window.get()); !fileName.isEmpty()) {
    write_file_to_device(fileName);
  }
}

void MainWindow::write_file_to_device(const QString &file_name) {
  if (refuse_if_operation_running() || refuse_if_no_device()) return;
  if (!confirmations.device_change(tr("Write to %1?").arg(device),
                             tr("This overwrites the contents of the %1 in the programmer with\n\n%2")
                                 .arg(device, QDir::toNativeSeparators(file_name)))) {
    status_view->appendPlainText("[Cancelled]: write to " + device);
    return;
  }
  QStringList arguments;
  arguments << "-p" << device << "-w" << file_name << parse_option_checkboxes();
  run_async_process(arguments, AsyncOperation::Write);
}

void MainWindow::update_firmware() {
  if (const QString fileName = QFileDialog::getOpenFileName(window.get()); !fileName.isEmpty()) {
    update_firmware_from_file(fileName);
  }
}

void MainWindow::update_firmware_from_file(const QString &file_name) {
  // No checkbox flags: minipro handles -F as soon as it is parsed and ignores everything after it.
  QStringList arguments;
  arguments << "-F" << file_name;
  run_async_process(arguments, AsyncOperation::FirmwareUpdate);
}

void MainWindow::build_default_hex_output() {
  hexViewModel.clearHexTable();
  format_hex_table_columns();
}

void MainWindow::build_formatted_hex_output() {
  QFile temp_file(read_file);
  if (!temp_file.open(QFile::ReadOnly)) {
    // minipro reported success but left no readable file.
    status_view->appendPlainText("[Error]: could not open the read result: " + temp_file.errorString());
    status_view->ensureCursorVisible();
    build_default_hex_output();
    return;
  }
  hexViewModel.loadBytes(temp_file.readAll());
  format_hex_table_columns();
}

void MainWindow::format_hex_table_columns() const {
  hexTableView->horizontalHeader()->setStretchLastSection(true);
  for (int n = 0; n < hexViewModel.columnCount(); n++) {
    hexTableView->resizeColumnToContents(n);
  }
  // Every row has the same height. Sizing each row to its contents takes seconds for a large chip
  // (a 16 MiB image has a million rows).
  QHeaderView *rows = hexTableView->verticalHeader();
  rows->setSectionResizeMode(QHeaderView::Fixed);
  rows->setDefaultSectionSize(QFontMetrics(status_view->font()).height() + 8);
}
