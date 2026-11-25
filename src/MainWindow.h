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
#include <QTableView>
#include <QListWidget>
#include <QLineEdit>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QHeaderView>
#include <iostream>
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
  void async_process_err_output() const;
  void async_process_std_output() const;
  void read_device_output(int, QProcess::ExitStatus);

  void run_command();
  void read_device();
  void write_device();
  void update_firmware();

 private:
  QWidget *window;

  QGridLayout *layout;

  QFont monospace_font;

  const QString temp_file_name = "temp.bin";

  QProcess *async_process;

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
  static QString run_process(QPlainTextEdit &, const QStringList &, const QString &);
  void run_async_process(QStringList &, const QString &);
  void build_default_hex_output();
  void build_formatted_hex_output();
};

#endif //MINIPRO_GUI_MAINWINDOW_H
