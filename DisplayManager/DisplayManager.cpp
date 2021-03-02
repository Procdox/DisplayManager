#include "DisplayManager.h"
#include "monitors.h"
#include "USBWatcher.h"

#include <QDebug>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QStandardItemModel>
#include <QTimer>

#include <unordered_map>

#include "ui_HubModal.h"

class InputModel : public QStandardItemModel {
  Q_OBJECT

  const DisplayObject& config;
  QStandardItem * const device_item;
  QString name;

public:
  InputModel(const DisplayObject&, QStandardItem*, QObject*);
  ~InputModel() = default;

  
  std::vector<std::string> values;
  
  std::string& rowName(const QModelIndex& qidx) {
    return values.at(qidx.row());
  }
  void selectInput(const QModelIndex& qidx) {
    const auto& input_name = rowName(qidx);
    qDebug() << "Input Changed to:" << QString::fromStdString(input_name);
    config.setInput(input_name);
  }
  void selectInput(const std::string& s) {
    qDebug() << "Input Changed to:" << QString::fromStdString(s);
    config.setInput(s);
  }
  QModelIndex getCurrent() {
    auto value = config.current();
    qDebug() << "Current Input:" << QString::fromStdString(value);
    for (int i = 0; i < values.size(); ++i) {
      if(values[i] == value)
        return invisibleRootItem()->child(i,0)->index();
    }
    return QModelIndex();
  }

  const QString& getName() {
    return name;
  }
  void setName(const QString& _name) {
    name = _name;

    device_item->setText(name);

    QSettings settings;
    const auto group = QString::fromStdString(config.serial());
    settings.beginGroup(group);
    settings.setValue("name", name);
    settings.endGroup();
  }
  const DisplayObject& display() const {
    return config;
  }
};

InputModel::InputModel(const DisplayObject& _config, QStandardItem* _item, QObject* parent)
: QStandardItemModel(parent)
, config(_config)
, device_item(_item)
, name(QString::fromStdWString(config.name()))
{
  QStandardItem *parentItem = invisibleRootItem();

  QSettings settings;
  const auto group = QString::fromStdString(config.serial());
  const bool known = settings.childGroups().contains(group);
  settings.beginGroup(group);

  // we try to load inputs from file, because querying the monitor for them is incredibly slow
  if( known ) {
    name = settings.value("name").toString();
    device_item->setText(name);
    settings.beginGroup("sources");
    for(const auto& input_name : settings.childKeys()) {
      const auto input_mode = settings.value(input_name).toString().toStdString();

      QStandardItem *item = new QStandardItem(input_name);
      parentItem->appendRow(item);
      values.push_back(input_mode);
    }
    settings.endGroup();
  }
  else {
    settings.setValue("name", name);

    settings.beginGroup("sources");
    for (auto& pair : config.sources()) {
      QStandardItem *item = new QStandardItem(QString::fromStdString(pair.first));
      parentItem->appendRow(item);
      values.push_back(pair.second);

      settings.setValue(QString::fromStdString(pair.first),QString::fromStdString(pair.second));
    }
    settings.endGroup();
  }

  settings.endGroup();
}

class DeviceModel : public QStandardItemModel {
  Q_OBJECT

  std::vector<InputModel*> known_devices;
  std::vector<std::string> profile_a;
  std::vector<std::string> profile_b;
  bool profile_toggle = false;

  

  DisplayCollection collection;

public:
  DeviceModel(QObject* parent);
  ~DeviceModel() = default;

  void scan();
  InputModel* get_device(const QModelIndex&);

  void save_profile(const QString& name);
  void load_profile(const QString& name);

  void save_a();
  void save_b();
  void toggle_profile();
};

DeviceModel::DeviceModel(QObject* parent)
: QStandardItemModel(parent)
{}

void DeviceModel::scan() {
  clear();
  for (auto& device : known_devices)
    delete device;
  known_devices.clear();
  profile_a.clear();
  profile_b.clear();
  profile_toggle = false;

  collection.refresh();
  
  QStandardItem *parentItem = invisibleRootItem();
  for (auto& device : collection.get()) {
    QStandardItem *item = new QStandardItem(QString::fromStdWString(device.name()));
    parentItem->appendRow(item);
    known_devices.push_back(new InputModel(device, item, this));
  }
    
}

InputModel* DeviceModel::get_device(const QModelIndex& qidx) {
  return known_devices.at(qidx.row());
}

void DeviceModel::save_profile(const QString& name) {

  QSettings settings;
  settings.beginGroup("profiles");
  settings.beginGroup(name);
  for(auto& device : known_devices) {
    settings.setValue(QString::fromStdString(device->display().serial()),QString::fromStdString(device->display().current()));
  }
  settings.endGroup();
  settings.endGroup();
}
void DeviceModel::load_profile(const QString& name) {
  QSettings settings;
  settings.beginGroup("profiles");
  settings.beginGroup(name);

  for(auto& device : known_devices) {
    const auto value = settings.value(QString::fromStdString(device->display().serial()));
    if(value.isNull())
      continue;
    device->selectInput(value.toString().toStdString());
  }

  settings.endGroup();
  settings.endGroup();
}

void DeviceModel::save_a() {
  save_profile(QString("profile a"));
}

void DeviceModel::save_b() {
  save_profile(QString("profile b"));
}

void DeviceModel::toggle_profile() {
  load_profile(profile_toggle ? QString("profile a") : QString("profile b"));
  profile_toggle = !profile_toggle;
}

class HubDialog : public QDialog {
  Q_OBJECT

  Ui::HubSelectWindowClass ui;

  hubList list;
  std::wstring& watched;
public:
  HubDialog(QWidget* parent, std::wstring& _watched) 
  : QDialog(parent) 
  , watched(_watched) {
    ui.setupUi(this);

    connect(ui.option->button(QDialogButtonBox::Discard), &QPushButton::released, this, &HubDialog::reset);
    connect(ui.option->button(QDialogButtonBox::Apply), &QPushButton::released, this, &HubDialog::apply);

    ui.option->button(QDialogButtonBox::Discard)->setAutoDefault(false);
    ui.option->button(QDialogButtonBox::Close)->setAutoDefault(false);
    ui.option->button(QDialogButtonBox::Apply)->setAutoDefault(true);

    QSettings settings;
    const auto var = settings.value("Hub");
    if(!var.isNull() ) {
      watched = var.toString().toStdWString();
    }
  }

  virtual int exec() override {
    reset();
    return QDialog::exec();
  }
  void reset() {
    list = getConnectedUSB();
    ui.listWidget->clear();
    int r = 0;
    bool found = false;

    for(auto& p : list ) {
      QString name = QString::fromStdWString(p.second);
      ui.listWidget->addItem(name);
      if(p.first == watched) {
        found = true;
        ui.listWidget->setCurrentRow(r);
      }
      r++;
    }
    if(!found)
      ui.listWidget->setCurrentRow(0);
  }
  void apply() {
    watched = list.empty() ? L"" : list[ui.listWidget->currentRow()].first;
    qDebug() << "Hub Selected: " << watched;
    QSettings settings;
    settings.setValue("Hub",QString::fromStdWString(watched));
    close();
  }
};


class DisplayManager::Data : public QObject {
  Q_OBJECT
  DisplayManager& owner;

  DeviceModel* const devices;

  QTimer * const watch_timer;
  std::wstring watched_hub;
  HubDialog* const dialog;
  bool was_connected = true;

  bool validate_suggestion() const;

public:
  virtual ~Data() override {};
  Data(DisplayManager& owner);

private slots:
  void handleRefresh();
  void handleNameEdit();
  void handleDeviceSelected(const QModelIndex&);
  void handleInputSelected(const QModelIndex&);

  void handleDoWatch();
  void handleEnableWatch(bool);
  void handleOpenHubSelect();

  void handleSaveA();
  void handleSaveB();
  void handleToggle();
};

DisplayManager::Data::Data(DisplayManager& _owner)
: QObject(&_owner) 
, owner(_owner) 
, devices(new DeviceModel(&owner))
, watch_timer(new QTimer(this))
, dialog(new HubDialog(&owner, watched_hub))
{
  owner.ui.list_devices->setModel(devices);

QShortcut* const shortcut_toggle = new QShortcut(QKeySequence(Qt::Key_Asterisk + Qt::KeypadModifier, Qt::Key_8 + Qt::KeypadModifier), &owner);
QShortcut* const shortcut_savea = new QShortcut(QKeySequence(Qt::Key_Asterisk + Qt::KeypadModifier, Qt::Key_1 + Qt::KeypadModifier), &owner);
QShortcut* const shortcut_saveb = new QShortcut(QKeySequence(Qt::Key_Asterisk + Qt::KeypadModifier, Qt::Key_2 + Qt::KeypadModifier), &owner);

  bool valid = true;
  valid &= (bool)connect(owner.ui.action_refresh, &QAction::triggered, this, &DisplayManager::Data::handleRefresh);
  valid &= (bool)connect(owner.ui.action_savea, &QAction::triggered, this, &DisplayManager::Data::handleSaveA);
  valid &= (bool)connect(owner.ui.action_saveb, &QAction::triggered, this, &DisplayManager::Data::handleSaveB);
  valid &= (bool)connect(owner.ui.action_toggle, &QAction::triggered, this, &DisplayManager::Data::handleToggle);

  valid &= (bool)connect(watch_timer, &QTimer::timeout, this, &DisplayManager::Data::handleDoWatch);
  valid &= (bool)connect(owner.ui.action_watch, &QAction::triggered, this, &DisplayManager::Data::handleEnableWatch);
  valid &= (bool)connect(owner.ui.action_select, &QAction::triggered, this, &DisplayManager::Data::handleOpenHubSelect);
  
  valid &= (bool)connect(owner.ui.input_name, &QLineEdit::editingFinished, this, &DisplayManager::Data::handleNameEdit);
  valid &= (bool)connect(owner.ui.list_devices, &QListView::clicked, this, &DisplayManager::Data::handleDeviceSelected);
  valid &= (bool)connect(owner.ui.list_inputs, &QListView::clicked, this, &DisplayManager::Data::handleInputSelected);
  valid &= (bool)connect(shortcut_toggle, &QShortcut::activated, this, &DisplayManager::Data::handleToggle);
  valid &= (bool)connect(shortcut_savea, &QShortcut::activated, this, &DisplayManager::Data::handleSaveA);
  valid &= (bool)connect(shortcut_saveb, &QShortcut::activated, this, &DisplayManager::Data::handleSaveB);
  Q_ASSERT(valid);

  handleRefresh();
  getConnectedUSB();
}

bool DisplayManager::Data::validate_suggestion() const {
  const QString suggested_name = owner.ui.input_name->text();
  return suggested_name.size() > 0;
}

void DisplayManager::Data::handleRefresh() {
  qDebug() << "Refresh";
  devices->scan();
}
void DisplayManager::Data::handleNameEdit() {
  qDebug() << "Name Changed to:" << owner.ui.input_name->text();
  const auto qidx = owner.ui.list_devices->currentIndex();
  auto* device = devices->get_device(qidx);
  device->setName(owner.ui.input_name->text());
}
void DisplayManager::Data::handleDeviceSelected(const QModelIndex& qidx) {
  qDebug() << "Device Selected:" << qidx;
  auto* device = devices->get_device(qidx);
  owner.ui.list_inputs->setModel(device);
  owner.ui.input_name->setText(device->getName());
  owner.ui.list_inputs->setCurrentIndex(device->getCurrent());
}
void DisplayManager::Data::handleInputSelected(const QModelIndex& qidx) {
  auto* model = static_cast<InputModel*>(owner.ui.list_inputs->model());
  const auto& input_name = model->rowName(qidx);

  const double max_time = 5.0;

  model->selectInput(input_name);

  const auto start = std::chrono::high_resolution_clock::now();
  while(true) {
    const auto end = std::chrono::high_resolution_clock::now();
    const auto time = std::chrono::duration<double>(end - start).count();
    if( model->display().current() == input_name ) {
      qDebug() << "Input change took" << time << "seconds.";
      break;
    }
    if (time > max_time) {
      qDebug() << "Input change timing took longer than" << max_time << "seconds.";
      break;
    }
  }
}

void DisplayManager::Data::handleDoWatch() {
  
  bool is_connected = isUSBConnected(watched_hub);
  if( was_connected != is_connected ) {
    qDebug() << "!!!!!  DIFFERENCE  !!!!!";

    devices->load_profile(is_connected ? QString("profile a") : QString("profile b"));
    auto* device = static_cast<InputModel*>(owner.ui.list_inputs->model());
    if(device)
      owner.ui.list_inputs->setCurrentIndex(device->getCurrent());

    was_connected = is_connected;
  }
}
void DisplayManager::Data::handleEnableWatch(bool checked) {
  //verify selected hub exists RIGHT NOW before enabling
  if(!isUSBConnected(watched_hub)) {
    qWarning() << "The hub you want to watch isn't currently select it. It must be so when you turn on monitoring.";
    qWarning() << "Curent Hub:" << watched_hub;
    owner.ui.action_watch->setChecked(false);
    return;
  }

  if(checked) {
    was_connected = true;
    watch_timer->start(1000);
  }
  else {
    watch_timer->stop();
  }
}
void DisplayManager::Data::handleOpenHubSelect() {
  //show a modal to allow the user to select the watched hub
  watch_timer->stop();
  owner.ui.action_watch->setChecked(false);

  dialog->exec();
}
void DisplayManager::Data::handleSaveA() {
  qDebug() << "Save A";
  devices->save_a();
}
void DisplayManager::Data::handleSaveB() {
  qDebug() << "Save B";
  devices->save_b();
}
void DisplayManager::Data::handleToggle() {
  qDebug() << "Toggle Profile";
  devices->toggle_profile();

  auto* device = static_cast<InputModel*>(owner.ui.list_inputs->model());
  if(device)
    owner.ui.list_inputs->setCurrentIndex(device->getCurrent());
}










DisplayManager::~DisplayManager() {

}

DisplayManager::DisplayManager(QWidget *parent)
: QMainWindow( parent ) 
, data( [this](){ 
    ui.setupUi(this);
    return std::make_unique<Data>(*this);
  }() ) 
{}
#include "DisplayManager.moc"