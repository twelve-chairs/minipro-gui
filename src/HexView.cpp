#include "HexView.h"

const int bytes_per_line = 16;
const int column_count = bytes_per_line + 1; // 16 byte columns + ASCII column

HexView::HexView(QObject *parent) : QAbstractTableModel(parent) {
  clearHexTable();
  monospace_font.setFamily("Courier New");
  monospace_font.setStyleHint(QFont::Monospace);
}

int HexView::rowCount(const QModelIndex &parent) const {
  // Flat table: cells have no children.
  return parent.isValid() ? 0 : static_cast<int>(table.size());
}

int HexView::columnCount(const QModelIndex &parent) const {
  // Fixed column count, so an empty table (fewer than 16 bytes loaded) is still safe.
  return parent.isValid() ? 0 : column_count;
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
    int offset_val = section * bytes_per_line;
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

  const QByteArray bytes = QByteArray::fromHex(temp_file_content.toLatin1());
  for (qsizetype offset = 0; offset < bytes.size(); offset += bytes_per_line) {
    QVector<QString> row;
    QString ascii_string_line;
    for (int column = 0; column < bytes_per_line; column++) {
      if (offset + column >= bytes.size()) {
        row.append("");  // pad a trailing partial row
        continue;
      }
      const auto byte = static_cast<unsigned char>(bytes.at(offset + column));
      row.append(QString("%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper());
      // Printable, non-extended ASCII only
      ascii_string_line += (byte >= 0x20 && byte <= 0x7E) ? QChar(byte) : QChar('.');
    }
    row.append(ascii_string_line);
    table.append(row);
  }
  endResetModel();
}
