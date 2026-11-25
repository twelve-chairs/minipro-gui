#include "../src/HexView.h"
#include <QtTest/QtTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QElapsedTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace {
// The most memory this process has used so far, in MiB.
long peakMemoryMiB() {
#ifdef Q_OS_WIN
  PROCESS_MEMORY_COUNTERS counters{};
  GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters));
  return static_cast<long>(counters.PeakWorkingSetSize / (1024 * 1024));
#else
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  // ru_maxrss is bytes on macOS and kilobytes on Linux.
#ifdef Q_OS_MACOS
  return usage.ru_maxrss / (1024 * 1024);
#else
  return usage.ru_maxrss / 1024;
#endif
#endif
}

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
QByteArray bytesOf(std::initializer_list<int> values) {
  QByteArray bytes;
  for (int value : values) bytes.append(static_cast<char>(value));
  return bytes;
}
}  // namespace

class HexViewTest : public QObject {
 Q_OBJECT

 private slots:
  // Placeholder table
  void defaultTableIsPlaceholder() const;
  void clearHexTableRestoresPlaceholder() const;

  // Header labels
  void horizontalHeaderLabels_data() const;
  void horizontalHeaderLabels() const;
  void horizontalHeaderOutOfRangeIsInvalid() const;
  void verticalHeaderShowsByteOffset_data() const;
  void verticalHeaderShowsByteOffset() const;
  void headerNonDisplayRoleIsInvalid() const;

  // Data roles
  void dataAlignmentAndFont() const;
  void dataUnknownRoleIsInvalid() const;

  // buildHexTable
  void buildSingleRow() const;
  void buildMultipleRows() const;
  void hexIsUppercased() const;
  void asciiTranslation_data() const;
  void asciiTranslation() const;
  void buildEmitsModelReset() const;
  void rebuildReplacesPreviousContent() const;
  void trailingPartialRowIsKept() const;
  void lessThanOneRowDoesNotCrash_data() const;
  void lessThanOneRowDoesNotCrash() const;
  void validParentHasNoChildren() const;
  void largeImageIsCheapToLoad() const;

  // Generic model consistency checks
  void modelTesterOnPlaceholder() const;
  void modelTesterOnBuiltTable() const;
};

void HexViewTest::defaultTableIsPlaceholder() const {
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

void HexViewTest::clearHexTableRestoresPlaceholder() const {
  HexView model;
  model.buildHexTable(sequentialHex(64));
  QCOMPARE(model.rowCount(), 4);

  model.clearHexTable();
  QCOMPARE(model.rowCount(), kPlaceholderRows);
  QCOMPARE(model.index(0, 0).data().toString(), QString("--"));
}

void HexViewTest::horizontalHeaderLabels_data() const {
  QTest::addColumn<int>("section");
  QTest::addColumn<QString>("label");
  const QStringList labels{"00", "01", "02", "03", "04", "05", "06", "07",
                           "08", "09", "0A", "0B", "0C", "0D", "0E", "0F", "ASCII"};
  for (int i = 0; i < labels.size(); i++) {
    QTest::addRow("section %d", i) << i << labels.at(i);
  }
}

void HexViewTest::horizontalHeaderLabels() const {
  QFETCH(int, section);
  QFETCH(QString, label);
  HexView model;
  QCOMPARE(model.headerData(section, Qt::Horizontal, Qt::DisplayRole).toString(), label);
}

void HexViewTest::horizontalHeaderOutOfRangeIsInvalid() const {
  HexView model;
  QVERIFY(!model.headerData(kColumnCount, Qt::Horizontal, Qt::DisplayRole).isValid());
}

void HexViewTest::verticalHeaderShowsByteOffset_data() const {
  QTest::addColumn<int>("section");
  QTest::addColumn<QString>("label");
  QTest::newRow("row 0") << 0 << "000000";
  QTest::newRow("row 1") << 1 << "000010";
  QTest::newRow("row 2") << 2 << "000020";
  QTest::newRow("row 10") << 10 << "0000A0";
  QTest::newRow("row 255") << 255 << "000FF0";
  QTest::newRow("row 4096") << 4096 << "010000";
}

void HexViewTest::verticalHeaderShowsByteOffset() const {
  QFETCH(int, section);
  QFETCH(QString, label);
  HexView model;
  QCOMPARE(model.headerData(section, Qt::Vertical, Qt::DisplayRole).toString(), label);
}

void HexViewTest::headerNonDisplayRoleIsInvalid() const {
  HexView model;
  QVERIFY(!model.headerData(0, Qt::Horizontal, Qt::ToolTipRole).isValid());
  QVERIFY(!model.headerData(0, Qt::Vertical, Qt::FontRole).isValid());
}

void HexViewTest::dataAlignmentAndFont() const {
  HexView model;
  const QModelIndex index = model.index(0, 0);
  QCOMPARE(index.data(Qt::TextAlignmentRole).toInt(), static_cast<int>(Qt::AlignCenter));
  const auto font = index.data(Qt::FontRole).value<QFont>();
  QCOMPARE(font.family(), QString("Courier New"));
  QCOMPARE(font.styleHint(), QFont::Monospace);
}

void HexViewTest::dataUnknownRoleIsInvalid() const {
  HexView model;
  QVERIFY(!model.index(0, 0).data(Qt::ToolTipRole).isValid());
  QVERIFY(!model.index(0, 0).data(Qt::BackgroundRole).isValid());
}

void HexViewTest::buildSingleRow() const {
  HexView model;
  model.buildHexTable(sequentialHex(16));
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.columnCount(), kColumnCount);
  for (int col = 0; col < kByteColumns; col++) {
    QCOMPARE(model.index(0, col).data().toString(), QString("%1").arg(col, 2, 16, QLatin1Char('0')).toUpper());
  }
}

void HexViewTest::buildMultipleRows() const {
  HexView model;
  model.buildHexTable(sequentialHex(48));
  QCOMPARE(model.rowCount(), 3);
  QCOMPARE(model.index(1, 0).data().toString(), QString("10"));
  QCOMPARE(model.index(2, 15).data().toString(), QString("2F"));
  // Every row is distinct (regression: rows used to share the same values)
  QVERIFY(model.index(0, 0).data() != model.index(1, 0).data());
}

void HexViewTest::hexIsUppercased() const {
  HexView model;
  model.buildHexTable(QString("abcdef").repeated(5) + "ff");  // 16 bytes, lowercase input
  QCOMPARE(model.index(0, 0).data().toString(), QString("AB"));
  QCOMPARE(model.index(0, 2).data().toString(), QString("EF"));
  QCOMPARE(model.index(0, 15).data().toString(), QString("FF"));
}

void HexViewTest::asciiTranslation_data() const {
  QTest::addColumn<QByteArray>("bytes");
  QTest::addColumn<QString>("ascii");
  QTest::newRow("printable text") << QByteArray("Hello, minipro!!") << "Hello, minipro!!";
  QTest::newRow("space is printable") << QByteArray("A B C D E F G H ") << "A B C D E F G H ";
  QTest::newRow("NUL and control chars") << bytesOf({0x00, 0x01, 0x09, 0x0A, 0x0D, 0x1F}) + "ABCDEFGHIJ"
                                         << "......ABCDEFGHIJ";
  QTest::newRow("DEL and high bytes") << bytesOf({0x7E, 0x7F, 0x80, 0xFF}) + "abcdefghijkl" << "~...abcdefghijkl";
}

void HexViewTest::asciiTranslation() const {
  QFETCH(QByteArray, bytes);
  QFETCH(QString, ascii);
  QCOMPARE(bytes.size(), 16);
  HexView model;
  model.buildHexTable(QString::fromLatin1(bytes.toHex()));
  QCOMPARE(model.index(0, kAsciiColumn).data().toString(), ascii);
}

void HexViewTest::buildEmitsModelReset() const {
  HexView model;
  QSignalSpy aboutToReset(&model, &QAbstractItemModel::modelAboutToBeReset);
  QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
  model.buildHexTable(sequentialHex(16));
  QCOMPARE(aboutToReset.count(), 1);
  QCOMPARE(reset.count(), 1);
  model.clearHexTable();
  QCOMPARE(reset.count(), 2);
}

void HexViewTest::rebuildReplacesPreviousContent() const {
  HexView model;
  model.buildHexTable(sequentialHex(64));
  model.buildHexTable(QString("ff").repeated(16));
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.index(0, 0).data().toString(), QString("FF"));
}

void HexViewTest::trailingPartialRowIsKept() const {
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

void HexViewTest::lessThanOneRowDoesNotCrash_data() const {
  QTest::addColumn<int>("byteCount");
  QTest::addColumn<int>("rows");
  QTest::newRow("empty") << 0 << 0;
  QTest::newRow("1 byte") << 1 << 1;
  QTest::newRow("15 bytes") << 15 << 1;
}

void HexViewTest::lessThanOneRowDoesNotCrash() const {
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

void HexViewTest::validParentHasNoChildren() const {
  HexView model;
  const QModelIndex cell = model.index(0, 0);
  QCOMPARE(model.rowCount(cell), 0);
  QCOMPARE(model.columnCount(cell), 0);
  QVERIFY(!static_cast<QAbstractItemModel &>(model).hasChildren(cell));
}

void HexViewTest::modelTesterOnPlaceholder() const {
  HexView model;
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
  model.clearHexTable();
}

void HexViewTest::modelTesterOnBuiltTable() const {
  HexView model;
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
  model.buildHexTable(sequentialHex(256));
  QCOMPARE(model.rowCount(), 16);
}

void HexViewTest::largeImageIsCheapToLoad() const {
  // A 16 MiB SPI flash (e.g. 25Q128). Regression: the model stored one QString per byte, which
  // took gigabytes and many seconds for chips this size.
  constexpr qsizetype kSize = 16 * 1024 * 1024;
  QByteArray image(kSize, '\0');
  for (qsizetype i = 0; i < kSize; i++) image[i] = static_cast<char>(i * 7);

  const long peakBefore = peakMemoryMiB();
  QElapsedTimer timer;
  timer.start();
  HexView model;
  model.loadBytes(image);
  const qint64 elapsedMs = timer.elapsed();
  const long growthMiB = peakMemoryMiB() - peakBefore;
  qInfo("loaded 16 MiB in %lld ms, peak memory grew by %ld MiB", elapsedMs, growthMiB);

  QCOMPARE(model.rowCount(), static_cast<int>(kSize / 16));
  QCOMPARE(model.index(model.rowCount() - 1, 15).data().toString(),
           QString("%1").arg(static_cast<unsigned char>((kSize - 1) * 7), 2, 16, QLatin1Char('0')).toUpper());
  QCOMPARE(model.headerData(model.rowCount() - 1, Qt::Vertical, Qt::DisplayRole).toString(),
           QString("FFFFF0"));
  QVERIFY2(elapsedMs < 1000, qPrintable(QString("took %1 ms").arg(elapsedMs)));
  QVERIFY2(growthMiB < 200, qPrintable(QString("peak memory grew by %1 MiB").arg(growthMiB)));
}

QTEST_MAIN(HexViewTest)
#include "HexViewTest.moc"
