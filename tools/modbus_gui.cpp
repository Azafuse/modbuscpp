#include "modbuscpp/client.hpp"
#include "modbuscpp/tcp_link.hpp"
#include "modbuscpp/rtu_link.hpp"
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTextEdit>
#include <QSpinBox>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QMetaObject>
#include <memory>
#include <sstream>
#include <iomanip>
using namespace modbus;

class ModbusWorker : public QObject {
  Q_OBJECT
public:
  void setClient(ModbusClient* c) { client_ = c; }
public slots:
  void doReadHR(uint16_t addr, uint16_t qty) {
    if (!client_) return;
    auto r = client_->readHoldingRegisters(addr, qty);
    QString res;
    if (r.ok()) { for (auto v : r.value()) res += QString("%1 ").arg(v,4,16,QChar('0')).toUpper(); }
    else { res = "Err: " + QString::fromStdString(r.error().message); }
    emit readDone(res);
  }
  void doReadCoils(uint16_t addr, uint16_t qty) {
    if (!client_) return;
    auto r = client_->readCoils(addr, qty);
    QString res;
    if (r.ok()) { for (auto b : r.value()) res += b?'1':'0'; }
    else { res = "Err: " + QString::fromStdString(r.error().message); }
    emit readDone(res);
  }
  void doWriteSR(uint16_t addr, uint16_t val) {
    if (!client_) return;
    auto e = client_->writeSingleRegister(addr, val);
    emit writeDone(e.ok() ? "OK" : QString::fromStdString(e.message));
  }
  void doWriteSC(uint16_t addr, bool on) {
    if (!client_) return;
    auto e = client_->writeSingleCoil(addr, on);
    emit writeDone(e.ok() ? "OK" : QString::fromStdString(e.message));
  }
signals:
  void readDone(const QString& r);
  void writeDone(const QString& r);
private: ModbusClient* client_ = nullptr;
};

class MainWindow : public QWidget { Q_OBJECT
public:
  MainWindow(){setWindowTitle("modbus_tool Qt6");setMinimumSize(700,480);
    auto*L=new QVBoxLayout(this);auto*cg=new QGroupBox("Connection");auto*cl=new QHBoxLayout(cg);
    cl->addWidget(new QLabel("Type:"));typeCB_=new QComboBox();typeCB_->addItems({"TCP","RTU"});cl->addWidget(typeCB_);
    cl->addWidget(new QLabel("Host/Dev:"));hostEd_=new QLineEdit("127.0.0.1");cl->addWidget(hostEd_);
    cl->addWidget(new QLabel("Port/Bd:"));portEd_=new QLineEdit("502");cl->addWidget(portEd_);
    connBtn_=new QPushButton("Connect");cl->addWidget(connBtn_);
    statusLbl_=new QLabel("Disconnected");cl->addWidget(statusLbl_);L->addWidget(cg);
    auto*rg=new QGroupBox("Read");auto*rl=new QHBoxLayout(rg);
    rl->addWidget(new QLabel("FC:"));rFc_=new QComboBox();rFc_->addItems({"HR","IR","Coils","DI"});rl->addWidget(rFc_);
    rl->addWidget(new QLabel("Addr:"));rA_=new QSpinBox();rA_->setRange(0,65535);rl->addWidget(rA_);
    rl->addWidget(new QLabel("Qty:"));rQ_=new QSpinBox();rQ_->setRange(1,125);rl->addWidget(rQ_);
    readBtn_=new QPushButton("Read");rl->addWidget(readBtn_);L->addWidget(rg);
    auto*wg=new QGroupBox("Write");auto*wl=new QHBoxLayout(wg);
    wl->addWidget(new QLabel("FC:"));wFc_=new QComboBox();wFc_->addItems({"SR","SC"});wl->addWidget(wFc_);
    wl->addWidget(new QLabel("Addr:"));wA_=new QSpinBox();wA_->setRange(0,65535);wl->addWidget(wA_);
    wl->addWidget(new QLabel("Val:"));wV_=new QLineEdit("0");wl->addWidget(wV_);
    writeBtn_=new QPushButton("Write");wl->addWidget(writeBtn_);L->addWidget(wg);
    auto*pg=new QGroupBox("Poll");auto*pl=new QHBoxLayout(pg);
    pl->addWidget(new QLabel("FC:"));pFc_=new QComboBox();pFc_->addItems({"HR","Coils"});pl->addWidget(pFc_);
    pl->addWidget(new QLabel("Addr:"));pA_=new QSpinBox();pA_->setRange(0,65535);pl->addWidget(pA_);
    pl->addWidget(new QLabel("ms:"));pMs_=new QSpinBox();pMs_->setRange(50,10000);pMs_->setValue(500);pl->addWidget(pMs_);
    pollBtn_=new QPushButton("Start Poll");pl->addWidget(pollBtn_);L->addWidget(pg);
    out_=new QTextEdit();out_->setReadOnly(true);L->addWidget(out_);
    worker_=new ModbusWorker();thr_=new QThread(this);worker_->moveToThread(thr_);thr_->start();
    connect(connBtn_,&QPushButton::clicked,this,&MainWindow::onConn);
    connect(readBtn_,&QPushButton::clicked,this,&MainWindow::onRead);
    connect(writeBtn_,&QPushButton::clicked,this,&MainWindow::onWrite);
    connect(pollBtn_,&QPushButton::clicked,this,&MainWindow::onPoll);
    connect(worker_,&ModbusWorker::readDone,this,&MainWindow::onR);
    connect(worker_,&ModbusWorker::writeDone,this,&MainWindow::onW);
    connect(&pollTm_,&QTimer::timeout,this,&MainWindow::onPollTick);
  }
  ~MainWindow(){thr_->quit();thr_->wait();delete worker_;}
private slots:
  void onConn();
  void onRead();
  void onWrite();
  void onPoll();
  void onPollTick();
  void onR(const QString&s){out_->append(s);if(s.startsWith("Err:")&&client_&&!client_->connected()){setDisconnected();}}
  void onW(const QString&s){out_->append(s);if(s.startsWith("Err:")&&client_&&!client_->connected()){setDisconnected();}}
private:
  void setDisconnected(){statusLbl_->setText("Disconnected");connBtn_->setText("Connect");}
  bool connected_{false};
  QComboBox*typeCB_,*rFc_,*wFc_,*pFc_;QLineEdit*hostEd_,*portEd_,*wV_;
  QSpinBox*rA_,*rQ_,*wA_,*pA_,*pMs_;QPushButton*connBtn_,*readBtn_,*writeBtn_,*pollBtn_;
  QLabel*statusLbl_;QTextEdit*out_;std::unique_ptr<ModbusClient>client_;
  ModbusWorker*worker_;QThread*thr_;QTimer pollTm_;
  bool polling_=false;QString pType_;uint16_t pAddr_=0;
};

void MainWindow::onConn(){if(client_&&client_->connected()){client_->disconnect();client_.reset();statusLbl_->setText("Disconnected");connBtn_->setText("Connect");out_->append("-Disconnected-");return;}
  bool tcp=typeCB_->currentText()=="TCP";
  if(tcp)client_=std::make_unique<ModbusClient>(std::make_unique<TcpLink>(TcpConfig{hostEd_->text().toStdString(),(uint16_t)portEd_->text().toUShort(),std::chrono::milliseconds(3000),true}));
  else{RtuConfig c;c.device=hostEd_->text().toStdString();c.baud=portEd_->text().toUInt();client_=std::make_unique<ModbusClient>(std::make_unique<RtuLink>(c));}
  auto e=client_->connect();if(e.ok()){statusLbl_->setText("Connected");connBtn_->setText("Disconnect");out_->append("-Connected-");worker_->setClient(client_.get());}
  else{out_->append("Err: "+QString::fromStdString(e.message));client_.reset();}
}

void MainWindow::onRead(){if(!client_||!client_->connected()){out_->append("Not connected");return;}
  auto fc=rFc_->currentText();uint16_t a=rA_->value(),q=rQ_->value();
  if(fc=="HR"||fc=="IR")QMetaObject::invokeMethod(worker_,"doReadHR",Qt::QueuedConnection,Q_ARG(uint16_t,a),Q_ARG(uint16_t,q));
  else QMetaObject::invokeMethod(worker_,"doReadCoils",Qt::QueuedConnection,Q_ARG(uint16_t,a),Q_ARG(uint16_t,q));
}

void MainWindow::onWrite(){if(!client_||!client_->connected()){out_->append("Not connected");return;}
  uint16_t a=wA_->value();
  if(wFc_->currentText()=="SR")QMetaObject::invokeMethod(worker_,"doWriteSR",Qt::QueuedConnection,Q_ARG(uint16_t,a),Q_ARG(uint16_t,(uint16_t)wV_->text().toUInt(nullptr,16)));
  else QMetaObject::invokeMethod(worker_,"doWriteSC",Qt::QueuedConnection,Q_ARG(uint16_t,a),Q_ARG(bool,wV_->text().toLower()=="on"||wV_->text()=="1"));
}

void MainWindow::onPoll(){if(polling_){pollTm_.stop();polling_=false;pollBtn_->setText("Start Poll");out_->append("-Poll stopped-");return;}
  if(!client_||!client_->connected()){out_->append("Not connected");return;}
  pType_=pFc_->currentText();pAddr_=pA_->value();polling_=true;pollTm_.start(pMs_->value());pollBtn_->setText("Stop Poll");
}

void MainWindow::onPollTick(){if(!client_||!polling_)return;if(!client_->connected()){pollTm_.stop();polling_=false;pollBtn_->setText("Start Poll");setDisconnected();out_->append("-Connection lost-");return;}
  if(pType_=="HR")QMetaObject::invokeMethod(worker_,"doReadHR",Qt::QueuedConnection,Q_ARG(uint16_t,pAddr_),Q_ARG(uint16_t,1));
  else QMetaObject::invokeMethod(worker_,"doReadCoils",Qt::QueuedConnection,Q_ARG(uint16_t,pAddr_),Q_ARG(uint16_t,1));
}

#include "modbus_gui.moc"

int main(int argc,char*argv[]){QApplication app(argc,argv);MainWindow w;w.show();return app.exec();}
