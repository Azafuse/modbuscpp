#include "connection_panel.hpp"
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QPushButton>
#include <QMessageBox>

class MainWindow : public QWidget {
  Q_OBJECT
public:
  MainWindow() {
    setWindowTitle("modbus_tool — Multi-Connection Qt6");
    setMinimumSize(750, 520);
    auto* L = new QVBoxLayout(this);
    auto* top = new QHBoxLayout();
    auto* addBtn = new QPushButton("+ New Connection");
    top->addStretch(); top->addWidget(addBtn);
    L->addLayout(top);
    tabs_ = new QTabWidget();
    tabs_->setTabsClosable(true);
    L->addWidget(tabs_);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addPanel);
    connect(tabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::closePanel);
    addPanel();
  }
private slots:
  void addPanel() {
    auto* p = new ConnectionPanel();
    int idx = tabs_->addTab(p, "Conn " + QString::number(panels_ + 1));
    panels_++;
    connect(p, &QObject::destroyed, this, [this]() { panels_--; });
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
private:
  QTabWidget* tabs_;
  int panels_ = 0;
};

#include "modbus_gui.moc"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  MainWindow w; w.show();
  return app.exec();
}
