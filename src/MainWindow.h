#ifndef MINIPRO_GUI_MAINWINDOW_H
#define MINIPRO_GUI_MAINWINDOW_H

#include <QMainWindow>
#include <QStatusBar>
#include <QProcess>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QGridLayout>
#include <QRegularExpression>
#include <QFileDialog>
#include <QFile>
#include <QTemporaryDir>
#include <QTableView>
#include <QListWidget>
#include <QLineEdit>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QHeaderView>
#include <iostream>
#include <functional>
#include <array>
#include "HexView.h"

class MainWindow : public QMainWindow {
 Q_OBJECT
  friend class MainWindowTest;
  friend struct MainWindowDeleter;

 public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  void check_for_minipro();
  bool minipro_found;

 private slots:
  void check_for_programmer();
  void get_devices();
  void select_device(const QString & = "");
  void async_process_err_output();
  void async_process_std_output() const;
  void async_process_finished(int, QProcess::ExitStatus);
  void async_process_error(QProcess::ProcessError);

  void run_command();
  void read_device();
  void write_device();
  void update_firmware();

 private:
  QWidget *window;

  QGridLayout *layout;

  QFont monospace_font;

  // Device reads go to read.bin in a private (0700) temporary folder, created at startup and
  // removed on exit. Never the working directory: a user's own file there could be overwritten,
  // a planted symlink could redirect the write, and apps started from the Finder run in "/".
  QTemporaryDir read_dir;
  [[nodiscard]] QString read_file_path() const;

  // The one running device operation, or nullptr. Owned by this window; deleted when it ends.
  QProcess *async_process = nullptr;

  QPushButton *button_run_command;
  QPushButton *button_write;
  QPushButton *button_read;
  QPushButton *button_update;

  QLabel *device_name_label;
  QLineEdit *device_name;
  QLabel *device_memory_label;
  QLineEdit *device_memory;
  QLabel *device_package_label;
  QLineEdit *device_package;
  QLabel *device_protocol_label;
  QLineEdit *device_protocol;
  QLabel *device_readbuffer_label;
  QLineEdit *device_readbuffer;
  QLabel *device_writebuffer_label;
  QLineEdit *device_writebuffer;
  [[maybe_unused]] QLineEdit *device_other;

  bool programmer_found;

  QString programmer;
  QString device;

  QComboBox *combobox_programmer;
  QComboBox *combobox_device;
  [[maybe_unused]] QComboBox *vpp;
  [[maybe_unused]] QComboBox *vdd;
  [[maybe_unused]] QComboBox *vcc;

  QCheckBox *no_id_error;
  QCheckBox *skip_id;
  QCheckBox *no_size_error;
  QCheckBox *skip_verify;
  QCheckBox *pin_check;
  QCheckBox *blank_check;
  QCheckBox *erase_device;
  QCheckBox *hardware_check;

  QPlainTextEdit *status_view;

  QTableView *hexTableView;
  HexView hexViewModel;

  QStringList programmers_list;
  QStringList devices_list;

  void initializer();
  void format_hex_table_columns() const;
  void disable_buttons();
  void enable_buttons();
  [[nodiscard]] QStringList parse_option_checkboxes() const;
  [[nodiscard]] QStringList parse_action_checkboxes() const;
  void write_file_to_device(const QString &);
  void update_firmware_from_file(const QString &);
  // How long a quick synchronous query (--version, --presence_check, -l, -d) may take before
  // minipro is stopped. Device operations run asynchronously and have no limit.
  static int sync_timeout_ms;
  static QString run_process(QPlainTextEdit &, const QStringList &, const QString &);
  // What the running async process is doing, so its result is handled correctly.
  enum class AsyncOperation { Read, Write, FirmwareUpdate, Erase, Check };
  AsyncOperation async_operation = AsyncOperation::Read;

  void run_async_process(const QStringList &, AsyncOperation, const QString & = "stderr");

  // Asks the user whether to go ahead with a firmware update. It receives minipro's output so far,
  // which shows the installed and new firmware versions. Replaceable so tests never open a dialog.
  std::function<bool(const QString &)> confirm_firmware_update;
  // Asks before an operation that overwrites or erases the chip (title, text). Default answer No.
  // Replaceable so tests never open a dialog.
  std::function<bool(const QString &, const QString &)> confirm_device_change;
  QString firmware_update_output;
  bool firmware_prompt_answered = false;
  void answer_firmware_prompt_if_asked(const QString &new_output);
  [[nodiscard]] bool refuse_if_operation_running() const;
  void set_device_operation_running(bool);
  void end_async_process();
  void build_default_hex_output();
  void build_formatted_hex_output();
};

#endif //MINIPRO_GUI_MAINWINDOW_H
