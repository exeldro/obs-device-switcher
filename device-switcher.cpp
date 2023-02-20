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
#include "volume-meter.hpp"
#include "util/config-file.h"
#include "util/platform.h"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("device-switcher", "en-US")

#define LOG_OFFSET_DB 6.0f
#define LOG_RANGE_DB 96.0f
/* equals -log10f(LOG_OFFSET_DB) */
#define LOG_OFFSET_VAL -0.77815125038364363f
/* equals -log10f(-LOG_RANGE_DB + LOG_OFFSET_DB) */
#define LOG_RANGE_VAL -2.00860017176191756f

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

	QString deviceName = QString::fromUtf8(obs_source_get_name(source));

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
	auto prev_name = calldata_string(calldata, "prev_name");
	auto new_name = calldata_string(calldata, "new_name");
	if (!prev_name || !new_name)
		return;
	QString prevDeviceName = QT_UTF8(prev_name);
	QString newDeviceName = QT_UTF8(new_name);
	if (prevDeviceName.isEmpty() || newDeviceName.isEmpty())
		return;
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
	add_source(p, calldata);
	auto dock = (DeviceSwitcherDock *)p;
	if (!dock->retain_config)
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
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		if (obs_frontend_virtualcam_active()) {
			obs_frontend_stop_virtualcam();
			const auto dock =
				static_cast<DeviceSwitcherDock *>(data);
			dock->restart_virtual_camera = true;
		}
	} else if (event == OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED) {
		const auto dock = static_cast<DeviceSwitcherDock *>(data);
		if (dock->virtualCamera)
			dock->virtualCamera->setChecked(true);
		dock->restart_virtual_camera = false;
	} else if (event == OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED) {
		const auto dock = static_cast<DeviceSwitcherDock *>(data);
		if (dock->virtualCamera)
			dock->virtualCamera->setChecked(false);
		if (dock->restart_virtual_camera) {
			obs_frontend_start_virtualcam();
		}
	}
}

bool DeviceSwitcherDock::HasSourceSettings(QString sourceName)
{
	if (!retain_config)
		return false;
	const auto array = obs_data_get_array(retain_config, "sources");
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
	if (!retain_config || !source)
		return;
	auto array = obs_data_get_array(retain_config, "sources");
	if (!array) {
		array = obs_data_array_create();
		obs_data_set_array(retain_config, "sources", array);
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
	UNUSED_PARAMETER(source);
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
	obs_data_release(t);
	obs_data_release(s);
}

void DeviceSwitcherDock::RemoveSourceSettings(QString sourceName)
{
	if (!retain_config)
		return;
	const auto array = obs_data_get_array(retain_config, "sources");
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
	if (!retain_config || !source)
		return;
	auto array = obs_data_get_array(retain_config, "sources");
	if (!array) {
		array = obs_data_array_create();
		obs_data_set_array(retain_config, "sources", array);
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
	obs_data_release(t);
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

QIcon GetIconFromType(enum obs_icon_type icon_type)
{
	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

	switch (icon_type) {
	case OBS_ICON_TYPE_IMAGE:
		return main_window->property("imageIcon").value<QIcon>();
	case OBS_ICON_TYPE_COLOR:
		return main_window->property("colorIcon").value<QIcon>();
	case OBS_ICON_TYPE_SLIDESHOW:
		return main_window->property("slideshowIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_INPUT:
		return main_window->property("audioInputIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		return main_window->property("audioOutputIcon").value<QIcon>();
	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		return main_window->property("desktopCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		return main_window->property("windowCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_GAME_CAPTURE:
		return main_window->property("gameCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_CAMERA:
		return main_window->property("cameraIcon").value<QIcon>();
	case OBS_ICON_TYPE_TEXT:
		return main_window->property("textIcon").value<QIcon>();
	case OBS_ICON_TYPE_MEDIA:
		return main_window->property("mediaIcon").value<QIcon>();
	case OBS_ICON_TYPE_BROWSER:
		return main_window->property("browserIcon").value<QIcon>();
	case OBS_ICON_TYPE_CUSTOM:
		//TODO: Add ability for sources to define custom icons
		return main_window->property("defaultIcon").value<QIcon>();
	case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
		return main_window->property("audioProcessOutputIcon")
			.value<QIcon>();
	default:
		return main_window->property("defaultIcon").value<QIcon>();
	}
}

DeviceSwitcherDock::DeviceSwitcherDock(QWidget *parent)
	: QDockWidget(parent),
	  mainLayout(new QVBoxLayout(this)),
	  monitoringCombo(nullptr),
	  retain_config(nullptr)
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

	char *config_file = obs_module_file("config/config.ini");
	if (!config_file) {
		config_file = obs_module_config_path("config.ini");
	}
	if (config_file) {
		config_open(&show_config, config_file, CONFIG_OPEN_EXISTING);
		bfree(config_file);
	}
	if (!show_config ||
	    !config_has_user_value(show_config, "General", "VirtualCamera") ||
	    config_get_bool(show_config, "General", "VirtualCamera")) {
		virtualCamera = new QPushButton(
			QString::fromUtf8(obs_module_text("VirtualCamera")));
		virtualCamera->setCheckable(true);
		connect(virtualCamera, &QPushButton::clicked, [this] {
			const bool checked = virtualCamera->isChecked();
			if (checked != obs_frontend_virtualcam_active()) {
				if (checked)
					obs_frontend_start_virtualcam();
				else
					obs_frontend_stop_virtualcam();
			}
		});
		mainLayout->addWidget(virtualCamera);
	}

	if (!show_config ||
	    !config_has_user_value(show_config, "General", "AudioMonitor") ||
	    config_get_bool(show_config, "General", "AudioMonitor")) {

		const auto nameRow = new QHBoxLayout;
		if (!show_config ||
		    !config_has_user_value(show_config, "General", "Icon") ||
		    config_get_bool(show_config, "General", "Icon")) {
			const auto iconLabel = new QLabel(w);
			iconLabel->setPixmap(
				GetIconFromType(OBS_ICON_TYPE_AUDIO_OUTPUT)
					.pixmap(16, 16));
			nameRow->addWidget(iconLabel);
		}

		const auto nameLabel = new QLabel(w);
		nameLabel->setText(
			QString::fromUtf8(obs_module_text("AudioMonitor")));
		nameRow->addWidget(nameLabel, 1);

		if (nameRow->count())
			mainLayout->addLayout(nameRow);

		monitoringCombo = new QComboBox(w);
		monitoringCombo->addItem(
			QString::fromUtf8(obs_module_text("Default")),
			QString::fromUtf8("default"));
		obs_enum_audio_monitoring_devices(add_monitoring_device, this);
		const char *name;
		const char *id;
		obs_get_audio_monitoring_device(&name, &id);
		monitoringCombo->setCurrentText(QString::fromUtf8(name));

		auto comboIndexChanged = static_cast<void (QComboBox::*)(int)>(
			&QComboBox::currentIndexChanged);
		connect(monitoringCombo, comboIndexChanged, [this](int index) {
			auto id = monitoringCombo->itemData(index)
					  .toString()
					  .toUtf8();
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
	}
	if (char *file = obs_module_config_path("config.json")) {
		retain_config =
			obs_data_create_from_json_file_safe(file, "bak");
		bfree(file);
	}
	if (!retain_config)
		retain_config = obs_data_create();

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
	if (retain_config) {
		if (char *file = obs_module_config_path("config.json")) {
			if (!obs_data_save_json_safe(retain_config, file, "tmp",
						     "bak")) {
				if (char *path = obs_module_config_path("")) {
					os_mkdirs(path);
					bfree(path);
				}
				obs_data_save_json_safe(retain_config, file,
							"tmp", "bak");
				bfree(file);
			}
		}
		obs_data_release(retain_config);
	}
	config_close(show_config);
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
		const auto *w = item->widget();
		if (w && w->objectName() == sourceName) {
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

	auto w = new DeviceWidget(source, prop, show_config, this);
	mainLayout->addWidget(w);
	obs_properties_destroy(props);
}

void DeviceSwitcherDock::RemoveDeviceSource(QString sourceName)
{
	const auto count = mainLayout->count();
	for (int i = 0; i < count; i++) {
		QLayoutItem *item = mainLayout->itemAt(i);
		if (!item)
			continue;
		auto *w = item->widget();
		if (w && w->objectName() == sourceName) {
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
		if (w && w->objectName() == prevDeviceName) {
			w->setObjectName(newDeviceName);
			auto subItem = w->layout()->itemAt(0);
			if (subItem) {
				auto lw = subItem->widget();
				auto l = dynamic_cast<QLabel *>(lw);
				if (l)
					l->setText(newDeviceName);
			}
			break;
		}
	}
}

DeviceWidget::DeviceWidget(obs_source_t *source, obs_property_t *prop,
			   config_t *sc, DeviceSwitcherDock *parent)
	: QWidget(parent)
{
	dock = parent;
	this->source = obs_source_get_weak_source(source);
	auto sn = obs_source_get_name(source);
	auto st = obs_source_get_unversioned_id(source);
	const QString sourceName = QString::fromUtf8(sn);

	setObjectName(sourceName);
	setContentsMargins(0, 0, 0, 0);

	auto l = new QVBoxLayout(this);
	l->setContentsMargins(0, 0, 0, 0);

	const auto nameRow = new QHBoxLayout;
	if (GetShowSetting(sc, st, sn, "Icon")) {
		const auto iconLabel = new QLabel(this);
		auto icon = GetIconFromType(
			obs_source_get_icon_type(obs_source_get_id(source)));
		iconLabel->setPixmap(icon.pixmap(16, 16));
		nameRow->addWidget(iconLabel);
	}
	const auto nameLabel = new QLabel(this);
	nameLabel->setText(sourceName);
	nameRow->addWidget(nameLabel, 1);

	l->addLayout(nameRow);
	auto settingName = obs_property_name(prop);
	QString settingNameString = QString::fromUtf8(settingName);
	if (GetShowSetting(sc, st, sn, "Device")) {

		auto settings = obs_source_get_settings(source);
		auto deviceId =
			settings ? obs_data_get_string(settings, settingName)
				 : nullptr;
		obs_data_release(settings);

		auto combo = new QComboBox(this);
		const auto propCount = obs_property_list_item_count(prop);
		for (size_t i = 0; i < propCount; i++) {
			auto valName = obs_property_list_item_name(prop, i);
			auto dId = obs_property_list_item_string(prop, i);
			combo->addItem(valName, dId);
			if (deviceId && strcmp(dId, deviceId) == 0) {
				combo->setCurrentIndex((int)i);
			}
		}

		l->addWidget(combo);
		auto comboIndexChanged = static_cast<void (QComboBox::*)(int)>(
			&QComboBox::currentIndexChanged);
		connect(combo, comboIndexChanged,
			[combo, this, settingNameString](int index) {
				auto id = combo->itemData(index).toString();
				auto source = obs_weak_source_get_source(
					this->source);
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
	}
	auto bw = new QWidget(this);
	bw->setObjectName(QStringLiteral("contextContainer"));
	bw->setContentsMargins(0, 0, 0, 0);
	auto hl = new QHBoxLayout(bw);
	hl->setContentsMargins(0, 0, 0, 0);
	if (GetShowSetting(sc, st, sn, "Properties")) {
		auto pb = new QPushButton(bw);
		pb->setObjectName(QStringLiteral("sourcePropertiesButton"));
		auto t = obs_module_text("PropertiesButton");
		if (strcmp(t, "PropertiesButton") != 0) {
			pb->setText(t);
		} else {
			pb->setFixedSize(pb->size().height(),
					 pb->size().height());
		}
		connect(pb, &QPushButton::clicked, [this]() {
			auto source = obs_weak_source_get_source(this->source);
			if (!source)
				return;
			obs_frontend_open_source_properties(source);
			obs_source_release(source);
		});
		hl->addWidget(pb);
	}
	if (GetShowSetting(sc, st, sn, "Filters")) {
		auto filter = new QPushButton(bw);
		filter->setObjectName(QStringLiteral("sourceFiltersButton"));
		auto t = obs_module_text("FiltersButton");
		if (strcmp(t, "FiltersButton") != 0) {
			filter->setText(t);
		} else {
			filter->setFixedSize(filter->size().height(),
					     filter->size().height());
		}
		connect(filter, &QPushButton::clicked, [this]() {
			auto source = obs_weak_source_get_source(this->source);
			if (!source)
				return;
			obs_frontend_open_source_filters(source);
			obs_source_release(source);
		});
		hl->addWidget(filter);
	}
	if (GetShowSetting(sc, st, sn, "Restart")) {
		auto restart = new QPushButton(bw);
		restart->setProperty("themeID", "restartIcon");
		auto t = obs_module_text("RestartButton");
		if (strcmp(t, "RestartButton") != 0) {
			restart->setText(t);
		} else {
			restart->setFixedSize(restart->size().height(),
					      restart->size().height());
		}
		connect(restart, &QPushButton::clicked,
			[this, settingNameString]() {
				auto source = obs_weak_source_get_source(
					this->source);
				if (!source)
					return;
				auto settings = obs_source_get_settings(source);

				auto us = settingNameString.toUtf8();
				auto s = us.constData();
				auto v = obs_data_get_string(settings, s);
				obs_data_set_string(settings, s, "");
				obs_source_update(source, nullptr);
				obs_data_set_string(settings, s, v);
				obs_source_update(source, nullptr);
				obs_data_release(settings);
				obs_source_release(source);
			});
		hl->addWidget(restart);
	}

	if ((obs_source_get_output_flags(source) & OBS_OUTPUT_AUDIO) ==
		    OBS_OUTPUT_AUDIO &&
	    obs_source_audio_active(source) &&
	    GetShowSetting(sc, st, sn, "Monitor")) {
		auto monitor = new QCheckBox(
			QString::fromUtf8(obs_module_text("Monitor")), bw);
		monitor->setChecked(obs_source_get_monitoring_type(source) !=
				    OBS_MONITORING_TYPE_NONE);
		connect(monitor, &QCheckBox::stateChanged,
			[this, monitor](int state) {
				UNUSED_PARAMETER(state);
				const auto sourceName = objectName();
				const auto source = obs_get_source_by_name(
					sourceName.toUtf8().constData());
				if (!source)
					return;
				obs_source_set_monitoring_type(
					source,
					monitor->isChecked()
						? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT
						: OBS_MONITORING_TYPE_NONE);
				obs_source_release(source);
			});

		hl->addWidget(monitor);
	}
	if (GetShowSetting(sc, st, sn, "Retain")) {
		auto retain = new QCheckBox(
			QString::fromUtf8(obs_module_text("Retain")), bw);
		retain->setChecked(dock->HasSourceSettings(sourceName));
		connect(retain, &QCheckBox::stateChanged,
			[this, retain](int state) {
				UNUSED_PARAMETER(state);
				auto sourceName = objectName();
				if (retain->isChecked()) {
					auto source = obs_get_source_by_name(
						sourceName.toUtf8().constData());
					if (!source)
						return;
					dock->SaveSourceSettings(source);
					obs_source_release(source);
				} else {
					dock->RemoveSourceSettings(sourceName);
				}
			});
		hl->addWidget(retain);
	}

	l->addWidget(bw);
	if ((obs_source_get_output_flags(source) & OBS_OUTPUT_AUDIO) ==
		    OBS_OUTPUT_AUDIO &&
	    obs_source_audio_active(source)) {
		if (GetShowSetting(sc, st, sn, "VolumeMeter")) {
			auto volMeter = new VolumeMeter(nullptr, source);
			volMeter->setSizePolicy(QSizePolicy::Minimum,
						QSizePolicy::Fixed);

			l->addWidget(volMeter);
		}
		if (GetShowSetting(sc, st, sn, "VolumeSlider")) {
			auto volControl = new QWidget;
			volControl->setContentsMargins(0, 0, 0, 0);
			auto *audioLayout = new QHBoxLayout(this);

			/*
			locked = new LockedCheckBox();
			locked->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
			locked->setFixedSize(16, 16);

			locked->setStyleSheet("background: none");

			connect(locked, &QCheckBox::stateChanged, this,
				&SourceDock::LockVolumeControl, Qt::DirectConnection);*/

			slider = new SliderIgnoreScroll(Qt::Horizontal);
			slider->setSizePolicy(QSizePolicy::Expanding,
					      QSizePolicy::Preferred);
			slider->setMinimum(0);
			slider->setMaximum(10000);
			slider->setToolTip(
				QT_UTF8(obs_module_text("VolumeSlider")));

			connect(slider, SIGNAL(valueChanged(int)), this,
				SLOT(SliderChanged(int)));

			mute = new MuteCheckBox();

			connect(mute, &QCheckBox::stateChanged,
				[this](bool mute) {
					auto source =
						obs_weak_source_get_source(
							this->source);
					if (source &&
					    obs_source_muted(source) != mute)
						obs_source_set_muted(source,
								     mute);
					obs_source_release(source);
				});

			const auto sh = obs_source_get_signal_handler(source);
			signal_handler_connect(sh, "mute", OBSMute, this);
			signal_handler_connect(sh, "volume", OBSVolume, this);

			audioLayout->addWidget(slider);
			audioLayout->addWidget(mute);

			volControl->setLayout(audioLayout);
			l->addWidget(volControl);
		}
	}
	setLayout(l);
	if (mute || slider)
		UpdateVolControls();
}

bool DeviceWidget::GetShowSetting(config_t *config, const char *st,
				  const char *sn, const char *setting)
{
	if (!config)
		return true;
	if (config_has_user_value(config, sn, setting))
		return config_get_bool(config, sn, setting);
	if (config_has_user_value(config, st, setting))
		return config_get_bool(config, st, setting);
	if (config_has_user_value(config, "General", setting))
		return config_get_bool(config, "General", setting);
	return true;
}

DeviceWidget::~DeviceWidget()
{
	auto s = obs_weak_source_get_source(source);
	if (s) {
		const auto sh = obs_source_get_signal_handler(s);
		signal_handler_disconnect(sh, "mute", OBSMute, this);
		signal_handler_disconnect(sh, "volume", OBSVolume, this);
		obs_source_release(s);
	}
	obs_weak_source_release(source);
}

void DeviceWidget::SliderChanged(int v)
{
	auto s = obs_weak_source_get_source(source);
	if (!s)
		return;
	float def = (float)v / 10000.0f;
	float db;
	if (def >= 1.0f)
		db = 0.0f;
	else if (def <= 0.0f)
		db = -INFINITY;
	else
		db = -(LOG_RANGE_DB + LOG_OFFSET_DB) *
			     powf((LOG_RANGE_DB + LOG_OFFSET_DB) /
					  LOG_OFFSET_DB,
				  -def) +
		     LOG_OFFSET_DB;
	const float mul = obs_db_to_mul(db);
	const float vol = obs_source_get_volume(s);
	float db2 = obs_mul_to_db(vol);
	if (!close_float(db, db2, 0.01f))
		obs_source_set_volume(s, mul);
	obs_source_release(s);
}

void DeviceWidget::UpdateVolControls()
{
	auto s = obs_weak_source_get_source(source);
	if (!s)
		return;
	bool lock = false;
	/*if (obs_data_t *settings =
		    source ? obs_source_get_private_settings(s)
			   : nullptr) {
		lock = obs_data_get_bool(settings, "volume_locked");
		obs_data_release(settings);
	}
	locked->setChecked(lock);*/
	if (mute) {
		mute->setEnabled(!lock);
		mute->setChecked(s ? obs_source_muted(s) : false);
	}
	if (slider) {
		slider->setEnabled(!lock);
		float mul = s ? obs_source_get_volume(s) : 0.0f;
		float db = obs_mul_to_db(mul);
		float def;
		if (db >= 0.0f)
			def = 1.0f;
		else if (db <= -96.0f)
			def = 0.0f;
		else
			def = (-log10f(-db + LOG_OFFSET_DB) - LOG_RANGE_VAL) /
			      (LOG_OFFSET_VAL - LOG_RANGE_VAL);
		slider->setValue(def * 10000.0f);
	}
	obs_source_release(s);
}

void DeviceWidget::OBSVolume(void *data, calldata_t *call_data)
{
	double volume = calldata_float(call_data, "volume");
	DeviceWidget *w = static_cast<DeviceWidget *>(data);
	QMetaObject::invokeMethod(w, "SetOutputVolume", Qt::QueuedConnection,
				  Q_ARG(double, volume));
}

void DeviceWidget::OBSMute(void *data, calldata_t *call_data)
{
	bool muted = calldata_bool(call_data, "muted");
	DeviceWidget *w = static_cast<DeviceWidget *>(data);
	QMetaObject::invokeMethod(w, "SetMute", Qt::QueuedConnection,
				  Q_ARG(bool, muted));
}

void DeviceWidget::SetOutputVolume(double volume)
{
	float db = obs_mul_to_db(volume);
	float def;
	if (db >= 0.0f)
		def = 1.0f;
	else if (db <= -96.0f)
		def = 0.0f;
	else
		def = (-log10f(-db + LOG_OFFSET_DB) - LOG_RANGE_VAL) /
		      (LOG_OFFSET_VAL - LOG_RANGE_VAL);

	int val = def * 10000.0f;
	slider->setValue(val);
}

void DeviceWidget::SetMute(bool muted)
{
	mute->setChecked(muted);
}

SliderIgnoreScroll::SliderIgnoreScroll(QWidget *parent) : QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
}

SliderIgnoreScroll::SliderIgnoreScroll(Qt::Orientation orientation,
				       QWidget *parent)
	: QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setOrientation(orientation);
}

void SliderIgnoreScroll::wheelEvent(QWheelEvent *event)
{
	if (!hasFocus())
		event->ignore();
	else
		QSlider::wheelEvent(event);
}
