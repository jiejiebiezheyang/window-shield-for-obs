/*
window-shield-for-obs
Copyright (C) <2026> <Cyan> <ltpcloud@qq.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/
#include <obs-module.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>

#include "window-shield-dialog.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "zh-CN")

static void open_window_shield(void *private_data)
{
	if (WindowShieldDialog::s_opened)
		return;
	UNUSED_PARAMETER(private_data);

	auto *dialog = new WindowShieldDialog();

	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->show();
	dialog->raise();
	dialog->activateWindow();
}

bool obs_module_load(void)
{
	// 注册菜单
	obs_frontend_add_tools_menu_item("窗口保护设置", open_window_shield, nullptr);

	obs_log(LOG_INFO, "window-shield loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "window-shield unloaded");
}
