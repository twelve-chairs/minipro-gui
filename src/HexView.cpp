#include "HexView.h"

const int bytes_per_line = 16;
const int column_count = bytes_per_line + 1; // 16 byte columns + ASCII column
const int ascii_column = bytes_per_line;
const int placeholder_rows = 8;

namespace {
QChar printable(unsigned char byte) {
  // Printable, non-extended ASCII only
  return (byte >= 0x20 && byte <= 0x7E) ? QChar(byte) : QChar('.');
}
}  // namespace

HexView::HexView(QObject *parent) : QAbstractTableModel(parent) {
  clearHexTable();
  monospace_font.setFamily("Courier New");
  monospace_font.setStyleHint(QFont::Monospace);
}

int HexView::rowCount(const QModelIndex &parent) const {
  // Flat table: cells have no children.
  if (parent.isValid()) return 0;
  if (showing_placeholder) return placeholder_rows;
  return static_cast<int>((bytes.size() + bytes_per_line - 1) / bytes_per_line);
}

int HexView::columnCount(const QModelIndex &parent) const {
  // Fixed column count, so an empty table (fewer than 16 bytes loaded) is still safe.
  return parent.isValid() ? 0 : column_count;
}

QVariant HexView::data(const QModelIndex &index, int role) const {
  switch (role) {
    case Qt::DisplayRole: {
      return cellText(index.row(), index.column());
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

QString HexView::cellText(int row, int column) const {
  if (showing_placeholder) {
    return column == ascii_column ? QString(bytes_per_line, '.') : QString("--");
  }
  const qsizetype row_start = static_cast<qsizetype>(row) * bytes_per_line;
  if (column == ascii_column) {
    QString ascii;
    for (qsizetype i = row_start; i < row_start + bytes_per_line && i < bytes.size(); i++) {
      ascii += printable(static_cast<unsigned char>(bytes.at(i)));
    }
    return ascii;
  }
  const qsizetype offset = row_start + column;
  if (offset >= bytes.size()) return {};  // blank cells pad a trailing partial row
  return QString("%1").arg(static_cast<unsigned char>(bytes.at(offset)), 2, 16, QLatin1Char('0')).toUpper();
}

void HexView::clearHexTable() {
  beginResetModel();
  bytes.clear();
  showing_placeholder = true;
  endResetModel();
}

void HexView::loadBytes(const QByteArray &new_bytes) {
  beginResetModel();
  bytes = new_bytes;
  showing_placeholder = false;
  endResetModel();
}

void HexView::buildHexTable(const QString &hex) {
  loadBytes(QByteArray::fromHex(hex.toLatin1()));
}
