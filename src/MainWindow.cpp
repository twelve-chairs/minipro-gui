#include "MainWindow.h"
#include <QMessageBox>

namespace {
// minipro's confirmation prompt for -F (tl866iiplus.c, t48.c, t56.c).
const QString firmware_prompt = "Do you want to continue with firmware update?";
}  // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  initializer();

  auto *groupBox = new QGroupBox(tr("Targets"));
  groupBox->setAlignment(Qt::AlignTop);
  auto *vbox = new QGridLayout;
  vbox->addWidget(combobox_programmer, 0, 0, Qt::AlignTop);
  connect(combobox_programmer, SIGNAL (highlighted(int)), this, SLOT (check_for_programmer()));
  vbox->addWidget(button_update, 0, 1);
  connect(button_update, SIGNAL (released()), this, SLOT (update_firmware()));
  vbox->addWidget(combobox_device, 1, 0, 1, 0);
  connect(combobox_device, SIGNAL (currentTextChanged(QString)), this, SLOT (select_device(QString)));
  groupBox->setLayout(vbox);
  layout->addWidget(groupBox, 0, 0);

  auto *groupBox3 = new QGroupBox(tr("Actions"));
  groupBox3->setAlignment(Qt::AlignTop);
  auto *vbox3 = new QGridLayout;
  vbox3->addWidget(pin_check, 0, 0, Qt::AlignTop);
  vbox3->addWidget(blank_check, 0, 1);
  vbox3->addWidget(erase_device, 1, 0, Qt::AlignTop);
  vbox3->addWidget(hardware_check, 1, 1);
  vbox3->addWidget(button_run_command, 2, 0, 1, 0);
  connect(button_run_command, SIGNAL (released()), this, SLOT (run_command()));
  vbox3->addWidget(no_id_error, 4, 0);
  vbox3->addWidget(skip_id, 5, 0);
  vbox3->addWidget(no_size_error, 4, 1);
  vbox3->addWidget(skip_verify, 5, 1);
  vbox3->addWidget(button_read, 6, 0);
  connect(button_read, SIGNAL (released()), this, SLOT (read_device()));
  vbox3->addWidget(button_write, 6, 1);
  connect(button_write, SIGNAL (released()), this, SLOT (write_device()));
  groupBox3->setLayout(vbox3);
  layout->addWidget(groupBox3, 2, 0);

  auto *groupBox4 = new QGroupBox(tr("Device Info"));
  auto *vbox4 = new QGridLayout;
  vbox4->addWidget(device_name_label, 0, 0);
  vbox4->addWidget(device_name, 0, 1);
  vbox4->addWidget(device_memory_label, 1, 0);
  vbox4->addWidget(device_memory, 1, 1);
  vbox4->addWidget(device_package_label, 2, 0);
  vbox4->addWidget(device_package, 2, 1);

  vbox4->addWidget(device_protocol_label, 0, 2);
  vbox4->addWidget(device_protocol, 0, 3);
  vbox4->addWidget(device_readbuffer_label, 1, 2);
  vbox4->addWidget(device_readbuffer, 1, 3);
  vbox4->addWidget(device_writebuffer_label, 2, 2);
  vbox4->addWidget(device_writebuffer, 2, 3);
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

  disable_buttons();
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
  confirm_firmware_update = [this](const QString &minipro_output) {
    return QMessageBox::question(window, tr("Update programmer firmware?"),
                                 tr("minipro is ready to update the programmer's firmware:\n\n%1\n\n"
                                    "Don't unplug the programmer until the update has finished.")
                                     .arg(minipro_output.trimmed()),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
  };
  confirm_device_change = [this](const QString &title, const QString &text) {
    return QMessageBox::warning(window, title, text, QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No) == QMessageBox::Yes;
  };
  minipro_found = false;
  programmer_found = false;

  window = new QWidget;
  window->setWindowTitle("minipro CLI not found!");
  window->resize(1000, 600);
  window->setMinimumSize(800, 480);

  layout = new QGridLayout(window);

  combobox_programmer = new QComboBox();
  combobox_device = new QComboBox();

  button_run_command = new QPushButton("Run Command");
  button_write = new QPushButton("Write to Device");
  button_read = new QPushButton("Read from Device");
  button_update = new QPushButton("Update Firmware");

  no_id_error = new QCheckBox("Ignore ID Error");
  skip_id = new QCheckBox("Skip ID Check");
  no_size_error = new QCheckBox("Ignore Size Error");
  skip_verify = new QCheckBox("Skip Verification");
  pin_check = new QCheckBox("Pin Check");
  blank_check = new QCheckBox("Blank Check");
  erase_device = new QCheckBox("Erase Device");
  hardware_check = new QCheckBox("Hardware Check");

  device_name_label = new QLabel("Name");
  device_name = new QLineEdit();
  device_name->setReadOnly(true);
  device_memory_label = new QLabel("Memory");
  device_memory = new QLineEdit();
  device_memory->setReadOnly(true);
  device_package_label = new QLabel("Package");
  device_package = new QLineEdit();
  device_package->setReadOnly(true);
  device_protocol_label = new QLabel("Protocol");
  device_protocol = new QLineEdit();
  device_protocol->setReadOnly(true);
  device_readbuffer_label = new QLabel("Read Buffer");
  device_readbuffer = new QLineEdit();
  device_readbuffer->setReadOnly(true);
  device_writebuffer_label = new QLabel("Write Buffer");
  device_writebuffer = new QLineEdit();
  device_writebuffer->setReadOnly(true);

  hexTableView = new QTableView(window);
  status_view = new QPlainTextEdit(window);

  monospace_font.setFamily("Courier New");
  monospace_font.setStyleHint(QFont::Monospace);
  status_view->setFont(monospace_font);
}

void MainWindow::run_async_process(const QStringList &process_arguments,
                                   AsyncOperation operation,
                                   const QString &type) {
  if (refuse_if_operation_running()) return;
  QString process_arguments_string = "";
  async_operation = operation;
  async_process = new QProcess(this);
  for (auto const &each : process_arguments) {
    process_arguments_string += each + " ";
  }

  status_view->appendPlainText("[Input]: minipro " + process_arguments_string);
  if (type == "stderr") {
    connect(async_process, SIGNAL(readyReadStandardError()), this, SLOT(async_process_err_output()));
  } else if (type == "stdout") {
    connect(async_process, SIGNAL(readyReadStandardOutput()), this, SLOT(async_process_std_output()));
  }
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
  set_device_operation_running(true);
  firmware_update_output.clear();
  firmware_prompt_answered = false;
  async_process->start("minipro", process_arguments);
  // minipro asks some questions on stdin. A firmware update's y/n prompt is answered from the
  // GUI (answer_firmware_prompt_if_asked). Nothing answers the others, such as which database to
  // use without a programmer, so give them end-of-input: they treat it as "no" and exit.
  if (operation != AsyncOperation::FirmwareUpdate) {
    async_process->closeWriteChannel();
  }
}

void MainWindow::answer_firmware_prompt_if_asked(const QString &new_output) {
  if (async_operation != AsyncOperation::FirmwareUpdate || firmware_prompt_answered) return;
  firmware_update_output += new_output;
  if (!firmware_update_output.contains(firmware_prompt)) return;
  // Set before asking: the dialog runs an event loop, and more output may arrive meanwhile.
  firmware_prompt_answered = true;
  QProcess *process = async_process;
  const bool go_ahead = confirm_firmware_update(firmware_update_output);
  if (process != async_process) return;  // the operation ended while the dialog was open
  status_view->appendPlainText(go_ahead ? "[Input]: y" : "[Input]: n");
  process->write(go_ahead ? "y\n" : "n\n");
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

void MainWindow::set_device_operation_running(bool running) {
  if (running) {
    disable_buttons();
    // Selecting a device or programmer also runs minipro against the programmer.
    combobox_programmer->setDisabled(true);
    return;
  }
  combobox_programmer->setDisabled(!minipro_found);
  if (programmer_found) {
    enable_buttons();
  } else {
    disable_buttons();
  }
}

void MainWindow::end_async_process() {
  async_process->deleteLater();
  async_process = nullptr;
  set_device_operation_running(false);
}

void MainWindow::async_process_err_output() {
  const QString output = QString::fromLocal8Bit(async_process->readAllStandardError());
  status_view->appendPlainText(QString(output).trimmed().replace("\u001B[K", ""));
  status_view->ensureCursorVisible();
  answer_firmware_prompt_if_asked(output);
}

void MainWindow::async_process_std_output() const {
  status_view->appendPlainText(async_process->readAllStandardOutput().trimmed().replace("\u001B[K", ""));
  status_view->ensureCursorVisible();
}

// Options modify an operation; they are safe to add to Read, Write and Run Command.
QStringList MainWindow::parse_option_checkboxes() const {
  QStringList arguments;
  if (no_id_error->isChecked()) arguments.append("--no_id_error");
  if (skip_id->isChecked()) arguments.append("--skip_id");
  if (no_size_error->isChecked()) arguments.append("--no_size_error");
  if (skip_verify->isChecked()) arguments.append("--skip_verify");
  if (pin_check->isChecked()) arguments.append("--pin_check");
  return arguments;
}

// Actions are operations of their own in minipro: --blank_check replaces an earlier -r/-w,
// and --hardware_check runs as soon as it is parsed. They are only used by Run Command, never
// added to Read, Write or Update Firmware.
QStringList MainWindow::parse_action_checkboxes() const {
  QStringList arguments;
  if (blank_check->isChecked()) arguments.append("--blank_check");
  if (erase_device->isChecked()) arguments.append("--erase");
  if (hardware_check->isChecked()) arguments.append("--hardware_check");
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

  QProcess process;
  process.start("minipro", process_arguments);
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
    target_plain_text_edit.appendPlainText("[Output]: " + output.replace("\u001B[K", ""));
  } else if (type == "stdout") {
    output = process.readAllStandardOutput();
    if (QRegularExpressionMatch match = serial_code.match(output); match.hasMatch()) {
      output = match.captured(1).trimmed().replace("\u001B[K", "");
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
    } else {
      combobox_programmer->setDisabled(true);
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
    const QSignalBlocker blocker(combobox_programmer);
    combobox_programmer->clear();
    if (match.hasMatch()) {
      programmer = match.captured(1).trimmed();
      programmer_found = true;
      combobox_programmer->addItem(programmer);
      enable_buttons();
    } else {
      combobox_programmer->setPlaceholderText("No programmer found");
      programmer_found = false;
      disable_buttons();
    }
  }
}

void MainWindow::disable_buttons() {
  combobox_device->setDisabled(true);
  button_run_command->setDisabled(true);
  button_read->setDisabled(true);
  button_write->setDisabled(true);
  button_update->setDisabled(true);
}

void MainWindow::enable_buttons() {
  combobox_device->setDisabled(false);
  button_run_command->setDisabled(false);
  button_read->setDisabled(false);
  button_write->setDisabled(false);
  button_update->setDisabled(false);
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

    combobox_device->addItems(devices_list);
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
    for (QLineEdit *field : {device_name, device_memory, device_package, device_protocol,
                             device_readbuffer, device_writebuffer}) {
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
    device_name->setText(device_field(info, "Name"));
    device_memory->setText(device_field(info, "Memory"));
    device_package->setText(device_field(info, "Package"));
    device_protocol->setText(device_field(info, "Protocol"));
    device_readbuffer->setText(device_field(info, "Read buffer size"));
    device_writebuffer->setText(device_field(info, "Write buffer size"));
    build_default_hex_output();
  }
}

void MainWindow::run_command() {
  if (minipro_found && !refuse_if_operation_running()) {
    const QStringList actions = parse_action_checkboxes();
    if (actions.size() != 1) {
      // minipro runs only one action; with several it would silently pick one.
      status_view->appendPlainText(
          "[Error]: Run Command needs exactly one of Blank Check, Erase Device or "
          "Hardware Check to be checked.");
      status_view->ensureCursorVisible();
      return;
    }
    if (actions.first() == "--erase" &&
        !confirm_device_change(tr("Erase %1?").arg(device),
                               tr("This erases everything on the %1 in the programmer.").arg(device))) {
      status_view->appendPlainText("[Cancelled]: erase " + device);
      return;
    }
    QStringList arguments;
    arguments << "-p" << device << actions << parse_option_checkboxes();
    // Asynchronous: an erase or blank check can take minutes on a large chip.
    run_async_process(arguments, actions.first() == "--erase" ? AsyncOperation::Erase
                                                              : AsyncOperation::Check);
  }
}

void MainWindow::read_device() {
  if (refuse_if_operation_running()) return;
  QStringList arguments;
  if (!read_dir.isValid()) {
    status_view->appendPlainText("[Error]: no temporary folder for the read: " + read_dir.errorString());
    status_view->ensureCursorVisible();
    return;
  }
  arguments << "-p" << device << "-r" << read_file_path() << parse_option_checkboxes();
  // Remove the previous read first, so a read that writes nothing can't show old data.
  QFile::remove(read_file_path());
  build_default_hex_output();
  run_async_process(arguments, AsyncOperation::Read);
}

void MainWindow::async_process_finished(int code, QProcess::ExitStatus status) {
  const bool succeeded = status == QProcess::NormalExit && code == 0;
  if (!succeeded) {
    status_view->appendPlainText(status == QProcess::CrashExit
                                     ? QString("[Error]: minipro crashed")
                                     : QString("[Error]: minipro exited with code %1").arg(code));
    status_view->ensureCursorVisible();
  }
  if (async_operation == AsyncOperation::Read && succeeded) {
    hexViewModel.clearHexTable();
    build_formatted_hex_output();
  } else if (async_operation == AsyncOperation::Check) {
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

void MainWindow::write_device() {
  if (const QString fileName = QFileDialog::getOpenFileName(this); !fileName.isEmpty()) {
    write_file_to_device(fileName);
  }
}

void MainWindow::write_file_to_device(const QString &file_name) {
  if (refuse_if_operation_running()) return;
  if (!confirm_device_change(tr("Write to %1?").arg(device),
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
  if (const QString fileName = QFileDialog::getOpenFileName(this); !fileName.isEmpty()) {
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

QString MainWindow::read_file_path() const {
  return read_dir.filePath("read.bin");
}

void MainWindow::build_formatted_hex_output() {
  QFile temp_file(read_file_path());
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
  rows->setDefaultSectionSize(QFontMetrics(monospace_font).height() + 8);
}
