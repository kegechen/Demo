// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDebug>
#include <QX11Info>
#include <QWindow>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_icccm.h>

static inline xcb_window_t xcbWindow(QWindow *window)
{
    return static_cast<xcb_window_t>(window->winId());
}

static xcb_screen_t *xcbScreen()
{
    // 获取屏幕信息
    static xcb_screen_t *screen = nullptr;
    if (screen)
        return screen;
    xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator(xcb_get_setup(QX11Info::connection()));
    for (; screen_iterator.rem; xcb_screen_next(&screen_iterator)) {
        if (screen_iterator.data->root == QX11Info::appRootWindow()) {
            screen = screen_iterator.data;
            break;
        }
    }

    if (!screen) {
        qWarning("Failed to get screen information");
    }

    return screen;
}

static xcb_atom_t atom(const char *name)
{
    xcb_connection_t *connection = QX11Info::connection();
    auto cookie = xcb_intern_atom(connection, 0, strlen(name), name);
    return xcb_intern_atom_reply(connection, cookie, nullptr)->atom;
}

static void sendXcbClientMsg(xcb_window_t window, xcb_atom_t type, uint32_t data32[5])
{
    xcb_client_message_event_t event;

    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.sequence = 0;
    event.window = window;
    event.type = type;

    for (int i = 0; i < 5; ++i)
        event.data.data32[i] = data32[i];

    xcb_send_event(QX11Info::connection(), 0, xcbScreen()->root,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   (const char *)&event);
}

static void setMaxNetWmState(bool set, xcb_window_t window)
{
    uint32_t data32[5] = {0};
    data32[0] = set ? 1 : 0;
    data32[1] = atom("_NET_WM_STATE_MAXIMIZED_HORZ");
    data32[2] = atom("_NET_WM_STATE_MAXIMIZED_VERT");
    sendXcbClientMsg(window, atom("_NET_WM_STATE"), data32);
}

static void setMinNetWmState(xcb_window_t window)
{
    uint32_t data32[5] = {0};
    data32[0] = XCB_ICCCM_WM_STATE_ICONIC;
    sendXcbClientMsg(window, atom("WM_CHANGE_STATE"), data32);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QWidget w;

    QHBoxLayout *hlay = new QHBoxLayout();
    QPushButton *minBtn = new QPushButton("Min");
    QPushButton *maxBtn = new QPushButton("Max");
    QObject::connect(minBtn, &QPushButton::clicked, &w, [&w](){
        w.showMinimized();
        qInfo() << "showMinimized, windowState:" << w.windowState();
    });
    QObject::connect(maxBtn, &QPushButton::clicked, &w, [&w](){
        w.showMaximized();
        qInfo() << "showMaximized, windowState:" << w.windowState();
    });

    hlay->addWidget(minBtn);
    hlay->addWidget(maxBtn);

    QHBoxLayout *hlay1 = new QHBoxLayout();
    QPushButton *min1Btn = new QPushButton("Min-step1");
    QPushButton *min2Btn = new QPushButton("Min-step2");
    hlay1->addWidget(min1Btn);
    hlay1->addWidget(min2Btn);

    QObject::connect(min1Btn, &QPushButton::clicked, &w, [&w](){
        // unset old state
        setMaxNetWmState(false, xcbWindow(w.windowHandle()));
        qInfo() << "unset old state, windowState:" << w.windowState();
    });
    QObject::connect(min2Btn, &QPushButton::clicked, &w, [&w](){
        // set new state
        setMinNetWmState(xcbWindow(w.windowHandle()));

        setMaxNetWmState(true, xcbWindow(w.windowHandle()));

        qInfo() << "set new state, windowState:" << w.windowState();
    });

    QVBoxLayout *vlay = new QVBoxLayout(&w);
    vlay->addLayout(hlay);
    vlay->addLayout(hlay1);
    w.setLayout(vlay);

    w.resize(400, 300);
    w.show();
    return a.exec();
}

/*
 * void QXcbWindow::setNetWmState(bool set, xcb_atom_t one, xcb_atom_t two)
{
    xcb_client_message_event_t event;

event.response_type = XCB_CLIENT_MESSAGE;
event.format = 32;
event.sequence = 0;
event.window = m_window;
event.type = atom(QXcbAtom::_NET_WM_STATE);
event.data.data32[0] = set ? 1 : 0;
event.data.data32[1] = one;
event.data.data32[2] = two;
event.data.data32[3] = 0;
event.data.data32[4] = 0;

xcb_send_event(xcb_connection(), 0, xcbScreen()->root(),
               XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
               (const char *)&event);
}


void QXcbWindow::setWindowState(Qt::WindowStates state)
{
    if (state == m_windowState)
        return;

// unset old state
if (m_windowState & Qt::WindowMinimized)
    xcb_map_window(xcb_connection(), m_window);
if (m_windowState & Qt::WindowMaximized)
    setNetWmState(false,
                  atom(QXcbAtom::_NET_WM_STATE_MAXIMIZED_HORZ),
                  atom(QXcbAtom::_NET_WM_STATE_MAXIMIZED_VERT));
if (m_windowState & Qt::WindowFullScreen)
    setNetWmState(false, atom(QXcbAtom::_NET_WM_STATE_FULLSCREEN));

// set new state
if (state & Qt::WindowMinimized) {
    {
        xcb_client_message_event_t event;

        event.response_type = XCB_CLIENT_MESSAGE;
        event.format = 32;
        event.sequence = 0;
        event.window = m_window;
        event.type = atom(QXcbAtom::WM_CHANGE_STATE);
        event.data.data32[0] = XCB_ICCCM_WM_STATE_ICONIC;
        event.data.data32[1] = 0;
        event.data.data32[2] = 0;
        event.data.data32[3] = 0;
        event.data.data32[4] = 0;

        xcb_send_event(xcb_connection(), 0, xcbScreen()->root(),
                       XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                       (const char *)&event);
    }
    m_minimized = true;
}
if (state & Qt::WindowMaximized)
    setNetWmState(true,
                  atom(QXcbAtom::_NET_WM_STATE_MAXIMIZED_HORZ),
                  atom(QXcbAtom::_NET_WM_STATE_MAXIMIZED_VERT));
if (state & Qt::WindowFullScreen)
    setNetWmState(true, atom(QXcbAtom::_NET_WM_STATE_FULLSCREEN));

setNetWmState(state);

xcb_get_property_cookie_t cookie = xcb_icccm_get_wm_hints_unchecked(xcb_connection(), m_window);
xcb_icccm_wm_hints_t hints;
if (xcb_icccm_get_wm_hints_reply(xcb_connection(), cookie, &hints, nullptr)) {
    if (state & Qt::WindowMinimized)
        xcb_icccm_wm_hints_set_iconic(&hints);
    else
        xcb_icccm_wm_hints_set_normal(&hints);
    xcb_icccm_set_wm_hints(xcb_connection(), m_window, &hints);
}

connection()->sync();
m_windowState = state;
}
*/
