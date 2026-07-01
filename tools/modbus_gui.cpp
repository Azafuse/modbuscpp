#include "connection_panel.hpp"
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QComboBox>
#include <QLabel>
#include <QTextEdit>
#include <QScrollBar>
#include <QMap>
#include <algorithm>

class MainWindow : public QWidget {
  Q_OBJECT
public:
  MainWindow() {
    setWindowTitle("modbus_tool - Multi-Connection Qt6");
    setMinimumSize(750, 580);
    auto* L = new QVBoxLayout(this);

    // Top bar: Add button + log mode switch
    auto* top = new QHBoxLayout();
    auto* addBtn = new QPushButton("+ New Connection");
    top->addWidget(addBtn);
    top->addStretch();
    top->addWidget(new QLabel("Log:"));
    logMode_ = new QComboBox();
    logMode_->addItems({"Active tab", "Common"});
    top->addWidget(logMode_);
    L->addLayout(top);

    // Tabs
    tabs_ = new QTabWidget();
    tabs_->setTabsClosable(true);
    L->addWidget(tabs_, 1);

    // Shared log output
    logOut_ = new QTextEdit();
    logOut_->setReadOnly(true);
    logOut_->document()->setMaximumBlockCount(5000);
    L->addWidget(logOut_, 2);

    // Signals
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addPanel);
    connect(tabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::closePanel);
    connect(tabs_, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(logMode_, &QComboBox::currentTextChanged, this, &MainWindow::onModeChanged);

    addPanel();
  }

private slots:
  void addPanel() {
    auto* p = new ConnectionPanel();
    int id = nextId_++;
    p->setConnId(id);
    int idx = tabs_->addTab(p, p->title());
    panels_[p] = QStringList();

    connect(p, &QObject::destroyed, this, [this, p]() { panels_.remove(p); });
    connect(p, &ConnectionPanel::logLine, this, &MainWindow::onLogLine);
    tabs_->setCurrentIndex(idx);
  }

  void closePanel(int idx) {
    auto* w = qobject_cast<ConnectionPanel*>(tabs_->widget(idx));
    if (w) {
      auto reply = QMessageBox::question(this, "Close", "Disconnect and close this connection?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (reply != QMessageBox::Yes) return;
      w->stopPoll();
      w->disconnect();
    }
    tabs_->removeTab(idx);
    if (w) w->deleteLater();
  }

  void onLogLine(int connId, const QString& timestamp, const QString& text) {
    auto* panel = findPanel(connId);
    if (!panel) return;
    QString line = QString("[%1] %2 %3").arg(panel->connLabel(), timestamp, text);
    panels_[panel].append(line);
    if (logMode_->currentText() == "Common") {
      logOut_->append(line);
    } else if (activePanel() == panel) {
      logOut_->append(line);
    }
    // autoscroll
    auto* sb = logOut_->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
  }

  void onTabChanged(int) { rebuildLog(); }

  void onModeChanged(const QString&) { rebuildLog(); }

private:
  void rebuildLog() {
    logOut_->clear();
    if (logMode_->currentText() == "Active tab") {
      auto* ap = activePanel();
      if (ap && panels_.contains(ap)) {
        for (auto& line : panels_[ap]) logOut_->append(line);
      }
    } else {
      // Common: chronologically interleaved by insertion order
      // Rebuild from all panels in order
      // Use a simple approach: collect all lines with their order
      QMap<qint64, QString> ordered; // can't have duplicate keys, use QList
      struct Entry { qint64 idx; QString line; };
      QList<Entry> all;
      qint64 counter = 0;
      // panels are in insertion order - iterate all panels
      for (auto it = panels_.begin(); it != panels_.end(); ++it) {
        for (auto& line : it.value()) {
          all.append({counter++, line});
        }
      }
      std::sort(all.begin(), all.end(), [](auto& a, auto& b){ return a.idx < b.idx; });
      for (auto& e : all) logOut_->append(e.line);
    }
    auto* sb = logOut_->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
  }

  ConnectionPanel* activePanel() const {
    return qobject_cast<ConnectionPanel*>(tabs_->currentWidget());
  }

  ConnectionPanel* findPanel(int connId) {
    for (int i = 0; i < tabs_->count(); ++i) {
      auto* p = qobject_cast<ConnectionPanel*>(tabs_->widget(i));
      if (p && p->connId() == connId) return p;
    }
    return nullptr;
  }

  QTabWidget* tabs_;
  QComboBox* logMode_;
  QTextEdit* logOut_;
  int nextId_ = 1;
  QMap<ConnectionPanel*, QStringList> panels_;
};

#include "modbus_gui.moc"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  MainWindow w; w.show();
  return app.exec();
}