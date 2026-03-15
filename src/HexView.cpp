#include "HexView.h"

const int chars_per_line = 32; // (16 bytes * 2)

HexView::HexView(QObject *parent) : QAbstractTableModel(parent) {
  clearHexTable();
  monospace_font.setFamily("Courier New");
  monospace_font.setStyleHint(QFont::Monospace);
}

int HexView::rowCount(const QModelIndex &) const {
  return static_cast<int>(table.size());
}

int HexView::columnCount(const QModelIndex &) const {
  return static_cast<int>(table.at(0).size());
}

QVariant HexView::data(const QModelIndex &index, int role) const {
  switch (role) {
    case Qt::DisplayRole: {
      return table.at(index.row()).at(index.column());
    }
    case Qt::TextAlignmentRole: {
      return Qt::AlignCenter;
    }
    case Qt::FontRole: {
      return monospace_font;
    }
    default:break;
  }

  return QVariant();
}

QVariant HexView::headerData(int section, Qt::Orientation orientation, int role) const {
  if (role != Qt::DisplayRole) {
    return QVariant();
  }
  if (orientation == Qt::Horizontal) {
    switch (section) {
      case 0:return "00";
      case 1:return "01";
      case 2:return "02";
      case 3:return "03";
      case 4:return "04";
      case 5:return "05";
      case 6:return "06";
      case 7:return "07";
      case 8:return "08";
      case 9:return "09";
      case 10:return "0A";
      case 11:return "0B";
      case 12:return "0C";
      case 13:return "0D";
      case 14:return "0E";
      case 15:return "0F";
      case 16:return "ASCII";
      default:break;
    }
  }

  if (orientation == Qt::Vertical) {
    int offset_val = section * chars_per_line;
    //generate the starting offset for each row of the table in hex
    return QVariant::fromValue(QString("%1").arg(offset_val,6,16,QLatin1Char('0')).toUpper()); 
  }
  return QVariant();
}

void HexView::clearHexTable() {
  beginResetModel();
  table.clear();
  const int number_of_rows = 8;
  // Create 8 lines of blank data to display
  for (int i = 0; i < number_of_rows; i++) {
    table.append({"--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--",
                  "................"});
  }
  endResetModel();
}

void HexView::buildHexTable(QString temp_file_content) {
  beginResetModel();
  table.clear();

  QString ascii_string_line;
  QVector<QVector<QString>> new_hex_table;

  QVector<QString> next_row;

  for (int i = 0; i <= temp_file_content.length(); i++) {
    // Parse out 1-byte hex values, and convert to ASCII
    if (i % 2 != 0) {
      QString byte_string = QChar(temp_file_content[i - 1]);
      byte_string += QChar(temp_file_content[i]);

      next_row.append(byte_string.toUpper());

      auto ascii_int = byte_string.toUInt(nullptr, 16);
      // Non-extended ASCII only
      if (ascii_int > 126) ascii_int = 46;
      auto ascii_char = QChar(ascii_int);
      // Printable characters only
      if (!ascii_char.isPrint() || ascii_char.isNull()) {
        ascii_char = '.';
      }
      ascii_string_line += ascii_char;
    }
    // Add ASCII character translation, and move on to next 16-byte line
    if (i != 0 && i % chars_per_line == 0) {
      next_row.append(ascii_string_line);
      new_hex_table.append(next_row);
      next_row.clear();
      ascii_string_line.clear();
    }
  }
  table = new_hex_table;
  endResetModel();
}
