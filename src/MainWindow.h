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
#include <memory>
#include <array>
#include "HexView.h"

class MainWindow : public QMainWindow {
 Q_OBJECT
  friend class MainWindowTest;

 public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  void check_for_minipro();

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
  // The top-level widget that holds the whole UI (MainWindow itself is never shown).
  std::unique_ptr<QWidget> window;
  bool minipro_found = false;

  // Device reads go to read.bin in a private (0700) temporary folder, created at startup and
  // removed on exit. Never the working directory: a user's own file there could be overwritten,
  // a planted symlink could redirect the write, and apps started from the Finder run in "/".
  QTemporaryDir read_dir;
  [[nodiscard]] QString read_file_path() const;

  // The one running device operation, or nullptr. Owned by this window; deleted when it ends.
  QProcess *async_process = nullptr;

  bool programmer_found = false;
  QString programmer;
  QString device;

  // Widgets, grouped by panel. All are owned by `window` through Qt's parent-child tree.
  struct Targets {
    QComboBox *programmer = nullptr;
    QComboBox *device = nullptr;
  } targets;
  struct DeviceButtons {
    QPushButton *run_command = nullptr;
    QPushButton *write = nullptr;
    QPushButton *read = nullptr;
    QPushButton *update = nullptr;
  } buttons;
  struct DeviceInfoFields {
    QLineEdit *name = nullptr;
    QLineEdit *memory = nullptr;
    QLineEdit *package = nullptr;
    QLineEdit *protocol = nullptr;
    QLineEdit *read_buffer = nullptr;
    QLineEdit *write_buffer = nullptr;
  } device_info;
  // Modify an operation; added to Read, Write and Run Command.
  struct OptionCheckboxes {
    QCheckBox *no_id_error = nullptr;
    QCheckBox *skip_id = nullptr;
    QCheckBox *no_size_error = nullptr;
    QCheckBox *skip_verify = nullptr;
    QCheckBox *pin_check = nullptr;
  } options;
  // Separate minipro operations; only Run Command uses them.
  struct ActionCheckboxes {
    QCheckBox *blank_check = nullptr;
    QCheckBox *erase = nullptr;
    QCheckBox *hardware_check = nullptr;
  } actions;

  QPlainTextEdit *status_view = nullptr;

  QTableView *hexTableView = nullptr;
  HexView hexViewModel;

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

  // Yes/No questions, defaulting to No. Replaceable so tests never open a dialog.
  struct Confirmations {
    // Go ahead with a firmware update? Gets minipro's output so far, which shows the installed
    // and new firmware versions.
    std::function<bool(const QString &)> firmware_update;
    // Go ahead with an operation that overwrites or erases the chip? (title, text)
    std::function<bool(const QString &, const QString &)> device_change;
  } confirmations;
  // minipro's output during a firmware update, until its y/n prompt has been answered.
  struct FirmwarePrompt {
    QString output;
    bool answered = false;
  } firmware_prompt;
  void answer_firmware_prompt_if_asked(const QString &new_output);
  [[nodiscard]] bool refuse_if_operation_running() const;
  [[nodiscard]] bool device_selected() const;
  [[nodiscard]] bool refuse_if_no_device() const;
  void set_device_operation_running(bool);
  void end_async_process();
  void build_default_hex_output();
  void build_formatted_hex_output();
};

#endif //MINIPRO_GUI_MAINWINDOW_H
