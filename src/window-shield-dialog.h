/*
window-shield-for-obs
Copyright (C) <2026> <jiejiebiezheyang> <1964234252@qq.com>

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

#pragma once

#include <QDialog>
#include <QSet>

#include <windows.h>

class QListWidget;
class QListWidgetItem;
class QTimer;

class WindowShieldDialog : public QDialog {
	Q_OBJECT

public:
	explicit WindowShieldDialog(QWidget *parent = nullptr);
	~WindowShieldDialog();
	static bool s_opened;

protected:
	void closeEvent(QCloseEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void showEvent(QShowEvent *event) override;

private slots:
	void refreshWindows();
	void onItemChanged(QListWidgetItem *item);

private:
	QListWidget *windowList = nullptr;
	QTimer *refreshTimer = nullptr;
	bool updatingList = false;
};