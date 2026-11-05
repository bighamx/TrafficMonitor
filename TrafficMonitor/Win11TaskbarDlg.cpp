#include "stdafx.h"
#include "Win11TaskbarDlg.h"
#include "WindowsSettingHelper.h"

void CWin11TaskbarDlg::AdjustTaskbarWndPos(bool force_adjust)
{
    //显示器或远程桌面会话变化时，Explorer可能重新创建这些子窗口。
    const HWND notify = ::FindWindowEx(m_hTaskbar, nullptr, L"TrayNotifyWnd", nullptr);
    const HWND start = ::FindWindowEx(m_hTaskbar, nullptr, L"Start", nullptr);
    force_adjust = force_adjust || notify != m_hNotify || start != m_hStart;
    m_hNotify = notify;
    m_hStart = start;
    const CRect previous_notify{ m_rcNotify };
    const CRect previous_start{ m_rcStart };
    m_rcNotify.SetRectEmpty();
    m_rcStart.SetRectEmpty();
    if (::GetWindowRect(m_hNotify, m_rcNotify))
        m_rcNotify.OffsetRect(-m_rcTaskbar.left, -m_rcTaskbar.top);
    if (::GetWindowRect(m_hStart, m_rcStart))
        m_rcStart.OffsetRect(-m_rcTaskbar.left, -m_rcTaskbar.top);

    const bool horizontal = m_taskbar_on_top_or_bottom;
    //DPI切换也可能由系统直接移动子窗口，缓存的目标坐标不一定同步变化。
    CRect actual_rect;
    if (::GetWindowRect(GetSafeHwnd(), actual_rect))
    {
        ::MapWindowPoints(nullptr, m_hTaskbar, reinterpret_cast<LPPOINT>(&actual_rect), 2);
        force_adjust = force_adjust || actual_rect != m_rect;
    }
    //设置窗口大小
    m_rect.right = m_rect.left + m_window_width;
    m_rect.bottom = m_rect.top + m_window_height;
    if (force_adjust || m_rcNotify != previous_notify || m_rcStart != previous_start)
    {
        if (!horizontal)
        {
            //竖向任务栏的开始区域在上方，通知区域在下方；坐标均相对于任务栏窗口。
            if (theApp.m_taskbar_data.tbar_wnd_on_left)
            {
                const int start_bottom = m_rcStart.IsRectEmpty() ? DPI(88) : m_rcStart.bottom;
                m_rect.MoveToY(theApp.m_taskbar_data.tbar_wnd_snap ? start_bottom + DPI(2) : DPI(2));
            }
            else
            {
                //副屏可能没有 TrayNotifyWnd，保留系统时间区域的位置。
                const int notify_top = m_rcNotify.IsRectEmpty() ? m_rcTaskbar.Height() - DPI(88) : m_rcNotify.top;
                m_rect.MoveToY(notify_top - m_rect.Height() - DPI(2));
            }

            m_rect.MoveToX((m_rcTaskbar.Width() - m_rect.Width()) / 2 + DPI(theApp.m_taskbar_data.window_offset_left));
            m_rect.MoveToY(m_rect.top + DPI(theApp.m_taskbar_data.window_offset_top));
            MoveWindow(m_rect);
            return;
        }
        //任务窗口显示在右侧时，或者Windows11下任务栏左对齐时
        //（Windows11下，如果任务栏设置为左对齐，即使在“任务栏窗口设置”中设置了任务窗口显示在左边，窗口仍然显示在右边）
        if (!theApp.m_taskbar_data.tbar_wnd_on_left || !CWindowsSettingHelper::IsTaskbarCenterAlign())
        {
            ////靠近任务栏图标的情况
            //if (theApp.m_taskbar_data.tbar_wnd_snap && IsTaskbarCloseToIconEnable(theApp.m_taskbar_data.tbar_wnd_on_left))
            //{
            //    m_rect.MoveToX(m_rcMin.right + 2);
            //}
            ////靠近通知区的情况
            //else
            //{
            //通知区窗口的水平位置
            int notify_x_pos = m_rcNotify.left;
            //没有获取到通知区位置的情况
            if (notify_x_pos == 0)
            {
                //Win11副屏没有通知区窗口，这里使用固定的值（88像素的系统时间区域）
                if (m_is_secondary_display)
                    notify_x_pos = m_rcTaskbar.Width() - DPI(88);
                //如果不是副屏，但是仍然没有获取到通知区域的位置，使用配置文件中taskbar_right_space_win11指定的值
                else
                    notify_x_pos = m_rcTaskbar.Width() - DPI(theApp.m_taskbar_data.taskbar_right_space_win11);
            }
            //如果显示了小组件，并且任务栏靠左显示，则留出小组件的位置
            if (theApp.m_taskbar_data.avoid_overlap_with_widgets && CWindowsSettingHelper::IsTaskbarWidgetsBtnShown() && !CWindowsSettingHelper::IsTaskbarCenterAlign())
                m_rect.MoveToX(notify_x_pos - m_rect.Width() + 2 - DPI(theApp.m_taskbar_data.taskbar_left_space_win11));
            else
                m_rect.MoveToX(notify_x_pos - m_rect.Width() + 2);
            //}
        }
        //任务栏窗口显示在左侧时
        else
        {
            //靠近“开始”按钮
            if (theApp.m_taskbar_data.tbar_wnd_snap)
            {
                m_rect.MoveToX(m_rcStart.left - m_rect.Width() - 2);
            }
            //靠近最左侧
            else
            {
                if (CWindowsSettingHelper::IsTaskbarWidgetsBtnShown())
                    m_rect.MoveToX(2 + DPI(theApp.m_taskbar_data.taskbar_left_space_win11));
                else
                    m_rect.MoveToX(2);
            }
        }
        //水平偏移
        m_rect.MoveToX(m_rect.left + DPI(theApp.m_taskbar_data.window_offset_left));
        ////确保水平方向不超出屏幕边界
        //if (m_rect.left < 0)
        //    m_rect.MoveToX(0);
        //if (m_rcTaskbar.Width() > m_rect.Width() && m_rect.right > m_rcTaskbar.Width())
        //    m_rect.MoveToX(m_rcTaskbar.Width() - m_rect.Width());

        //触屏任务栏可能有额外的顶部区域，优先在开始按钮所在区域居中。
        //会话切换时开始按钮可能暂时不存在或仍保留旧坐标，此时在整个任务栏内居中。
        int content_top = 0;
        int content_height = m_rcTaskbar.Height();
        if (!m_rcStart.IsRectEmpty() && m_rcStart.top >= 0 && m_rcStart.bottom <= m_rcTaskbar.Height())
        {
            content_top = m_rcStart.top;
            content_height = m_rcStart.Height();
        }
        m_rect.MoveToY(content_top + (content_height - m_rect.Height()) / 2 + DPI(theApp.m_taskbar_data.window_offset_top));
        ////确保垂直方向不超出屏幕边界
        //if (m_rect.top < 0)
        //    m_rect.MoveToY(0);
        //if (m_rcTaskbar.Height() > m_rect.Height() && m_rect.bottom > m_rcTaskbar.Height())
        //    m_rect.MoveToY(m_rcTaskbar.Height() - m_rect.Height());

        MoveWindow(m_rect);
    }
}

void CWin11TaskbarDlg::InitTaskbarWnd()
{
    m_hNotify = ::FindWindowEx(m_hTaskbar, 0, L"TrayNotifyWnd", NULL);
    m_hStart = ::FindWindowEx(m_hTaskbar, nullptr, L"Start", NULL);
    ::GetWindowRect(m_hNotify, m_rcNotify);
}

void CWin11TaskbarDlg::ResetTaskbarPos()
{
}

HWND CWin11TaskbarDlg::GetParentHwnd()
{
    return m_hTaskbar;
}

void CWin11TaskbarDlg::CheckTaskbarOnTopOrBottom()
{
    m_taskbar_on_top_or_bottom = m_rcTaskbar.Width() >= m_rcTaskbar.Height();
}
