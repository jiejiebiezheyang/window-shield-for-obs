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

#include "window-shield-dialog.h"

#include <QListWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QScrollBar>
#include <QMessageBox>
#include "WindowHandler.h"
#include <qevent.h>

bool WindowShieldDialog::s_opened = false;

void WindowShieldDialog::closeEvent(QCloseEvent *event)
{
	// 窗口关闭前的处理
	qDebug() << "窗口正在关闭...";

	// 停止定时器
	if (refreshTimer) {
		refreshTimer->stop();
	}

	// 清理资源（如果需要）
	WindowShieldDialog::s_opened = false;

	// 接受关闭事件（默认行为）
	event->accept();

	// 如果想阻止关闭：
	// event->ignore();  // 窗口不会关闭
}

void WindowShieldDialog::hideEvent(QHideEvent *event)
{
	qDebug() << "窗口被隐藏";
	// 窗口隐藏时的处理
	if (refreshTimer) {
		refreshTimer->stop(); // 隐藏时停止刷新
	}
	WindowShieldDialog::s_opened = false;
	QDialog::hideEvent(event);
}

void WindowShieldDialog::showEvent(QShowEvent *event)
{
	qDebug() << "窗口被显示";
	WindowShieldDialog::s_opened = true;
	// 窗口显示时的处理
	if (refreshTimer) {
		refreshTimer->start(500); // 显示时恢复刷新
	}
	QDialog::showEvent(event);
}

WindowShieldDialog::WindowShieldDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("窗口保护");
	setMinimumSize(600, 400);

	windowList = new QListWidget(this);
	windowList->setSelectionMode(QAbstractItemView::SingleSelection);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(windowList);

	// 勾选 / 取消勾选立即生效
	connect(windowList, &QListWidget::itemChanged, this, &WindowShieldDialog::onItemChanged);

	// 自动刷新
	refreshTimer = new QTimer(this);
	connect(refreshTimer, &QTimer::timeout, this, &WindowShieldDialog::refreshWindows);

	// 打开窗口立即刷新
	refreshWindows();

	WindowShieldDialog::s_opened = true;
	// 每 2 秒刷新一次（降低频率减少冲突）
	refreshTimer->start(500);
}

void WindowShieldDialog::refreshWindows()
{
	// 如果正在处理用户勾选，跳过刷新
	if (updatingList) {
		return;
	}

	const auto windows = WindowHandler::GetVisibleWindows();

	// 标记正在更新列表
	updatingList = true;

	// 保存滚动位置
	const int scrollValue = windowList->verticalScrollBar()->value();

	// 清空列表
	windowList->clear();

	for (const auto &window : windows) {
		if (!IsWindow(window.handle))
			continue;

		// 如果状态不一致，以实际 API 调用结果为准
		bool actualProtected = WindowHandler::GetWindowDisplayAffinity(window.handle);

		auto *item = new QListWidgetItem();
		item->setText(
			QString("%1 - %2").arg(window.title).arg(window.title)); // 注意：这里标题重复了，建议改为 handle

		// 存储窗口句柄
		item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(window.handle)));

		// 设置可勾选
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

		// 根据实际的保护状态设置勾选
		item->setCheckState(actualProtected ? Qt::Checked : Qt::Unchecked);

		windowList->addItem(item);
	}

	// 恢复滚动位置
	windowList->verticalScrollBar()->setValue(scrollValue);

	// 取消更新标记
	updatingList = false;
}

void WindowShieldDialog::onItemChanged(QListWidgetItem *item)
{
	// 如果正在程序化更新列表，忽略此信号
	if (updatingList || !item) {
		return;
	}

	HWND hwnd = reinterpret_cast<HWND>(item->data(Qt::UserRole).value<quintptr>());

	if (!IsWindow(hwnd)) {
		// 窗口已关闭，恢复勾选状态为 Unchecked
		updatingList = true;
		item->setCheckState(Qt::Unchecked);
		updatingList = false;
		return;
	}

	bool newState = (item->checkState() == Qt::Checked);

	if (newState) {
		// 尝试保护
		WindowHandler::SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
	} else {
		// 取消保护
		WindowHandler::SetWindowDisplayAffinity(hwnd, WDA_NONE);
	}
}

WindowShieldDialog::~WindowShieldDialog()
{
	WindowShieldDialog::s_opened = false;
}