#pragma once
#include "obs.h"
#include <obs-frontend-api.h>
#include <QCheckBox>
#include <qcombobox.h>
#include <QDockWidget>
#include <qpushbutton.h>
#include <QTextEdit>
#include <QVBoxLayout>

#include "obs.hpp"
#include "volume-meter.hpp"

class DeviceSwitcherDock : public QDockWidget {
	Q_OBJECT

private:
	QVBoxLayout *mainLayout;
	QComboBox *monitoringCombo = nullptr;
	obs_data_t *retain_config = nullptr;
	config_t *show_config = nullptr;
	QPushButton *virtualCamera = nullptr;
	static void add_source(void *p, calldata_t *calldata);
	static void remove_source(void *p, calldata_t *calldata);
	static void rename_source(void *p, calldata_t *calldata);
	static void save_source(void *p, calldata_t *calldata);
	static bool is_device_source(obs_source_t *source);
	static bool add_monitoring_device(void *data, const char *name,
					  const char *id);
	static void frontend_event(enum obs_frontend_event event, void *data);
	static void SaveFilterSettings(obs_source_t *parent,
				       obs_source_t *child, void *param);
	static void RemoveFilter(obs_source_t *parent, obs_source_t *child,
				 void *param);

	bool HasSourceSettings(QString sourceName);
	void SaveSourceSettings(obs_source_t *source);
	void RemoveSourceSettings(QString sourceName);
	void LoadSourceSettings(obs_source_t *source);
	void LoadFilter(obs_source_t *parent, obs_data_t *filter_data);

	friend class DeviceWidget;

private slots:
	void AddDeviceSource(QString sourceName);
	void RemoveDeviceSource(QString sourceName);
	void RenameDeviceSource(QString prevDeviceName, QString newDeviceName);

public:
	DeviceSwitcherDock(QWidget *parent = nullptr);
	~DeviceSwitcherDock();
};

class DeviceWidget : public QWidget {
	Q_OBJECT
private:
	obs_weak_source_t *source;
	DeviceSwitcherDock *dock;
	QSlider *slider = nullptr;
	QCheckBox *mute = nullptr;

	bool GetShowSetting(config_t *config, const char *st, const char *sn,
			    const char *setting);
	void UpdateVolControls();
	static void OBSVolume(void *data, calldata_t *call_data);
	static void OBSMute(void *data, calldata_t *call_data);

private slots:
	void SliderChanged(int vol);
	void SetOutputVolume(double volume);
	void SetMute(bool muted);

public:
	DeviceWidget(obs_source_t *source, obs_property_t *device_prop,
		     config_t *show_config, DeviceSwitcherDock *parent);

	~DeviceWidget();
};

class SliderIgnoreScroll : public QSlider {
	Q_OBJECT

public:
	SliderIgnoreScroll(QWidget *parent = nullptr);
	SliderIgnoreScroll(Qt::Orientation orientation,
			   QWidget *parent = nullptr);

protected:
	virtual void wheelEvent(QWheelEvent *event) override;
};

class MuteCheckBox : public QCheckBox {
	Q_OBJECT
};
