#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_DisplayManager.h"

#include "common.h"

class DisplayManager : public QMainWindow
{
  Q_OBJECT
  PIMPL

public:
  ~DisplayManager();
  DisplayManager(QWidget *parent = Q_NULLPTR);
  
private:
  Ui::DisplayManagerClass ui;
};
