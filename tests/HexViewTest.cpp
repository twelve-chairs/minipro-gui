#include "../src/HexView.h"
#include <QtTest/QtTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>

namespace {
constexpr int kByteColumns = 16;
constexpr int kAsciiColumn = 16;
constexpr int kColumnCount = 17;
constexpr int kPlaceholderRows = 8;

// Hex string for `count` bytes 00, 01, 02, ...
QString sequentialHex(int count) {
  QByteArray bytes;
  for (int i = 0; i < count; i++) bytes.append(static_cast<char>(i & 0xFF));
  return QString::fromLatin1(bytes.toHex());
}
}  // namespace

class HexViewTest : public QObject {
 Q_OBJECT

 private slots:
  // Placeholder table
  void defaultTableIsPlaceholder();
  void clearHexTableRestoresPlaceholder();

  // Header labels
  void horizontalHeaderLabels_data();
  void horizontalHeaderLabels();
  void horizontalHeaderOutOfRangeIsInvalid();
  void verticalHeaderShowsByteOffset_data();
  void verticalHeaderShowsByteOffset();
  void headerNonDisplayRoleIsInvalid();

  // Data roles
  void dataAlignmentAndFont();
  void dataUnknownRoleIsInvalid();

  // buildHexTable
  void buildSingleRow();
  void buildMultipleRows();
  void hexIsUppercased();
  void asciiTranslation_data();
  void asciiTranslation();
  void buildEmitsModelReset();
  void rebuildReplacesPreviousContent();
  void trailingPartialRowIsKept();
  void lessThanOneRowDoesNotCrash_data();
  void lessThanOneRowDoesNotCrash();
  void validParentHasNoChildren();

  // Generic model consistency checks
  void modelTesterOnPlaceholder();
  void modelTesterOnBuiltTable();
};

void HexViewTest::defaultTableIsPlaceholder() {
  HexView model;
  QCOMPARE(model.rowCount(), kPlaceholderRows);
  QCOMPARE(model.columnCount(), kColumnCount);
  for (int row = 0; row < model.rowCount(); row++) {
    for (int col = 0; col < kByteColumns; col++) {
      QCOMPARE(model.index(row, col).data().toString(), QString("--"));
    }
    QCOMPARE(model.index(row, kAsciiColumn).data().toString(), QString("................"));
  }
}

void HexViewTest::clearHexTableRestoresPlaceholder() {
  HexView model;
  model.buildHexTable(sequentialHex(64));
  QCOMPARE(model.rowCount(), 4);

  model.clearHexTable();
  QCOMPARE(model.rowCount(), kPlaceholderRows);
  QCOMPARE(model.index(0, 0).data().toString(), QString("--"));
}

void HexViewTest::horizontalHeaderLabels_data() {
  QTest::addColumn<int>("section");
  QTest::addColumn<QString>("label");
  const QStringList labels{"00", "01", "02", "03", "04", "05", "06", "07",
                           "08", "09", "0A", "0B", "0C", "0D", "0E", "0F", "ASCII"};
  for (int i = 0; i < labels.size(); i++) {
    QTest::addRow("section %d", i) << i << labels.at(i);
  }
}

void HexViewTest::horizontalHeaderLabels() {
  QFETCH(int, section);
  QFETCH(QString, label);
  HexView model;
  QCOMPARE(model.headerData(section, Qt::Horizontal, Qt::DisplayRole).toString(), label);
}

void HexViewTest::horizontalHeaderOutOfRangeIsInvalid() {
  HexView model;
  QVERIFY(!model.headerData(kColumnCount, Qt::Horizontal, Qt::DisplayRole).isValid());
}

void HexViewTest::verticalHeaderShowsByteOffset_data() {
  QTest::addColumn<int>("section");
  QTest::addColumn<QString>("label");
  QTest::newRow("row 0") << 0 << "000000";
  QTest::newRow("row 1") << 1 << "000010";
  QTest::newRow("row 2") << 2 << "000020";
  QTest::newRow("row 10") << 10 << "0000A0";
  QTest::newRow("row 255") << 255 << "000FF0";
  QTest::newRow("row 4096") << 4096 << "010000";
}

void HexViewTest::verticalHeaderShowsByteOffset() {
  QFETCH(int, section);
  QFETCH(QString, label);
  HexView model;
  QCOMPARE(model.headerData(section, Qt::Vertical, Qt::DisplayRole).toString(), label);
}

void HexViewTest::headerNonDisplayRoleIsInvalid() {
  HexView model;
  QVERIFY(!model.headerData(0, Qt::Horizontal, Qt::ToolTipRole).isValid());
  QVERIFY(!model.headerData(0, Qt::Vertical, Qt::FontRole).isValid());
}

void HexViewTest::dataAlignmentAndFont() {
  HexView model;
  const QModelIndex index = model.index(0, 0);
  QCOMPARE(index.data(Qt::TextAlignmentRole).toInt(), static_cast<int>(Qt::AlignCenter));
  const auto font = index.data(Qt::FontRole).value<QFont>();
  QCOMPARE(font.family(), QString("Courier New"));
  QCOMPARE(font.styleHint(), QFont::Monospace);
}

void HexViewTest::dataUnknownRoleIsInvalid() {
  HexView model;
  QVERIFY(!model.index(0, 0).data(Qt::ToolTipRole).isValid());
  QVERIFY(!model.index(0, 0).data(Qt::BackgroundRole).isValid());
}

void HexViewTest::buildSingleRow() {
  HexView model;
  model.buildHexTable(sequentialHex(16));
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.columnCount(), kColumnCount);
  for (int col = 0; col < kByteColumns; col++) {
    QCOMPARE(model.index(0, col).data().toString(), QString("%1").arg(col, 2, 16, QLatin1Char('0')).toUpper());
  }
}

void HexViewTest::buildMultipleRows() {
  HexView model;
  model.buildHexTable(sequentialHex(48));
  QCOMPARE(model.rowCount(), 3);
  QCOMPARE(model.index(1, 0).data().toString(), QString("10"));
  QCOMPARE(model.index(2, 15).data().toString(), QString("2F"));
  // Every row is distinct (regression: rows used to share the same values)
  QVERIFY(model.index(0, 0).data() != model.index(1, 0).data());
}

void HexViewTest::hexIsUppercased() {
  HexView model;
  model.buildHexTable(QString("abcdef").repeated(5) + "ff");  // 16 bytes, lowercase input
  QCOMPARE(model.index(0, 0).data().toString(), QString("AB"));
  QCOMPARE(model.index(0, 2).data().toString(), QString("EF"));
  QCOMPARE(model.index(0, 15).data().toString(), QString("FF"));
}

void HexViewTest::asciiTranslation_data() {
  QTest::addColumn<QByteArray>("bytes");
  QTest::addColumn<QString>("ascii");
  QTest::newRow("printable text") << QByteArray("Hello, minipro!!") << "Hello, minipro!!";
  QTest::newRow("space is printable") << QByteArray("A B C D E F G H ") << "A B C D E F G H ";
  QTest::newRow("NUL and control chars") << QByteArray("\x00\x01\x09\x0A\x0D\x1F" "ABCDEFGHIJ", 16)
                                         << "......ABCDEFGHIJ";
  QTest::newRow("DEL and high bytes") << QByteArray("\x7E\x7F\x80\xFF" "abcdefghijkl", 16) << "~...abcdefghijkl";
}

void HexViewTest::asciiTranslation() {
  QFETCH(QByteArray, bytes);
  QFETCH(QString, ascii);
  QCOMPARE(bytes.size(), 16);
  HexView model;
  model.buildHexTable(QString::fromLatin1(bytes.toHex()));
  QCOMPARE(model.index(0, kAsciiColumn).data().toString(), ascii);
}

void HexViewTest::buildEmitsModelReset() {
  HexView model;
  QSignalSpy aboutToReset(&model, &QAbstractItemModel::modelAboutToBeReset);
  QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
  model.buildHexTable(sequentialHex(16));
  QCOMPARE(aboutToReset.count(), 1);
  QCOMPARE(reset.count(), 1);
  model.clearHexTable();
  QCOMPARE(reset.count(), 2);
}

void HexViewTest::rebuildReplacesPreviousContent() {
  HexView model;
  model.buildHexTable(sequentialHex(64));
  model.buildHexTable(QString("ff").repeated(16));
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.index(0, 0).data().toString(), QString("FF"));
}

void HexViewTest::trailingPartialRowIsKept() {
  HexView model;
  model.buildHexTable(sequentialHex(20));  // one full row + 4 bytes
  QCOMPARE(model.rowCount(), 2);
  QCOMPARE(model.columnCount(), kColumnCount);
  QCOMPARE(model.index(1, 0).data().toString(), QString("10"));
  QCOMPARE(model.index(1, 3).data().toString(), QString("13"));
  for (int col = 4; col < kByteColumns; col++) {
    QCOMPARE(model.index(1, col).data().toString(), QString());
  }
  QCOMPARE(model.index(1, kAsciiColumn).data().toString(), QString("...."));
  QCOMPARE(model.headerData(1, Qt::Vertical, Qt::DisplayRole).toString(), QString("000010"));
}

void HexViewTest::lessThanOneRowDoesNotCrash_data() {
  QTest::addColumn<int>("byteCount");
  QTest::addColumn<int>("rows");
  QTest::newRow("empty") << 0 << 0;
  QTest::newRow("1 byte") << 1 << 1;
  QTest::newRow("15 bytes") << 15 << 1;
}

void HexViewTest::lessThanOneRowDoesNotCrash() {
  // Regression: columnCount() used to index row 0 of an empty table (crash on empty / tiny reads).
  QFETCH(int, byteCount);
  QFETCH(int, rows);
  HexView model;
  model.buildHexTable(sequentialHex(byteCount));
  QCOMPARE(model.rowCount(), rows);
  QCOMPARE(model.columnCount(), kColumnCount);
  if (rows > 0) {
    QCOMPARE(model.index(0, byteCount - 1).data().toString(),
             QString("%1").arg(byteCount - 1, 2, 16, QLatin1Char('0')).toUpper());
    QCOMPARE(model.index(0, byteCount).data().toString(), QString());
  }
}

void HexViewTest::validParentHasNoChildren() {
  HexView model;
  const QModelIndex cell = model.index(0, 0);
  QCOMPARE(model.rowCount(cell), 0);
  QCOMPARE(model.columnCount(cell), 0);
  QVERIFY(!static_cast<QAbstractItemModel &>(model).hasChildren(cell));
}

void HexViewTest::modelTesterOnPlaceholder() {
  HexView model;
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
  model.clearHexTable();
}

void HexViewTest::modelTesterOnBuiltTable() {
  HexView model;
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
  model.buildHexTable(sequentialHex(256));
  QCOMPARE(model.rowCount(), 16);
}

QTEST_MAIN(HexViewTest)
#include "HexViewTest.moc"
