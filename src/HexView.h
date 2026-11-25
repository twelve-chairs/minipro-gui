#ifndef MINIPRO_GUI_SRC_HEXVIEW_H_
#define MINIPRO_GUI_SRC_HEXVIEW_H_

#include <QTime>
#include <QAbstractTableModel>
#include <QFont>

class HexView : public QAbstractTableModel {
 Q_OBJECT
 public:
  explicit HexView(QObject *parent = nullptr);
  int rowCount(const QModelIndex & = QModelIndex()) const override;
  int columnCount(const QModelIndex & = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  // Show the placeholder rows of "--" (no data loaded).
  void clearHexTable();
  // Show these bytes. Cells are formatted when the view asks for them, so memory use stays
  // close to the image size whatever the chip size.
  void loadBytes(const QByteArray &);
  // Same as loadBytes(), from a hex string such as "48656c6c6f".
  void buildHexTable(const QString &);

 private:
  [[nodiscard]] QString cellText(int row, int column) const;
  QFont monospace_font;
  QByteArray bytes;
  bool showing_placeholder = true;

};

#endif //MINIPRO_GUI_SRC_HEXVIEW_H_
