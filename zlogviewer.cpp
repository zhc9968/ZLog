#include "zlogviewer.h"
#include "zlog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextStream>
#include <QRegularExpression>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>

LogSortProxyModel::LogSortProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}
void LogSortProxyModel::setLogFilter(const QString &level, const QString &searchText, bool useRegex)
{
    m_filterLevel = level;
    m_filterSearch = searchText;
    m_filterRegex = useRegex;
    invalidateFilter();   // 触发重新过滤
}

bool LogSortProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (left.column() == 0) {
        quint64 leftVal  = sourceModel()->data(left,  Qt::UserRole).toULongLong();
        quint64 rightVal = sourceModel()->data(right, Qt::UserRole).toULongLong();
        return leftVal < rightVal;
    }
    return QSortFilterProxyModel::lessThan(left, right);
}

bool LogSortProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!m_filterLevel.isEmpty()) {
        QModelIndex levelIndex = sourceModel()->index(sourceRow, 2, sourceParent);
        QString level = sourceModel()->data(levelIndex).toString();
        if (level.compare(m_filterLevel, Qt::CaseInsensitive) != 0)
            return false;
    }

    if (!m_filterSearch.isEmpty()) {
        bool matched = false;
        for (int col = 0; col < sourceModel()->columnCount(); ++col) {
            QModelIndex idx = sourceModel()->index(sourceRow, col, sourceParent);
            QString data = sourceModel()->data(idx).toString();
            if (m_filterRegex) {
                QRegularExpression re(m_filterSearch);
                if (re.isValid() && re.match(data).hasMatch()) {
                    matched = true;
                    break;
                }
            } else {
                if (data.contains(m_filterSearch, Qt::CaseInsensitive)) {
                    matched = true;
                    break;
                }
            }
        }
        if (!matched) return false;
    }

    return true;
}

ZLogViewer::ZLogViewer(QWidget *parent)
    : QWidget(parent), m_autoScroll(true)
{
    setWindowTitle("日志查看器");
    resize(900, 500);
    setupUi();

    connect(ZLog::instance(), &ZLog::newMessage,
            this, &ZLogViewer::onNewMessage, Qt::QueuedConnection);
}

ZLogViewer::~ZLogViewer() {}
void ZLogViewer::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // 工具栏
    auto *toolbar = new QHBoxLayout();
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("搜索...");
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ZLogViewer::onFilterChanged);

    m_levelFilter = new QComboBox();
    m_levelFilter->addItems({"全部", "DEBUG", "INFO", "QUESTION", "WARNING", "CRITICAL"});
    connect(m_levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ZLogViewer::onFilterChanged);

    m_regexCheck = new QCheckBox("正则");
    connect(m_regexCheck, &QCheckBox::toggled, this, &ZLogViewer::onFilterChanged);

    m_autoScrollCheck = new QCheckBox("自动滚动");
    m_autoScrollCheck->setChecked(m_autoScroll);
    connect(m_autoScrollCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_autoScroll = checked;
    });

    m_exportBtn = new QPushButton("导出");
    connect(m_exportBtn, &QPushButton::clicked, this, &ZLogViewer::onExport);

    m_importBtn = new QPushButton("导入");
    connect(m_importBtn, &QPushButton::clicked, this, &ZLogViewer::onImport);

    m_clearBtn = new QPushButton("清空");
    connect(m_clearBtn, &QPushButton::clicked, this, &ZLogViewer::onClear);

    toolbar->addWidget(m_searchEdit);
    toolbar->addWidget(m_levelFilter);
    toolbar->addWidget(m_regexCheck);
    toolbar->addWidget(m_autoScrollCheck);
    toolbar->addStretch();
    toolbar->addWidget(m_importBtn);
    toolbar->addWidget(m_exportBtn);
    toolbar->addWidget(m_clearBtn);

    mainLayout->addLayout(toolbar);

    // 表格
    m_tableView = new QTableView();
    m_model = new QStandardItemModel(0, 8, this);
    m_model->setHorizontalHeaderLabels({"ID", "时间", "级别", "线程", "文件", "行", "函数", "消息"});

    m_proxyModel = new LogSortProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1);
    m_proxyModel->sort(0, Qt::AscendingOrder);
    m_tableView->setModel(m_proxyModel);
    m_tableView->setSortingEnabled(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    m_tableView->setTextElideMode(Qt::ElideNone);
    m_tableView->setWordWrap(true);
    m_tableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Interactive);
    m_tableView->setColumnWidth(7, 200);

    m_tableView->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);

    connect(m_tableView, &QTableView::doubleClicked, this, &ZLogViewer::onDoubleClicked);
    connect(m_tableView, &QWidget::customContextMenuRequested, this, &ZLogViewer::onCustomContextMenu);

    mainLayout->addWidget(m_tableView);
}

// ---------- 实时日志接入 ----------
void ZLogViewer::onNewMessage(quint64 id, int level, const QString &message,
                              const QString &file, int line, const QString &function,
                              quintptr threadId)
{
    addEntry(id, level, message, file, line, function, threadId);
}

void ZLogViewer::addEntry(quint64 id, int level, const QString &message,
                          const QString &file, int line, const QString &function,
                          quintptr threadId, const QDateTime &time)
{
    QString levelStr;
    switch (level) {
    case ZLog::DEBUG:    levelStr = "DEBUG"; break;
    case ZLog::INFO:     levelStr = "INFO"; break;
    case ZLog::QUESTION: levelStr = "QUESTION"; break;
    case ZLog::WARNING:  levelStr = "WARNING"; break;
    case ZLog::CRITICAL: levelStr = "CRITICAL"; break;
    default:             levelStr = "UNKNOWN";
    }

    QList<QStandardItem*> row;
    QStandardItem *idItem = new QStandardItem(QString::number(id));
    idItem->setData(QVariant::fromValue(id), Qt::UserRole);
    row.append(idItem);

    row.append(new QStandardItem(time.toString("yyyy-MM-dd hh:mm:ss.zzz")));
    row.append(new QStandardItem(levelStr));
    row.append(new QStandardItem(QString::number(threadId)));
    row.append(new QStandardItem(file));
    row.append(new QStandardItem(QString::number(line)));
    row.append(new QStandardItem(function));
    row.append(new QStandardItem(message));

    QColor color(Qt::black);
    if (level == ZLog::DEBUG)          color = Qt::gray;
    else if (level == ZLog::INFO)      color = Qt::darkGreen;
    else if (level == ZLog::QUESTION)  color = Qt::blue;
    else if (level == ZLog::WARNING)   color = QColor(200, 150, 0);
    else if (level == ZLog::CRITICAL)  color = Qt::red;

    for (auto *item : row) {
        item->setForeground(color);
    }

    m_model->appendRow(row);

    if (m_autoScroll)
        m_tableView->scrollToBottom();
}

// ---------- 过滤 ----------
void ZLogViewer::onFilterChanged()
{
    QString levelFilter = m_levelFilter->currentText();
    if (levelFilter == "全部")
        levelFilter.clear();   // 空表示不过滤

    QString searchText = m_searchEdit->text();
    bool useRegex = m_regexCheck->isChecked();

    m_proxyModel->setLogFilter(levelFilter, searchText, useRegex);
    // 注意：之前的 setFilterFixedString / setFilterRegularExpression 全部删除
}

// ---------- 双击：打开详情（支持多选）----------
void ZLogViewer::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    QModelIndexList selected = m_tableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QModelIndex sourceIdx = m_proxyModel->mapToSource(index);
        showDetailDialog(sourceIdx);
    } else {
        QModelIndexList sourceIndexes;
        for (const QModelIndex &proxyIdx : selected) {
            sourceIndexes.append(m_proxyModel->mapToSource(proxyIdx));
        }
        showDetailDialogs(sourceIndexes);
    }
}

// ---------- 右键菜单（多选复制/删除）----------
void ZLogViewer::onCustomContextMenu(const QPoint &pos)
{
    QModelIndex clickedIndex = m_tableView->indexAt(pos);
    int clickedCol = clickedIndex.column();
    QModelIndexList selected = m_tableView->selectionModel()->selectedRows();
    QMenu menu;

    if (!selected.isEmpty()) {
        if (clickedCol >= 0) {
            QAction *copyColAction = menu.addAction("复制当前列");
            connect(copyColAction, &QAction::triggered, this, [this, selected, clickedCol]() {
                QStringList colData;
                for (const QModelIndex &proxyIdx : selected) {
                    QModelIndex colIdx = proxyIdx.siblingAtColumn(clickedCol);
                    colData << colIdx.data().toString();
                }
                QApplication::clipboard()->setText(colData.join("\n"));
            });
        }

        QAction *copyRowsAction = menu.addAction("复制整行");
        connect(copyRowsAction, &QAction::triggered, this, [this, selected]() {
            QStringList rowsData;
            for (const QModelIndex &proxyIdx : selected) {
                QStringList rowFields;
                for (int c = 0; c < m_model->columnCount(); ++c) {
                    QModelIndex srcIdx = m_proxyModel->mapToSource(proxyIdx.siblingAtColumn(c));
                    rowFields << srcIdx.data().toString();
                }
                rowsData << rowFields.join("\t");
            }
            QApplication::clipboard()->setText(rowsData.join("\n"));
        });

        menu.addSeparator();

        QAction *deleteAction = menu.addAction("删除选中行");
        connect(deleteAction, &QAction::triggered, this, [this, selected]() {
            QList<int> srcRows;
            for (const QModelIndex &proxyIdx : selected) {
                srcRows.append(m_proxyModel->mapToSource(proxyIdx).row());
            }
            std::sort(srcRows.begin(), srcRows.end(), std::greater<int>());
            for (int row : srcRows) {
                m_model->removeRow(row);
            }
        });

        menu.addSeparator();

        QAction *detailAction = menu.addAction("查看详情");
        connect(detailAction, &QAction::triggered, this, [this, selected]() {
            QModelIndexList sourceIndexes;
            for (const QModelIndex &proxyIdx : selected) {
                sourceIndexes.append(m_proxyModel->mapToSource(proxyIdx));
            }
            showDetailDialogs(sourceIndexes);
        });
    } else {
        QAction *clearAllAction = menu.addAction("清空全部");
        connect(clearAllAction, &QAction::triggered, this, &ZLogViewer::onClear);
    }

    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}

// ---------- 导入 ----------
void ZLogViewer::onImport()
{
    QString filePath = QFileDialog::getOpenFileName(this, "导入日志", "",
                                                    "日志文件 (*.json);;所有文件 (*)");
    if (filePath.isEmpty()) return;
    importFromFile(filePath);
}

void ZLogViewer::importFromFile(const QString &filePath)
{
    QList<QVariantMap> entries;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导入失败", "无法打开文件：" + filePath);
        return;
    }

    QTextStream in(&file);
    if (filePath.endsWith(".json") || filePath.endsWith(".jsonl")) {
        // JSON 行格式
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;
            QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                QVariantMap entry;
                entry["id"] = obj.value("id").toString();
                entry["time"] = obj.value("time").toString();
                entry["level"] = obj.value("level").toString();
                entry["thread"] = obj.value("thread").toString();
                entry["file"] = obj.value("file").toString();
                entry["line"] = obj.value("line").toInt();
                entry["function"] = obj.value("function").toString();
                entry["message"] = obj.value("message").toString();
                entries.append(entry);
            }
        }
    } else {
        QMessageBox::information(this, "提示", "仅支持导入 JSON 格式（.json / .jsonl）文件。");
        return;
    }

    for (const auto &e : entries) {
        quint64 id = e["id"].toULongLong();
        int level = ZLog::DEBUG;
        QString levelStr = e["level"].toString();
        if (levelStr == "INFO" || levelStr == "info") level = ZLog::INFO;
        else if (levelStr == "QUESTION" || levelStr == "question") level = ZLog::QUESTION;
        else if (levelStr == "WARNING" || levelStr == "warning") level = ZLog::WARNING;
        else if (levelStr == "CRITICAL" || levelStr == "critical") level = ZLog::CRITICAL;

        QDateTime time = QDateTime::fromString(e["time"].toString(), "yyyy-MM-dd hh:mm:ss.zzz");
        if (!time.isValid()) time = QDateTime::currentDateTime();

        // 在 loadFromFile 里，从 entry 读取线程字段（如果存在）
        quintptr threadId = e.value("thread", "0").toULongLong();
        addEntry(id, level,
                 e.value("message").toString(),
                 e.value("file").toString(),
                 e.value("line").toInt(),
                 e.value("function").toString(),
                 threadId, time);
    }
    QMessageBox::information(this, "导入成功", QString("已从 %1 导入 %2 条记录").arg(filePath).arg(entries.size()));
}

// ---------- 导出 ----------
void ZLogViewer::onExport()
{
    QString filePath = QFileDialog::getSaveFileName(this, "导出日志", "",
                                                    "文本文件 (*.txt);;CSV文件 (*.csv);;JSON文件 (*.json)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法写入文件：" + filePath);
        return;
    }

    QTextStream out(&file);
    if (filePath.endsWith(".json")) {
        // 导出为 JSON 行格式
        for (int row = 0; row < m_proxyModel->rowCount(); ++row) {
            QJsonObject obj;
            obj["id"] = m_proxyModel->data(m_proxyModel->index(row, 0)).toString();
            obj["time"] = m_proxyModel->data(m_proxyModel->index(row, 1)).toString();
            obj["level"] = m_proxyModel->data(m_proxyModel->index(row, 2)).toString();
            obj["thread"] = m_proxyModel->data(m_proxyModel->index(row, 3)).toString();
            obj["file"] = m_proxyModel->data(m_proxyModel->index(row, 4)).toString();
            obj["line"] = m_proxyModel->data(m_proxyModel->index(row, 5)).toInt();
            obj["function"] = m_proxyModel->data(m_proxyModel->index(row, 6)).toString();
            obj["message"] = m_proxyModel->data(m_proxyModel->index(row, 7)).toString();
            out << QJsonDocument(obj).toJson(QJsonDocument::Compact) << "\n";
        }
    } else if (filePath.endsWith(".csv")) {
        // CSV 导出：函数名和消息加引号，其他字段直接输出
        out << "ID,时间,级别,线程,文件,行,函数,消息\n";
        for (int row = 0; row < m_proxyModel->rowCount(); ++row) {
            for (int col = 0; col < 8; ++col) {
                if (col > 0) out << ",";
                QString field = m_proxyModel->data(m_proxyModel->index(row, col)).toString();
                if (col == 6 || col == 7) { // 函数名和消息
                    out << "\"" << field << "\"";
                } else {
                    out << field;
                }
            }
            out << "\n";
        }
    } else {
        // 纯文本 TXT 导出：制表符分隔，函数名和消息加引号
        out << "ID\t时间\t级别\t线程\t文件\t行\t函数\t消息\n";
        for (int row = 0; row < m_proxyModel->rowCount(); ++row) {
            for (int col = 0; col < 8; ++col) {
                if (col > 0) out << "\t";
                QString field = m_proxyModel->data(m_proxyModel->index(row, col)).toString();
                if (col == 6 || col == 7) { // 函数名和消息
                    out << "\"" << field << "\"";
                } else {
                    out << field;
                }
            }
            out << "\n";
        }
    }
    file.close();
    QMessageBox::information(this, "导出成功", "日志已导出到：" + filePath);
}

void ZLogViewer::onClear()
{
    m_model->removeRows(0, m_model->rowCount());
}

// ---------- 历史加载 ----------
void ZLogViewer::loadFromFile(const QString &filePath)
{
    auto entries = ZLog::parseLogFile(filePath);
    for (const auto &entry : entries) {
        quint64 id = entry.value("id").toULongLong();
        QString levelStr = entry.value("level").toString();
        int level = ZLog::DEBUG;
        if (levelStr == "INFO") level = ZLog::INFO;
        else if (levelStr == "QUESTION") level = ZLog::QUESTION;
        else if (levelStr == "WARNING") level = ZLog::WARNING;
        else if (levelStr == "CRITICAL") level = ZLog::CRITICAL;

        QDateTime time = QDateTime::fromString(entry.value("time").toString(), "yyyy-MM-dd hh:mm:ss.zzz");
        if (!time.isValid()) time = QDateTime::currentDateTime();

        // 在 loadFromFile 里，从 entry 读取线程字段（如果存在）
        quintptr threadId = entry.value("thread", "0").toULongLong();
        addEntry(id, level,
                 entry.value("message").toString(),
                 entry.value("file").toString(),
                 entry.value("line").toInt(),
                 entry.value("function").toString(),
                 threadId, time);
    }
}

void ZLogViewer::clear()
{
    m_model->removeRows(0, m_model->rowCount());
}

void ZLogViewer::showTop()
{
    show();
    raise();
}

void ZLogViewer::setAutoScroll(bool enabled)
{
    m_autoScroll = enabled;
    m_autoScrollCheck->setChecked(enabled);
}

bool ZLogViewer::autoScroll() const
{
    return m_autoScroll;
}

// ---------- 导出辅助（无文件对话框）----------
void ZLogViewer::exportToFile(const QString &filePath, const QString &format)
{
    // 简化：不弹对话框，直接写入
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    if (format == "csv") {
        out << "ID,时间,级别,线程,文件,行,函数,消息\n";
        for (int row = 0; row < m_proxyModel->rowCount(); ++row) {
            for (int col = 0; col < 8; ++col) {
                if (col > 0) out << ",";
                QString field = m_proxyModel->data(m_proxyModel->index(row, col)).toString();
                if (col == 6 || col == 7) out << "\"" << field << "\"";
                else out << field;
            }
            out << "\n";
        }
    } else {
        out << "ID\t时间\t级别\t线程\t文件\t行\t函数\t消息\n";
        for (int row = 0; row < m_proxyModel->rowCount(); ++row) {
            for (int col = 0; col < 8; ++col) {
                if (col > 0) out << "\t";
                QString field = m_proxyModel->data(m_proxyModel->index(row, col)).toString();
                if (col == 6 || col == 7) out << "\"" << field << "\"";
                else out << field;
            }
            out << "\n";
        }
    }
    file.close();
}

// ---------- 详情窗口创建（非模态）----------
QDialog* ZLogViewer::createDetailDialog(const QModelIndex &sourceIndex)
{
    const int row = sourceIndex.row();
    QString id       = m_model->item(row, 0)->text();
    QString time     = m_model->item(row, 1)->text();
    QString level    = m_model->item(row, 2)->text();
    QString thread   = m_model->item(row, 3)->text();
    QString file     = m_model->item(row, 4)->text();
    QString line     = m_model->item(row, 5)->text();
    QString function = m_model->item(row, 6)->text();
    QString message  = m_model->item(row, 7)->text();

    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("日志详情 - ID:" + id);
    dlg->resize(800, 600);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *mainLayout = new QVBoxLayout(dlg);

    // 顶部单行表格
    QTableWidget *detailTable = new QTableWidget(1, 8);
    detailTable->setHorizontalHeaderLabels({"ID","时间","级别","线程","文件","行","函数","消息"});
    detailTable->setItem(0, 0, new QTableWidgetItem(id));
    detailTable->setItem(0, 1, new QTableWidgetItem(time));
    detailTable->setItem(0, 2, new QTableWidgetItem(level));
    detailTable->setItem(0, 3, new QTableWidgetItem(thread));
    detailTable->setItem(0, 4, new QTableWidgetItem(file));
    detailTable->setItem(0, 5, new QTableWidgetItem(line));
    detailTable->setItem(0, 6, new QTableWidgetItem(function));
    detailTable->setItem(0, 7, new QTableWidgetItem(message));

    detailTable->setWordWrap(true);
    detailTable->setTextElideMode(Qt::ElideNone);
    detailTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    detailTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detailTable->horizontalHeader()->setStretchLastSection(true);
    detailTable->verticalHeader()->setVisible(false);
    detailTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    int lineHeight = detailTable->fontMetrics().height();
    detailTable->verticalHeader()->setDefaultSectionSize(lineHeight * 3.5);
    int headerH = detailTable->horizontalHeader()->height();
    int rowH = detailTable->rowHeight(0);
    detailTable->setFixedHeight(headerH + rowH + 4);
    detailTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(detailTable, &QTableWidget::cellDoubleClicked, [detailTable](int r, int c) {
        QTableWidgetItem *item = detailTable->item(r, c);
        if (item) QApplication::clipboard()->setText(item->text());
    });

    mainLayout->addWidget(new QLabel("原始条目:"));
    mainLayout->addWidget(detailTable);

    QPushButton *copyRowBtn = new QPushButton("复制整行");
    connect(copyRowBtn, &QPushButton::clicked, [detailTable]() {
        QStringList rowData;
        for (int col = 0; col < detailTable->columnCount(); ++col) {
            QTableWidgetItem *item = detailTable->item(0, col);
            rowData << (item ? item->text() : QString());
        }
        QApplication::clipboard()->setText(rowData.join("\t"));
    });
    mainLayout->addWidget(copyRowBtn, 0, Qt::AlignRight);

    // JSON 区域
    QJsonObject jsonObj;
    jsonObj["id"]       = id;
    jsonObj["time"]     = time;
    jsonObj["level"]    = level;
    jsonObj["thread"]   = thread;
    jsonObj["file"]     = file;
    jsonObj["line"]     = line.toInt();
    jsonObj["function"] = function;
    jsonObj["message"]  = message;
    QString jsonText = QJsonDocument(jsonObj).toJson(QJsonDocument::Indented);

    mainLayout->addWidget(new QLabel("原始 JSON:"));
    QTextEdit *jsonEdit = new QTextEdit();
    jsonEdit->setReadOnly(true);
    jsonEdit->setPlainText(jsonText);
    jsonEdit->setMaximumHeight(120);
    mainLayout->addWidget(jsonEdit);

    QPushButton *copyJsonBtn = new QPushButton("复制 JSON");
    connect(copyJsonBtn, &QPushButton::clicked, [jsonText]() {
        QApplication::clipboard()->setText(jsonText);
    });
    QHBoxLayout *jsonBtnLayout = new QHBoxLayout();
    jsonBtnLayout->addStretch();
    jsonBtnLayout->addWidget(copyJsonBtn);
    mainLayout->addLayout(jsonBtnLayout);

    // 详细信息
    mainLayout->addWidget(new QLabel("详细信息:"));
    QTextEdit *detailEdit = new QTextEdit();
    detailEdit->setReadOnly(true);
    detailEdit->setPlainText(message);
    mainLayout->addWidget(detailEdit, 1);

    // 保存按钮
    QPushButton *saveBtn = new QPushButton("保存此条目");
    connect(saveBtn, &QPushButton::clicked, [dlg, id, time, level, thread, file, line, function, message]() {
        QString defaultName = QString("log_entry_%1.txt").arg(id);
        QString filePath = QFileDialog::getSaveFileName(dlg, "保存日志条目", defaultName,
                                                        "文本文件 (*.txt);;JSON 文件 (*.json)");
        if (filePath.isEmpty()) return;
        QFile f(filePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            if (filePath.endsWith(".json")) {
                QJsonObject obj;
                obj["id"] = id;
                obj["time"] = time;
                obj["level"] = level;
                obj["thread"] = thread;
                obj["file"] = file;
                obj["line"] = line.toInt();
                obj["function"] = function;
                obj["message"] = message;
                out << QJsonDocument(obj).toJson(QJsonDocument::Indented);
            } else {
                out << "ID: " << id << "\n时间: " << time << "\n级别: " << level
                    << "\n线程: " << thread << "\n文件: " << file << "\n行号: " << line
                    << "\n函数: " << function << "\n消息: " << message;
            }
            f.close();
            QMessageBox::information(dlg, "保存成功", "日志条目已保存到:\n" + filePath);
        } else {
            QMessageBox::warning(dlg, "保存失败", "无法写入文件:\n" + filePath);
        }
    });
    mainLayout->addWidget(saveBtn);

    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    mainLayout->addWidget(btnBox);

    return dlg;
}

void ZLogViewer::showDetailDialog(const QModelIndex &sourceIndex)
{
    QDialog *dlg = createDetailDialog(sourceIndex);
    dlg->setModal(false);
    dlg->show();
}

void ZLogViewer::showDetailDialogs(const QModelIndexList &sourceIndexes)
{
    for (const QModelIndex &idx : sourceIndexes) {
        QDialog *dlg = createDetailDialog(idx);
        dlg->setModal(false);
        dlg->show();
    }
}

void ZLogViewer::closeEvent(QCloseEvent *event)
{
    emit closed();
    QWidget::closeEvent(event);
}

