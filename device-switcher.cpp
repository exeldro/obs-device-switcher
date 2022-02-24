#include "device-switcher.hpp"
#include <obs-module.h>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "version.h"
#include "util/config-file.h"
#include "util/platform.h"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("device-switcher", "en-US")

bool obs_module_load()
{
	blog(LOG_INFO, "[Device Switcher] loaded version %s", PROJECT_VERSION);

	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_push_ui_translation(obs_module_get_string);
	auto dock = new DeviceSwitcherDock(main_window);
	obs_frontend_add_dock(dock);
	obs_frontend_pop_ui_translation();
	return true;
}

void obs_module_unload() {}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("DeviceSwitcher");
}

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

void DeviceSwitcherDock::add_source(void *p, calldata_t *calldata)
{
	auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (!is_device_source(source))
		return;

	QString deviceName = obs_source_get_name(source);

	QMetaObject::invokeMethod((DeviceSwitcherDock *)p, "AddDeviceSource",
				  Qt::QueuedConnection,
				  Q_ARG(QString, deviceName));
}

void DeviceSwitcherDock::remove_source(void *p, calldata_t *calldata)
{
	auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (!is_device_source(source))
		return;

	QString deviceName = QT_UTF8(obs_source_get_name(source));

	QMetaObject::invokeMethod((DeviceSwitcherDock *)p, "RemoveDeviceSource",
				  Qt::QueuedConnection,
				  Q_ARG(QString, deviceName));
}

void DeviceSwitcherDock::rename_source(void *p, calldata_t *calldata)
{
	QString prevDeviceName =
		QT_UTF8(calldata_string(calldata, "prev_name"));
	QString newDeviceName = QT_UTF8(calldata_string(calldata, "new_name"));
	QMetaObject::invokeMethod((DeviceSwitcherDock *)p, "RenameDeviceSource",
				  Qt::QueuedConnection,
				  Q_ARG(QString, prevDeviceName),
				  Q_ARG(QString, newDeviceName));
}

void DeviceSwitcherDock::save_source(void *p, calldata_t *calldata)
{
	auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (!is_device_source(source))
		return;

	auto dock = (DeviceSwitcherDock *)p;
	if (!dock->config)
		return;
	QString deviceName = QT_UTF8(obs_source_get_name(source));
	if (!dock->HasSourceSettings(deviceName))
		return;
	dock->SaveSourceSettings(source);
}

bool DeviceSwitcherDock::is_device_source(obs_source_t *source)
{
	auto settings = obs_source_get_settings(source);
	if (!settings)
		return false;
	obs_data_release(settings);
	if (obs_data_has_user_value(settings, "device_id") ||
	    obs_data_has_default_value(settings, "device_id") ||
	    obs_data_has_autoselect_value(settings, "device_id")) {
		return true;
	}
	if (obs_data_has_user_value(settings, "video_device_id") ||
	    obs_data_has_default_value(settings, "video_device_id") ||
	    obs_data_has_autoselect_value(settings, "video_device_id")) {
		return true;
	}
	return false;
}

bool DeviceSwitcherDock::add_monitoring_device(void *data, const char *name,
					       const char *id)
{
	const auto dock = static_cast<DeviceSwitcherDock *>(data);
	dock->monitoringCombo->addItem(QString::fromUtf8(name),
				       QString::fromUtf8(id));
	return true;
}

void DeviceSwitcherDock::frontend_event(enum obs_frontend_event event,
					void *data)
{
	if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
		const auto dock = static_cast<DeviceSwitcherDock *>(data);
		if (dock && dock->monitoringCombo) {
			const char *name;
			const char *id;
			obs_get_audio_monitoring_device(&name, &id);
			dock->monitoringCombo->setCurrentText(
				QString::fromUtf8(name));
		}
	}
}

bool DeviceSwitcherDock::HasSourceSettings(QString sourceName)
{
	if (!config)
		return false;
	const auto array = obs_data_get_array(config, "sources");
	if (!array)
		return false;
	bool found = false;
	auto count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(array, i);
		if (!item)
			continue;
		auto name =
			QString::fromUtf8(obs_data_get_string(item, "name"));
		obs_data_release(item);
		if (name == sourceName) {
			found = true;
			break;
		}
	}
	obs_data_array_release(array);
	return found;
}

void DeviceSwitcherDock::SaveSourceSettings(obs_source_t *source)
{
	if (!config || !source)
		return;
	auto array = obs_data_get_array(config, "sources");
	if (!array) {
		array = obs_data_array_create();
		obs_data_set_array(config, "sources", array);
	}
	obs_data_t *t = nullptr;
	auto source_name = obs_source_get_name(source);
	auto count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(array, i);
		if (!item)
			continue;
		auto name = obs_data_get_string(item, "name");
		if (strcmp(name, source_name) == 0) {
			t = item;
			break;
		}
		obs_data_release(item);
	}
	if (t == nullptr) {
		t = obs_data_create();
		obs_data_set_string(t, "name", source_name);
		obs_data_array_push_back(array, t);
	}
	obs_data_set_string(t, "id", obs_source_get_unversioned_id(source));
	obs_data_array_release(array);
	auto s = obs_data_get_obj(t, "settings");
	if (s == nullptr) {
		s = obs_data_create();
		obs_data_set_obj(t, "settings", s);
	}
	if (const auto settings = obs_source_get_settings(source)) {
		obs_data_apply(s, settings);
		obs_data_release(settings);
	}
	obs_data_release(s);
	obs_data_array_t *filters = obs_data_array_create();
	obs_source_enum_filters(source, SaveFilterSettings, filters);
	obs_data_set_array(t, "filters", filters);
	obs_data_array_release(filters);
	obs_data_release(t);
}

void DeviceSwitcherDock::SaveFilterSettings(obs_source_t *source,
					    obs_source_t *filter, void *param)
{
	auto array = static_cast<obs_data_array_t *>(param);
	obs_data_t *t = nullptr;
	auto source_name = obs_source_get_name(filter);
	auto count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(array, i);
		if (!item)
			continue;
		auto name = obs_data_get_string(item, "name");
		if (strcmp(name, source_name) == 0) {
			t = item;
			break;
		}
		obs_data_release(item);
	}
	if (t == nullptr) {
		t = obs_data_create();
		obs_data_set_string(t, "name", source_name);
		obs_data_array_push_back(array, t);
	}
	obs_data_set_string(t, "id", obs_source_get_unversioned_id(filter));
	auto s = obs_data_get_obj(t, "settings");
	if (s == nullptr) {
		s = obs_data_create();
		obs_data_set_obj(t, "settings", s);
	}

	if (auto settings = obs_source_get_settings(filter)) {
		obs_data_apply(s, settings);
		obs_data_release(settings);
	}

	obs_data_release(s);
}

void DeviceSwitcherDock::RemoveSourceSettings(QString sourceName)
{
	if (!config)
		return;
	const auto array = obs_data_get_array(config, "sources");
	if (!array)
		return;
	auto count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(array, i);
		if (!item)
			continue;
		auto name =
			QString::fromUtf8(obs_data_get_string(item, "name"));
		obs_data_release(item);
		if (name == sourceName) {
			obs_data_array_erase(array, i);
			obs_data_array_release(array);
			return;
		}
	}
	obs_data_array_release(array);
}

void DeviceSwitcherDock::LoadSourceSettings(obs_source_t *source)
{
	if (!config || !source)
		return;
	auto array = obs_data_get_array(config, "sources");
	if (!array) {
		array = obs_data_array_create();
		obs_data_set_array(config, "sources", array);
	}
	obs_data_t *t = nullptr;
	auto source_name = obs_source_get_name(source);
	auto count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(array, i);
		if (!item)
			continue;
		auto name = obs_data_get_string(item, "name");
		if (strcmp(name, source_name) == 0) {
			t = item;
			break;
		}
		obs_data_release(item);
	}
	obs_data_array_release(array);
	if (t == nullptr)
		return;

	if (strcmp(obs_data_get_string(t, "id"),
		   obs_source_get_unversioned_id(source)) != 0) {
		obs_data_release(t);
		return;
	}
	auto s = obs_data_get_obj(t, "settings");
	if (s) {
		obs_source_update(source, s);
		obs_data_release(s);
	}

	auto filters = obs_data_get_array(t, "filters");
	if (filters) {
		obs_source_enum_filters(source, RemoveFilter, filters);
		auto count = obs_data_array_count(filters);
		for (size_t i = 0; i < count; i++) {
			auto item = obs_data_array_item(filters, i);
			if (!item)
				continue;
			LoadFilter(source, item);
			obs_data_release(item);
		}
		obs_data_array_release(filters);
	}
}

void DeviceSwitcherDock::RemoveFilter(obs_source_t *source,
				      obs_source_t *filter, void *param)
{
	auto array = (obs_data_array_t *)param;
	auto source_name = obs_source_get_name(filter);
	auto count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(array, i);
		if (!item)
			continue;
		auto name = obs_data_get_string(item, "name");
		obs_data_release(item);
		if (strcmp(name, source_name) == 0)
			return;
	}
	obs_source_filter_remove(source, filter);
}

void DeviceSwitcherDock::LoadFilter(obs_source_t *parent,
				    obs_data_t *filter_data)
{
	auto name = obs_data_get_string(filter_data, "name");
	auto id = obs_data_get_string(filter_data, "id");
	auto settings = obs_data_get_obj(filter_data, "settings");
	auto filter = obs_source_get_filter_by_name(parent, name);
	if (!filter) {
		filter = obs_source_create(id, name, settings, nullptr);
		obs_source_filter_add(parent, filter);
		obs_source_load(filter);
	} else {
		obs_source_update(filter, settings);
	}
	obs_data_release(settings);
	obs_source_release(filter);
}

DeviceSwitcherDock::DeviceSwitcherDock(QWidget *parent)
	: QDockWidget(parent),
	  mainLayout(new QVBoxLayout(this)),
	  monitoringCombo(nullptr),
	  config(nullptr)
{
	setFeatures(DockWidgetMovable | DockWidgetFloatable);
	setWindowTitle(QT_UTF8(obs_module_text("DeviceSwitcher")));
	setObjectName("DeviceSwitcherDock");
	setFloating(true);
	hide();

	auto scrollArea = new QScrollArea(this);
	scrollArea->setObjectName(QStringLiteral("scrollArea"));
	scrollArea->setWidgetResizable(true);

	auto w = new QWidget(scrollArea);
	QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	sizePolicy.setHorizontalStretch(0);
	sizePolicy.setVerticalStretch(0);
	sizePolicy.setHeightForWidth(w->sizePolicy().hasHeightForWidth());
	w->setSizePolicy(sizePolicy);
	w->setLayout(mainLayout);
	scrollArea->setWidget(w);

	setWidget(scrollArea);

	auto nameLabel = new QLabel(w);
	nameLabel->setText(QString::fromUtf8(obs_module_text("AudioMonitor")));
	mainLayout->addWidget(nameLabel);

	monitoringCombo = new QComboBox(w);
	monitoringCombo->addItem(QString::fromUtf8(obs_module_text("Default")),
				 QString::fromUtf8("default"));
	obs_enum_audio_monitoring_devices(add_monitoring_device, this);
	const char *name;
	const char *id;
	obs_get_audio_monitoring_device(&name, &id);
	monitoringCombo->setCurrentText(QString::fromUtf8(name));

	auto comboIndexChanged = static_cast<void (QComboBox::*)(int)>(
		&QComboBox::currentIndexChanged);
	connect(monitoringCombo, comboIndexChanged, [this](int index) {
		auto id = monitoringCombo->itemData(index).toString().toUtf8();
		auto name = monitoringCombo->itemText(index).toUtf8();
		obs_set_audio_monitoring_device(name.constData(),
						id.constData());
		const auto config = obs_frontend_get_profile_config();
		if (!config)
			return config_set_string(config, "Audio",
						 "MonitoringDeviceName",
						 name.constData());
		config_set_string(config, "Audio", "MonitoringDeviceId",
				  id.constData());
		config_save(config);
	});
	mainLayout->addWidget(monitoringCombo);

	if (char *file = obs_module_config_path("config.json")) {
		config = obs_data_create_from_json_file(file);
		bfree(file);
	}
	if (!config)
		config = obs_data_create();

	auto sh = obs_get_signal_handler();
	signal_handler_connect(sh, "source_create", add_source, this);
	signal_handler_connect(sh, "source_load", add_source, this);
	signal_handler_connect(sh, "source_save", save_source, this);
	signal_handler_connect(sh, "source_destroy", remove_source, this);
	signal_handler_connect(sh, "source_remove", remove_source, this);
	signal_handler_connect(sh, "source_rename", rename_source, this);

	obs_frontend_add_event_callback(frontend_event, this);
}

DeviceSwitcherDock::~DeviceSwitcherDock()
{
	if (config) {
		if (char *file = obs_module_config_path("config.json")) {
			if (!obs_data_save_json(config, file)) {
				if (char *path = obs_module_config_path("")) {
					os_mkdirs(path);
					bfree(path);
				}
				obs_data_save_json(config, file);
				bfree(file);
			}
			obs_data_release(config);
		}
	}
	obs_frontend_remove_event_callback(frontend_event, this);

	auto sh = obs_get_signal_handler();
	signal_handler_disconnect(sh, "source_create", add_source, this);
	signal_handler_disconnect(sh, "source_load", add_source, this);
	signal_handler_disconnect(sh, "source_destroy", remove_source, this);
	signal_handler_disconnect(sh, "source_remove", remove_source, this);
	signal_handler_disconnect(sh, "source_rename", rename_source, this);
}

void DeviceSwitcherDock::AddDeviceSource(QString sourceName)
{
	const auto count = mainLayout->count();
	for (int i = count - 1; i >= 0; i--) {
		QLayoutItem *item = mainLayout->itemAt(i);
		if (!item)
			continue;
		auto *w = item->widget();
		if (w->objectName() == sourceName) {
			return;
		}
	}
	auto source = obs_get_source_by_name(sourceName.toUtf8().constData());
	if (!source)
		return;
	obs_source_release(source);

	auto props = obs_source_properties(source);
	if (!props)
		return;

	auto prop = obs_properties_get(props, "device_id");
	if (!prop)
		prop = obs_properties_get(props, "video_device_id");

	if (!prop) {
		obs_properties_destroy(props);
		return;
	}

	const auto propCount = obs_property_list_item_count(prop);
	if (propCount == 0) {
		obs_properties_destroy(props);
		return;
	}

	LoadSourceSettings(source);

	auto settings = obs_source_get_settings(source);
	if (!settings) {
		obs_properties_destroy(props);
		return;
	}

	auto settingName = obs_property_name(prop);
	auto deviceId = obs_data_get_string(settings, settingName);
	QString settingNameString = QString::fromUtf8(settingName);

	obs_data_release(settings);

	auto w = new QWidget(this);
	w->setObjectName(sourceName);
	w->setContentsMargins(0, 0, 0, 0);

	auto l = new QVBoxLayout(w);
	l->setContentsMargins(0, 0, 0, 0);
	auto nameLabel = new QLabel(w);
	nameLabel->setText(sourceName);
	l->addWidget(nameLabel);

	auto combo = new QComboBox(w);

	for (size_t i = 0; i < propCount; i++) {
		auto valName = obs_property_list_item_name(prop, i);
		auto dId = obs_property_list_item_string(prop, i);
		combo->addItem(valName, dId);
		if (strcmp(dId, deviceId) == 0) {
			combo->setCurrentIndex(i);
		}
	}
	obs_properties_destroy(props);

	l->addWidget(combo);
	auto comboIndexChanged = static_cast<void (QComboBox::*)(int)>(
		&QComboBox::currentIndexChanged);
	connect(combo, comboIndexChanged,
		[combo, w, settingNameString](int index) {
			auto id = combo->itemData(index).toString();
			auto sourceName = w->objectName();
			auto source = obs_get_source_by_name(
				sourceName.toUtf8().constData());
			if (!source)
				return;
			auto settings = obs_data_create();
			auto uv = id.toUtf8();
			auto v = uv.constData();
			auto us = settingNameString.toUtf8();
			auto s = us.constData();
			obs_data_set_string(settings, s, v);
			obs_source_update(source, settings);
			obs_data_release(settings);
			obs_source_release(source);
		});
	auto bw = new QWidget(w);
	bw->setContentsMargins(0, 0, 0, 0);
	auto hl = new QHBoxLayout(bw);
	hl->setMargin(0);
	hl->setContentsMargins(0, 0, 0, 0);

	auto pb = new QPushButton(bw);
	pb->setText(obs_module_text("Properties"));
	connect(pb, &QPushButton::clicked, [w]() {
		auto sourceName = w->objectName();
		auto source =
			obs_get_source_by_name(sourceName.toUtf8().constData());
		if (!source)
			return;
		obs_frontend_open_source_properties(source);
		obs_source_release(source);
	});
	hl->addWidget(pb);

	auto filter = new QPushButton(bw);
	filter->setText(obs_module_text("Filters"));
	connect(filter, &QPushButton::clicked, [w]() {
		auto sourceName = w->objectName();
		auto source =
			obs_get_source_by_name(sourceName.toUtf8().constData());
		if (!source)
			return;
		obs_frontend_open_source_filters(source);
		obs_source_release(source);
	});
	hl->addWidget(filter);

	auto restart = new QPushButton(bw);
	restart->setText(obs_module_text("Restart"));
	connect(restart, &QPushButton::clicked, [combo, w, settingNameString]() {
		auto sourceName = w->objectName();
		auto source =
			obs_get_source_by_name(sourceName.toUtf8().constData());
		if (!source)
			return;
		auto settings = obs_data_create();
		auto us = settingNameString.toUtf8();
		auto s = us.constData();
		obs_data_set_string(settings, s, "");
		obs_source_update(source, settings);
		auto id = combo->itemData(combo->currentIndex()).toString();
		auto uv = id.toUtf8();
		auto v = uv.constData();
		obs_data_set_string(settings, s, v);
		obs_source_update(source, settings);
		obs_data_release(settings);
		obs_source_release(source);
	});
	hl->addWidget(restart);

	auto checkbox = new QCheckBox(
		QString::fromUtf8(obs_module_text("Monitor")), bw);
	checkbox->setChecked(obs_source_get_monitoring_type(source) !=
			     OBS_MONITORING_TYPE_NONE);
	connect(checkbox, &QCheckBox::stateChanged, [w, checkbox](int state) {
		auto sourceName = w->objectName();
		auto source =
			obs_get_source_by_name(sourceName.toUtf8().constData());
		if (!source)
			return;
		obs_source_set_monitoring_type(
			source, checkbox->isChecked()
					? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT
					: OBS_MONITORING_TYPE_NONE);
		obs_source_release(source);
	});

	hl->addWidget(checkbox);

	checkbox =
		new QCheckBox(QString::fromUtf8(obs_module_text("Retain")), bw);
	checkbox->setChecked(HasSourceSettings(sourceName));
	connect(checkbox, &QCheckBox::stateChanged,
		[this, w, checkbox](int state) {
			auto sourceName = w->objectName();
			if (checkbox->isChecked()) {
				auto source = obs_get_source_by_name(
					sourceName.toUtf8().constData());
				if (!source)
					return;
				SaveSourceSettings(source);
				obs_source_release(source);
			} else {
				RemoveSourceSettings(sourceName);
			}
		});
	hl->addWidget(checkbox);

	l->addWidget(bw);
	w->setLayout(l);
	mainLayout->addWidget(w);
}

void DeviceSwitcherDock::RemoveDeviceSource(QString sourceName)
{
	const auto count = mainLayout->count();
	for (int i = 0; i < count; i++) {
		QLayoutItem *item = mainLayout->itemAt(i);
		if (!item)
			continue;
		auto *w = item->widget();
		if (w->objectName() == sourceName) {
			mainLayout->removeItem(item);
			delete w;
			break;
		}
	}
}

void DeviceSwitcherDock::RenameDeviceSource(QString prevDeviceName,
					    QString newDeviceName)
{
	const auto count = mainLayout->count();
	for (int i = 0; i < count; i++) {
		QLayoutItem *item = mainLayout->itemAt(i);
		if (!item)
			continue;
		auto *w = item->widget();
		if (w->objectName() == prevDeviceName) {
			w->setObjectName(newDeviceName);
			auto lw = w->layout()->itemAt(0)->widget();
			auto l = dynamic_cast<QLabel *>(lw);
			if (l) {
				l->setText(newDeviceName);
			}
			break;
		}
	}
}
