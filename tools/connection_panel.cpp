#include "connection_panel.hpp"
#include <chrono>

// â”€â”€ ModbusWorker â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void ModbusWorker::doReadHR(uint16_t addr, uint16_t qty) {
  if (!client_) return;
  auto r = client_->readHoldingRegisters(addr, qty);
  QString s;
  if (r.ok()) { for (auto v : r.value()) s += QString("%1 ").arg(v,4,16,QChar('0')).toUpper(); }
  else s = "Err: " + QString::fromStdString(r.error().message);
  emit readDone(s);
}

void ModbusWorker::doReadCoils(uint16_t addr, uint16_t qty) {
  if (!client_) return;
  auto r = client_->readCoils(addr, qty);
  QString s;
  if (r.ok()) { for (auto b : r.value()) s += b?'1':'0'; }
  else s = "Err: " + QString::fromStdString(r.error().message);
  emit readDone(s);
}

void ModbusWorker::doWriteSR(uint16_t addr, uint16_t val) {
  if (!client_) return;
  auto e = client_->writeSingleRegister(addr, val);
  emit writeDone(e.ok() ? "OK" : QString::fromStdString(e.message));
}

void ModbusWorker::doWriteSC(uint16_t addr, bool on) {
  if (!client_) return;
  auto e = client_->writeSingleCoil(addr, on);
  emit writeDone(e.ok() ? "OK" : QString::fromStdString(e.message));
}

// â”€â”€ ConnectionPanel â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

ConnectionPanel::ConnectionPanel(QWidget* parent) : QWidget(parent) {
  auto* L = new QVBoxLayout(this);

  auto* cg = new QGroupBox("Connection"); auto* cl = new QHBoxLayout(cg);
  cl->addWidget(new QLabel("Type:")); typeCB_ = new QComboBox(); typeCB_->addItems({"TCP","RTU"}); cl->addWidget(typeCB_);
  cl->addWidget(new QLabel("Host/Dev:")); hostEd_ = new QLineEdit("127.0.0.1"); cl->addWidget(hostEd_);
  cl->addWidget(new QLabel("Port/Bd:")); portEd_ = new QLineEdit("502"); cl->addWidget(portEd_);
  cl->addWidget(new QLabel("UnitID:")); unitSpin_ = new QSpinBox(); unitSpin_->setRange(0,247); unitSpin_->setValue(1); cl->addWidget(unitSpin_);
  connBtn_ = new QPushButton("Connect"); cl->addWidget(connBtn_);
  statusLbl_ = new QLabel("Disconnected"); cl->addWidget(statusLbl_);
  L->addWidget(cg);

  auto* rg = new QGroupBox("Read"); auto* rl = new QHBoxLayout(rg);
  rl->addWidget(new QLabel("FC:")); rFc_ = new QComboBox(); rFc_->addItems({"HR","IR","Coils","DI"}); rl->addWidget(rFc_);
  rl->addWidget(new QLabel("Addr:")); rA_ = new QSpinBox(); rA_->setRange(0,65535); rl->addWidget(rA_);
  rl->addWidget(new QLabel("Qty:")); rQ_ = new QSpinBox(); rQ_->setRange(1,125); rl->addWidget(rQ_);
  readBtn_ = new QPushButton("Read"); rl->addWidget(readBtn_); L->addWidget(rg);

  auto* wg = new QGroupBox("Write"); auto* wl = new QHBoxLayout(wg);
  wl->addWidget(new QLabel("FC:")); wFc_ = new QComboBox(); wFc_->addItems({"SR","SC"}); wl->addWidget(wFc_);
  wl->addWidget(new QLabel("Addr:")); wA_ = new QSpinBox(); wA_->setRange(0,65535); wl->addWidget(wA_);
  wl->addWidget(new QLabel("Val:")); wV_ = new QLineEdit("0"); wl->addWidget(wV_);
  writeBtn_ = new QPushButton("Write"); wl->addWidget(writeBtn_); L->addWidget(wg);

  auto* pg = new QGroupBox("Poll"); auto* pl = new QHBoxLayout(pg);
  pl->addWidget(new QLabel("FC:")); pFc_ = new QComboBox(); pFc_->addItems({"HR","Coils"}); pl->addWidget(pFc_);
  pl->addWidget(new QLabel("Addr:")); pA_ = new QSpinBox(); pA_->setRange(0,65535); pl->addWidget(pA_);
  pl->addWidget(new QLabel("ms:")); pMs_ = new QSpinBox(); pMs_->setRange(50,10000); pMs_->setValue(500); pl->addWidget(pMs_);
  pollBtn_ = new QPushButton("Start Poll"); pl->addWidget(pollBtn_); L->addWidget(pg);

  out_ = new QTextEdit(); out_->setReadOnly(true); L->addWidget(out_);
  worker_ = new ModbusWorker(); thr_ = new QThread(this); worker_->moveToThread(thr_); thr_->start();
  pollTimer_ = new QTimer(this);

  connect(connBtn_,&QPushButton::clicked,this,&ConnectionPanel::onConn);
  connect(readBtn_,&QPushButton::clicked,this,&ConnectionPanel::onRead);
  connect(writeBtn_,&QPushButton::clicked,this,&ConnectionPanel::onWrite);
  connect(pollBtn_,&QPushButton::clicked,this,&ConnectionPanel::onPoll);
  connect(worker_,&ModbusWorker::readDone,this,&ConnectionPanel::onR);
  connect(worker_,&ModbusWorker::writeDone,this,&ConnectionPanel::onW);
  connect(pollTimer_,&QTimer::timeout,this,&ConnectionPanel::onPollTick);
}
ConnectionPanel::~ConnectionPanel() { stopPoll(); disconnect(); thr_->quit(); thr_->wait(); delete worker_; }

QString ConnectionPanel::title() const { return typeCB_->currentText()+":"+hostEd_->text()+":"+portEd_->text(); }

void ConnectionPanel::disconnect() { if(client_&&client_->connected())client_->disconnect(); client_.reset(); setDisconnected(); out_->append("-Disconnected-"); }

void ConnectionPanel::stopPoll() { polling_=false; pollTimer_->stop(); pollBtn_->setText("Start Poll"); }

void ConnectionPanel::setDisconnected() { statusLbl_->setText("Disconnected"); connBtn_->setText("Connect"); }

void ConnectionPanel::onConn() {
  if(client_&&client_->connected()){disconnect();return;}
  bool tcp=typeCB_->currentText()=="TCP";
  if(tcp)client_=std::make_unique<modbus::ModbusClient>(std::make_unique<modbus::TcpLink>(modbus::TcpConfig{hostEd_->text().toStdString(),(uint16_t)portEd_->text().toUShort(),std::chrono::milliseconds(3000),true}));
  else{modbus::RtuConfig c;c.device=hostEd_->text().toStdString();c.baud=portEd_->text().toUInt();client_=std::make_unique<modbus::ModbusClient>(std::make_unique<modbus::RtuLink>(c));}
  client_->setUnitId((uint8_t)unitSpin_->value());
  auto e=client_->connect();
  if(e.ok()){statusLbl_->setText("Connected");connBtn_->setText("Disconnect");out_->append("-Connected-");worker_->setClient(client_.get());}
  else{out_->append("Err: "+QString::fromStdString(e.message));client_.reset();}
}

void ConnectionPanel::onRead() {
  if(!client_||!client_->connected()){out_->append("Not connected");return;}
  client_->setUnitId((uint8_t)unitSpin_->value());
  auto fc=rFc_->currentText();uint16_t a=rA_->value(),q=rQ_->value();
  if(fc=="HR"||fc=="IR")QMetaObject::invokeMethod(worker_,"doReadHR",Qt::QueuedConnection,Q_ARG(uint16_t,a),Q_ARG(uint16_t,q));
  else QMetaObject::invokeMethod(worker_,"doReadCoils",Qt::QueuedConnection,Q_ARG(uint16_t,a),Q_ARG(uint16_t,q));
}

void ConnectionPanel::onWrite() {
  if(!client_||!client_->connected()){out_->append("Not connected");return;}
  client_->setUnitId((uint8_t)unitSpin_->value());
  uint16_t a=wA_->value();
  if(wFc_->currentText()=="SR")QMetaObject::invokeMethod(worker_,"doWriteSR",Qt::QueuedConnection,Q_ARG(uint16_t,a),Q_ARG(uint16_t,(uint16_t)wV_->text().toUInt(nullptr,16)));
  else QMetaObject::invokeMethod(worker_,"doWriteSC",Qt::QueuedConnection,Q_ARG(uint16_t,a),Q_ARG(bool,wV_->text().toLower()=="on"||wV_->text()=="1"));
}

void ConnectionPanel::onPoll() {
  if(polling_){stopPoll();out_->append("-Poll stopped-");return;}
  if(!client_||!client_->connected()){out_->append("Not connected");return;}
  pType_=pFc_->currentText();pAddr_=pA_->value();polling_=true;pollTimer_->start(pMs_->value());pollBtn_->setText("Stop Poll");
}

void ConnectionPanel::onPollTick() {
  if(!client_||!polling_)return;
  if(!client_->connected()){stopPoll();setDisconnected();out_->append("-Connection lost-");return;}
  client_->setUnitId((uint8_t)unitSpin_->value());
  if(pType_=="HR")QMetaObject::invokeMethod(worker_,"doReadHR",Qt::QueuedConnection,Q_ARG(uint16_t,pAddr_),Q_ARG(uint16_t,1));
  else QMetaObject::invokeMethod(worker_,"doReadCoils",Qt::QueuedConnection,Q_ARG(uint16_t,pAddr_),Q_ARG(uint16_t,1));
}

void ConnectionPanel::onR(const QString& s){out_->append(s);if(s.startsWith("Err:")&&client_&&!client_->connected())setDisconnected();}

void ConnectionPanel::onW(const QString& s){out_->append(s);if(s.startsWith("Err:")&&client_&&!client_->connected())setDisconnected();}