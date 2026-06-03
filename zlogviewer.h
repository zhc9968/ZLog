#ifndef ZLOGVIEWER_H
#define ZLOGVIEWER_H

#include <QWidget>
#include <QTableView>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QDateTime>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <QInputDialog>
#include <QDialog>
#include <QFormLayout>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QLabel>
#include <QTableWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QScrollBar>

// 自定义排序代理：ID 列按数值排序，其余正常
class LogSortProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit LogSortProxyModel(QObject *parent = nullptr);

    // 设置复合过滤条件
    void setLogFilter(const QString &level, const QString &searchText, bool useRegex);

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_filterLevel;      // 空表示不过滤级别
    QString m_filterSearch;     // 空表示不过滤文本
    bool    m_filterRegex = false;
};

class ZLogViewer : public QWidget
{
    Q_OBJECT

public:
    explicit ZLogViewer(QWidget *parent = nullptr);
    ~ZLogViewer();

    void loadFromFile(const QString &filePath);
    void clear();
    void showTop();

    void setAutoScroll(bool enabled);
    bool autoScroll() const;

    void exportToFile(const QString &filePath, const QString &format = "txt");
    void importFromFile(const QString &filePath);  // 新增导入

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNewMessage(quint64 id, int level, const QString &message,
                      const QString &file, int line, const QString &function,
                      quintptr threadId);   // 增加参数
    void onFilterChanged();
    void onExport();
    void onImport();
    void onClear();
    void onDoubleClicked(const QModelIndex &index);
    void onCustomContextMenu(const QPoint &pos);

private:
    void setupUi();
    void addEntry(quint64 id, int level, const QString &message,
                  const QString &file, int line, const QString &function,
                  quintptr threadId,                        // 增加参数
                  const QDateTime &time = QDateTime::currentDateTime());

    QDialog* createDetailDialog(const QModelIndex &sourceIndex);
    void showDetailDialog(const QModelIndex &sourceIndex);
    void showDetailDialogs(const QModelIndexList &sourceIndexes);

    QTableView *m_tableView;
    QStandardItemModel *m_model;
    LogSortProxyModel *m_proxyModel;

    QLineEdit *m_searchEdit;
    QComboBox *m_levelFilter;
    QPushButton *m_exportBtn;
    QPushButton *m_importBtn;
    QPushButton *m_clearBtn;
    QCheckBox *m_autoScrollCheck;
    QCheckBox *m_regexCheck;

    bool m_autoScroll;
};

#endif // ZLOGVIEWER_H