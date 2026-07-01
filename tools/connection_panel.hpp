#pragma once
#include "modbuscpp/client.hpp"
#include "modbuscpp/tcp_link.hpp"
#include "modbuscpp/rtu_link.hpp"
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
#include <QMetaObject>
#include <memory>

class ModbusWorker : public QObject {
  Q_OBJECT
public:
  void setClient(modbus::ModbusClient* c) { client_ = c; }
public slots:
  void doReadHR(uint16_t addr, uint16_t qty);
  void doReadCoils(uint16_t addr, uint16_t qty);
  void doWriteSR(uint16_t addr, uint16_t val);
  void doWriteSC(uint16_t addr, bool on);
signals:
  void readDone(const QString& r);
  void writeDone(const QString& r);
private:
  modbus::ModbusClient* client_ = nullptr;
};

class ConnectionPanel : public QWidget {
  Q_OBJECT
public:
  explicit ConnectionPanel(QWidget* parent = nullptr);
  ~ConnectionPanel();
  QString title() const;
  void disconnect();
  void stopPoll();
signals:
  void closeRequested();
private slots:
  void onConn();
  void onRead();
  void onWrite();
  void onPoll();
  void onPollTick();
  void onR(const QString& s);
  void onW(const QString& s);
private:
  void setDisconnected();
  QComboBox *typeCB_,*rFc_,*wFc_,*pFc_;
  QLineEdit *hostEd_,*portEd_,*wV_;
  QSpinBox *rA_,*rQ_,*wA_,*pA_,*pMs_,*unitSpin_;
  QPushButton *connBtn_,*readBtn_,*writeBtn_,*pollBtn_;
  QLabel* statusLbl_; QTextEdit* out_;
  std::unique_ptr<modbus::ModbusClient> client_;
  ModbusWorker* worker_; QThread* thr_; QTimer* pollTimer_;
  bool polling_=false; QString pType_; uint16_t pAddr_=0;
};