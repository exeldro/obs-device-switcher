#pragma once
#include "obs.h"
#include <obs-frontend-api.h>
#include <qcombobox.h>
#include <QDockWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include "obs.hpp"

class DeviceSwitcherDock : public QDockWidget {
	Q_OBJECT

private:
	QVBoxLayout *mainLayout;
	QComboBox * monitoringCombo;
	static void add_source(void *p, calldata_t *calldata);
	static void remove_source(void *p, calldata_t *calldata);
	static void rename_source(void *p, calldata_t *calldata);
	static bool is_device_source(obs_source_t *source);
	static bool add_monitoring_device(void * data, const char * name, const char * id);
	static void frontend_event(enum obs_frontend_event event, void *data);
private slots:
	void AddDeviceSource(QString sourceName);
	void RemoveDeviceSource(QString sourceName);
	void RenameDeviceSource(QString prevDeviceName, QString newDeviceName);

public:
	DeviceSwitcherDock(QWidget *parent = nullptr);
	~DeviceSwitcherDock();
};
