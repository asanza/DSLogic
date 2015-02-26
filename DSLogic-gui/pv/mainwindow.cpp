/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */


#ifdef ENABLE_DECODE
#include <libsigrokdecode/libsigrokdecode.h>
#include "dock/protocoldock.h"
#endif

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QDockWidget>
#include <QDebug>
#include <QDesktopWidget>
#include <QKeyEvent>
#include <QEvent>
#include <QSettings>

#include "mainwindow.h"

#include "devicemanager.h"
#include "device/device.h"
#include "device/file.h"

#include "dialogs/about.h"
#include "dialogs/storeprogress.h"

#include "toolbars/samplingbar.h"
#include "toolbars/trigbar.h"
#include "toolbars/filebar.h"
#include "toolbars/logobar.h"

#include "dock/triggerdock.h"
#include "dock/dsotriggerdock.h"
#include "dock/measuredock.h"
#include "dock/searchdock.h"

#include "view/view.h"
#include "view/trace.h"
#include "view/signal.h"
#include "view/dsosignal.h"

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <list>
#include <libsigrok4DSLogic/libsigrok.h>

using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using std::list;
using std::vector;

namespace pv {

MainWindow::MainWindow(DeviceManager &device_manager,
	const char *open_file_name,
	QWidget *parent) :
    QMainWindow(parent),
    _device_manager(device_manager),
    _session(device_manager)
{
	setup_ui();
    QRect position = frameGeometry();
    position.moveCenter(QDesktopWidget().availableGeometry().center());
    move(position.topLeft());
	if (open_file_name) {
		const QString s(QString::fromUtf8(open_file_name));
		QMetaObject::invokeMethod(this, "load_file",
			Qt::QueuedConnection,
			Q_ARG(QString, s));
	}
}

void MainWindow::setup_ui()
{
	setObjectName(QString::fromUtf8("MainWindow"));
    setMinimumHeight(500);
    setMinimumWidth(600);
    QRect screenSize = QApplication::desktop()->availableGeometry (-1);
    QSettings settings;
    int height = settings.value("mainwin_height", 0).toInt();
    int width  = settings.value("mainwin_width", 0).toInt();
    // look if values found or the saved size fits to screen.
    if(height == 0 || width == 0
            || screenSize.width() < width || screenSize.height() < height){
        // no values found, show maximized
        showMaximized();
    }else{
        resize(width, height);
    }

	// Set the window icon
	QIcon icon;
    icon.addFile(QString::fromUtf8(":/icons/logo.png"),
		QSize(), QIcon::Normal, QIcon::Off);
	setWindowIcon(icon);

	// Setup the central widget
	_central_widget = new QWidget(this);
	_vertical_layout = new QVBoxLayout(_central_widget);
	_vertical_layout->setSpacing(6);
	_vertical_layout->setContentsMargins(0, 0, 0, 0);
	setCentralWidget(_central_widget);

	// Setup the sampling bar
    _sampling_bar = new toolbars::SamplingBar(_session, this);
    _trig_bar = new toolbars::TrigBar(this);
    _file_bar = new toolbars::FileBar(_session, this);
    _logo_bar = new toolbars::LogoBar(_session, this);

    connect(_trig_bar, SIGNAL(on_protocol(bool)), this,
            SLOT(on_protocol(bool)));
    connect(_trig_bar, SIGNAL(on_trigger(bool)), this,
            SLOT(on_trigger(bool)));
    connect(_trig_bar, SIGNAL(on_measure(bool)), this,
            SLOT(on_measure(bool)));
    connect(_trig_bar, SIGNAL(on_search(bool)), this,
            SLOT(on_search(bool)));
    connect(_file_bar, SIGNAL(load_file(QString)), this,
            SLOT(load_file(QString)));
    connect(_file_bar, SIGNAL(save()), this,
            SLOT(on_save()));
    connect(_file_bar, SIGNAL(on_screenShot()), this,
            SLOT(on_screenShot()));

#ifdef ENABLE_DECODE
    // protocol dock
    _protocol_dock=new QDockWidget(tr("Protocol"),this);
    _protocol_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    _protocol_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _protocol_dock->setVisible(false);
    //dock::ProtocolDock *_protocol_widget = new dock::ProtocolDock(_protocol_dock, _session);
    _protocol_widget = new dock::ProtocolDock(_protocol_dock, _session);
    _protocol_dock->setWidget(_protocol_widget);
    qDebug() << "Protocol decoder enabled!\n";
#endif
    // trigger dock
    _trigger_dock=new QDockWidget(tr("Trigger Setting..."),this);
    _trigger_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    _trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _trigger_dock->setVisible(false);
    _trigger_widget = new dock::TriggerDock(_trigger_dock, _session);
    _trigger_dock->setWidget(_trigger_widget);

    _dso_trigger_dock=new QDockWidget(tr("Trigger Setting..."),this);
    _dso_trigger_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    _dso_trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _dso_trigger_dock->setVisible(false);
    _dso_trigger_widget = new dock::DsoTriggerDock(_dso_trigger_dock, _session);
    _dso_trigger_dock->setWidget(_dso_trigger_widget);


    // Setup _view widget
    _view = new pv::view::View(_session, this);
    _vertical_layout->addWidget(_view);

    connect(_sampling_bar, SIGNAL(device_selected()), this,
            SLOT(update_device_list()));
    connect(_sampling_bar, SIGNAL(device_updated()), &_session,
        SLOT(reload()));
    connect(_sampling_bar, SIGNAL(run_stop()), this,
        SLOT(run_stop()));
    connect(_sampling_bar, SIGNAL(instant_stop()), this,
        SLOT(instant_stop()));
    connect(_sampling_bar, SIGNAL(update_scale()), _view,
        SLOT(update_scale()));
    connect(_dso_trigger_widget, SIGNAL(set_trig_pos(quint64)), _view,
        SLOT(set_trig_pos(quint64)));

    addToolBar(_sampling_bar);
    addToolBar(_trig_bar);
    addToolBar(_file_bar);
    addToolBar(_logo_bar);

    // Setup the dockWidget
    // measure dock
    _measure_dock=new QDockWidget(tr("Measurement"),this);
    _measure_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    _measure_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _measure_dock->setVisible(false);
    dock::MeasureDock *_measure_widget = new dock::MeasureDock(_measure_dock, *_view, _session);
    _measure_dock->setWidget(_measure_widget);
    // search dock
    _search_dock=new QDockWidget(tr("Search..."), this);
    _search_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    _search_dock->setTitleBarWidget(new QWidget(_search_dock));
    _search_dock->setAllowedAreas(Qt::BottomDockWidgetArea);
    _search_dock->setVisible(false);
    //dock::SearchDock *_search_widget = new dock::SearchDock(_search_dock, *_view, _session);
    _search_widget = new dock::SearchDock(_search_dock, *_view, _session);
    _search_dock->setWidget(_search_widget);

#ifdef ENABLE_DECODE
    addDockWidget(Qt::RightDockWidgetArea,_protocol_dock);
#endif
    addDockWidget(Qt::RightDockWidgetArea,_trigger_dock);
    addDockWidget(Qt::RightDockWidgetArea,_dso_trigger_dock);
    addDockWidget(Qt::RightDockWidgetArea, _measure_dock);
    addDockWidget(Qt::BottomDockWidgetArea, _search_dock);

	// Set the title
    setWindowTitle(QApplication::translate("MainWindow", "DSLogic(Beta)", 0));

	// Setup _session events
	connect(&_session, SIGNAL(capture_state_changed(int)), this,
		SLOT(capture_state_changed(int)));
    connect(&_session, SIGNAL(device_attach()), this,
            SLOT(device_attach()));
    connect(&_session, SIGNAL(device_detach()), this,
            SLOT(device_detach()));
    connect(&_session, SIGNAL(test_data_error()), this,
            SLOT(test_data_error()));
    connect(&_session, SIGNAL(sample_rate_changed(uint64_t)), _sampling_bar,
            SLOT(set_sample_rate(uint64_t)));

    connect(_view, SIGNAL(cursor_update()), _measure_widget,
            SLOT(cursor_update()));
    connect(_view, SIGNAL(cursor_moved()), _measure_widget,
            SLOT(cursor_moved()));
    connect(_view, SIGNAL(mouse_moved()), _measure_widget,
            SLOT(mouse_moved()));
    connect(_view, SIGNAL(mode_changed()), this,
            SLOT(update_device_list()));

    // event filter
    _view->installEventFilter(this);
    _sampling_bar->installEventFilter(this);
    _trig_bar->installEventFilter(this);
    _file_bar->installEventFilter(this);
    _logo_bar->installEventFilter(this);
    _dso_trigger_dock->installEventFilter(this);
    _trigger_dock->installEventFilter(this);
#ifdef ENABLE_DECODE
    _protocol_dock->installEventFilter(this);
#endif
    _measure_dock->installEventFilter(this);
    _search_dock->installEventFilter(this);

    // Populate the device list and select the initially selected device
    _session.set_default_device();
    update_device_list();
    _session.start_hotplug_proc(boost::bind(&MainWindow::session_error, this,
                                             QString("Hotplug failed"), _1));
}

void MainWindow::session_error(
	const QString text, const QString info_text)
{
	QMetaObject::invokeMethod(this, "show_session_error",
		Qt::QueuedConnection, Q_ARG(QString, text),
		Q_ARG(QString, info_text));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
   QMainWindow::closeEvent(event);
   // save new size in settings
   QSettings settings;
   settings.setValue("mainwin_height", height());
   settings.setValue("mainwin_width", width());
}

void MainWindow::update_device_list()
{
    assert(_sampling_bar);

    _view->show_trig_cursor(false);
    _trigger_widget->device_change();
#ifdef ENABLE_DECODE
    _protocol_widget->del_all_protocol();
#endif
    _trig_bar->close_all();


    if (_session.get_device()->dev_inst()->mode == DSO) {
        _sampling_bar->enable_toggle(false);
    } else {
        _sampling_bar->enable_toggle(true);
    }

    shared_ptr<pv::device::DevInst> selected_device = _session.get_device();
    _device_manager.add_device(selected_device);
    _sampling_bar->set_device_list(_device_manager.devices(), selected_device);
    _session.init_signals();

    if(dynamic_pointer_cast<pv::device::File>(selected_device)) {
        const QString errorMessage(
            QString("Failed to capture file data!"));
        const QString infoMessage;
        _session.start_capture(true, boost::bind(&MainWindow::session_error, this,
            errorMessage, infoMessage));
    }

    if (strcmp(selected_device->dev_inst()->driver->name, "DSLogic") == 0)
        _logo_bar->dslogic_connected(true);
    else
        _logo_bar->dslogic_connected(false);
}

void MainWindow::load_file(QString file_name)
{
    try {
        _session.set_file(file_name.toStdString());
    } catch(QString e) {
        show_session_error(tr("Failed to load ") + file_name, e);
        _session.set_default_device();
        update_device_list();
        return;
    }

    update_device_list();
}

void MainWindow::show_session_error(
	const QString text, const QString info_text)
{
	QMessageBox msg(this);
	msg.setText(text);
	msg.setInformativeText(info_text);
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setIcon(QMessageBox::Warning);
	msg.exec();
}

void MainWindow::device_attach()
{
    //_session.stop_hot_plug_proc();

    if (_session.get_capture_state() == SigSession::Running)
        _session.stop_capture();

    struct sr_dev_driver **const drivers = sr_driver_list();
    struct sr_dev_driver **driver;
    for (driver = drivers; strcmp(((struct sr_dev_driver *)*driver)->name, "DSLogic") != 0 && *driver; driver++);

    if (*driver)
        _device_manager.driver_scan(*driver);

    _session.set_default_device();
    update_device_list();
}

void MainWindow::device_detach()
{
    //_session.stop_hot_plug_proc();

    if (_session.get_capture_state() == SigSession::Running)
        _session.stop_capture();

    struct sr_dev_driver **const drivers = sr_driver_list();
    struct sr_dev_driver **driver;
    for (driver = drivers; strcmp(((struct sr_dev_driver *)*driver)->name, "DSLogic") != 0 && *driver; driver++);

    if (*driver)
        _device_manager.driver_scan(*driver);

    _session.set_default_device();
    update_device_list();
}

void MainWindow::run_stop()
{
    _sampling_bar->enable_run_stop(false);
    _sampling_bar->enable_instant(false);
    switch(_session.get_capture_state()) {
    case SigSession::Init:
	case SigSession::Stopped:
        _view->show_trig_cursor(false);
        _session.start_capture(false,
			boost::bind(&MainWindow::session_error, this,
				QString("Capture failed"), _1));
		break;

	case SigSession::Running:
		_session.stop_capture();
		break;
	}
    g_usleep(1000);
    _sampling_bar->enable_run_stop(true);
}

void MainWindow::instant_stop()
{
    _sampling_bar->enable_instant(false);
    _sampling_bar->enable_run_stop(false);
    switch(_session.get_capture_state()) {
    case SigSession::Init:
    case SigSession::Stopped:
        _view->show_trig_cursor(false);
        _session.start_capture(true,
            boost::bind(&MainWindow::session_error, this,
                QString("Capture failed"), _1));
        break;

    case SigSession::Running:
        _session.stop_capture();
        break;
    }
    g_usleep(1000);
    _sampling_bar->enable_instant(true);
}

void MainWindow::test_data_error()
{
    _session.stop_capture();
    QMessageBox msg(this);
    msg.setText("Data Error");
    msg.setInformativeText("the receive data are not consist with pre-defined test data");
    msg.setStandardButtons(QMessageBox::Ok);
    msg.setIcon(QMessageBox::Warning);
    msg.exec();
}

void MainWindow::capture_state_changed(int state)
{
    if (_session.get_device()->dev_inst()->mode != DSO) {
        _sampling_bar->enable_toggle(state != SigSession::Running);
        _trig_bar->enable_toggle(state != SigSession::Running);
        _measure_dock->widget()->setEnabled(state != SigSession::Running);
    }
    _file_bar->enable_toggle(state != SigSession::Running);
    _sampling_bar->set_sampling(state == SigSession::Running);
    _view->on_state_changed(state != SigSession::Running);
}

void MainWindow::on_protocol(bool visible)
{
#ifdef ENABLE_DECODE
    _protocol_dock->setVisible(visible);
#endif
}

void MainWindow::on_trigger(bool visible)
{
    if (_session.get_device()->dev_inst()->mode != DSO) {
        _trigger_dock->setVisible(visible);
        _dso_trigger_dock->setVisible(false);
    } else {
        _trigger_dock->setVisible(false);
        _dso_trigger_dock->setVisible(visible);
    }
}

void MainWindow::on_measure(bool visible)
{
    _measure_dock->setVisible(visible);
}

void MainWindow::on_search(bool visible)
{
    _search_dock->setVisible(visible);
    _view->show_search_cursor(visible);
}

void MainWindow::on_screenShot()
{
    QPixmap pixmap;
    QDesktopWidget *desktop = QApplication::desktop();
    pixmap = QPixmap::grabWindow(desktop->winId(), pos().x(), pos().y(), frameGeometry().width(), frameGeometry().height());
    QString format = "png";
    QString initialPath = QDir::currentPath()+
                          tr("/untitled.") + format;

    QString fileName = QFileDialog::getSaveFileName(this,
                       tr("Save As"),initialPath,
                       tr("%1 Files (*.%2);;All Files (*)")
                       .arg(format.toUpper()).arg(format));
    if (!fileName.isEmpty())
        pixmap.save(fileName, format.toLatin1());
}

void MainWindow::on_save()
{
    using pv::dialogs::StoreProgress;

    // Stop any currently running capture session
    _session.stop_capture();

    // Show the dialog
    const QString file_name = QFileDialog::getSaveFileName(
        this, tr("Save File"), "", tr("DSLogic Sessions (*.dsl)"));

    if (file_name.isEmpty())
        return;

    StoreProgress *dlg = new StoreProgress(file_name, _session, this);
    dlg->run();
}


bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    (void) object;

    if( event->type() == QEvent::KeyPress ) {
        const vector< shared_ptr<view::Signal> > sigs(
                    _session.get_signals());

        QKeyEvent *ke = (QKeyEvent *) event;
        switch(ke->key()) {
        case Qt::Key_S:
            run_stop();
            break;
        case Qt::Key_I:
            instant_stop();
            break;
        case Qt::Key_T:
            if (_session.get_device()->dev_inst()->mode == DSO)
                on_trigger(!_dso_trigger_dock->isVisible());
            else
                on_trigger(!_trigger_dock->isVisible());
            break;
#ifdef ENABLE_DECODE
        case Qt::Key_D:
            on_protocol(!_protocol_dock->isVisible());
            break;
#endif
        case Qt::Key_M:
            on_measure(!_measure_dock->isVisible());
            break;
        case Qt::Key_R:
            on_search(!_search_dock->isVisible());
            break;
        case Qt::Key_O:
            _sampling_bar->on_configure();
            break;
        case Qt::Key_PageUp:
            _view->set_scale_offset(_view->scale(),
                                    _view->offset() - _view->scale()*_view->viewport()->width());
            break;
        case Qt::Key_PageDown:
            _view->set_scale_offset(_view->scale(),
                                    _view->offset() + _view->scale()*_view->viewport()->width());

            break;
        case Qt::Key_Left:
            _view->zoom(1);
            break;
        case Qt::Key_Right:
            _view->zoom(-1);
            break;
        case Qt::Key_Up:
            BOOST_FOREACH(const shared_ptr<view::Signal> s, sigs) {
                shared_ptr<view::DsoSignal> dsoSig;
                if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
                    if (dsoSig->get_vDialActive()) {
                        dsoSig->go_vDialNext();
                        update();
                        break;
                    }
                }
            }
            break;
        case Qt::Key_Down:
            BOOST_FOREACH(const shared_ptr<view::Signal> s, sigs) {
                shared_ptr<view::DsoSignal> dsoSig;
                if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
                    if (dsoSig->get_vDialActive()) {
                        dsoSig->go_vDialPre();
                        update();
                        break;
                    }
                }
            }
            break;
        default:
            QWidget::keyPressEvent((QKeyEvent *)event);
        }
        return true;
    }
    return false;
}

} // namespace pv
