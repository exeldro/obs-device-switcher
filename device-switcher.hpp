#pragma once
#include "obs.h"
#include <obs-frontend-api.h>
#include <QDockWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include "obs.hpp"

class DeviceSwitcherDock : public QDockWidget {
	Q_OBJECT

private:
	QVBoxLayout *mainLayout;
	static void add_source(void *p, calldata_t *calldata);
	static void remove_source(void *p, calldata_t *calldata);
	static void rename_source(void *p, calldata_t *calldata);
	static bool is_device_source(obs_source_t *source);
private slots:
	void AddDeviceSource(QString sourceName);
	void RemoveDeviceSource(QString sourceName);
	void RenameDeviceSource(QString prevDeviceName, QString newDeviceName);

public:
	DeviceSwitcherDock(QWidget *parent = nullptr);
	~DeviceSwitcherDock();
};
