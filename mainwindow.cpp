#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QMetaObject>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , pipeline(nullptr)
    , loop(nullptr)
    , gst_thread(nullptr)
    , isStreaming(false)
{
    ui->setupUi(this);

    // Initialize GStreamer
    gst_init(nullptr, nullptr);

    // Connect UI elements
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::browseFile);
    connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::startStreaming);
    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopStreaming);

    ui->stopButton->setEnabled(false);

    // Set up status timer
    statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::updateStatus);

    // Get local IP addresses
    QString ipAddresses;
    const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHost);
    for (const QHostAddress &address: QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost) {
            ipAddresses += address.toString() + "\n";
        }
    }

    if (ipAddresses.isEmpty()) {
        ipAddresses = "127.0.0.1";
    }

    ui->ipAddressLabel->setText(ipAddresses);

    // Log that application started
    ui->logTextEdit->append("Application started. Select a video file to stream.");
}

MainWindow::~MainWindow()
{
    stopStreaming();
    delete ui;
}

void MainWindow::browseFile()
{
    videoPath = QFileDialog::getOpenFileName(this,
                                             tr("Open Video File"), "", tr("Video Files (*.mp4 *.avi *.mkv *.mov)"));

    if (!videoPath.isEmpty()) {
        ui->filePathEdit->setText(videoPath);
        ui->startButton->setEnabled(true);
        ui->logTextEdit->append("Video file selected: " + videoPath);
    }
}

void MainWindow::startStreaming()
{
    if (videoPath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select a video file first");
        return;
    }

    int port = ui->portSpinBox->value();
    QString ip = ui->ipAddressLabel->text().split("\n")[0];
    QString rtspUrl = QString("rtsp://%1:%2/video").arg(ip).arg(port);
    ui->rtspUrlEdit->setText(rtspUrl);

    // Log streaming attempt
    ui->logTextEdit->append("Attempting to start streaming on port " + QString::number(port));
    ui->logTextEdit->append("RTSP URL: " + rtspUrl);

    // Create a more robust pipeline
    QString pipelineStr;

    // For Windows, we'll use a simpler, more compatible pipeline
    pipelineStr = QString(
                      "filesrc location=\"%1\" ! "
                      "decodebin name=decoder ! "
                      "videoconvert ! video/x-raw,format=I420 ! "
                      "x264enc tune=zerolatency bitrate=500 ! "
                      "rtph264pay config-interval=1 name=pay0 pt=96 ! "
                      "rtspsink service=%2 address=0.0.0.0 protocols=tcp+udp"
                      ).arg(videoPath).arg(port);

    ui->logTextEdit->append("Creating pipeline...");
    ui->logTextEdit->append(pipelineStr);

    GError *error = nullptr;
    pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);

    if (error) {
        ui->logTextEdit->append("Pipeline creation error: " + QString(error->message));
        QMessageBox::critical(this, "Error", "Failed to create GStreamer pipeline: " + QString(error->message));
        g_error_free(error);
        return;
    }

    if (!pipeline) {
        ui->logTextEdit->append("Failed to create pipeline (unknown error)");
        QMessageBox::critical(this, "Error", "Failed to create GStreamer pipeline");
        return;
    }

    // Set up bus watcher
    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, busCallback, this);
    gst_object_unref(bus);

    // Create main loop and run it in a separate thread
    loop = g_main_loop_new(NULL, FALSE);
    gst_thread = g_thread_new("GstThread", gstThreadFunc, this);

    // Start the pipeline
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        ui->logTextEdit->append("Failed to start pipeline");
        QMessageBox::critical(this, "Error", "Failed to start GStreamer pipeline");
        g_main_loop_quit(loop);
        return;
    }

    // Update UI
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    ui->browseButton->setEnabled(false);
    ui->statusLabel->setText("Streaming...");
    isStreaming = true;

    ui->logTextEdit->append("Pipeline started successfully");

    // Start status timer
    statusTimer->start(1000);
}

void MainWindow::stopStreaming()
{
    if (!isStreaming) {
        return;
    }

    ui->logTextEdit->append("Stopping stream...");

    // Stop status timer - Ensure this happens in the main thread
    if (statusTimer->isActive()) {
        statusTimer->stop();
    }

    // Stop GStreamer
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }

    // Quit main loop
    if (loop) {
        g_main_loop_quit(loop);
        if (gst_thread) {
            g_thread_join(gst_thread);
            gst_thread = nullptr;
        }
        g_main_loop_unref(loop);
        loop = nullptr;
    }

    // Update UI
    ui->startButton->setEnabled(true);
    ui->stopButton->setEnabled(false);
    ui->browseButton->setEnabled(true);
    ui->statusLabel->setText("Stopped");
    isStreaming = false;

    ui->logTextEdit->append("Stream stopped");
}

void MainWindow::updateStatus()
{
    if (pipeline) {
        GstState state;
        gst_element_get_state(pipeline, &state, NULL, GST_CLOCK_TIME_NONE);

        switch (state) {
        case GST_STATE_PLAYING:
            ui->statusLabel->setText("Streaming");
            break;
        case GST_STATE_PAUSED:
            ui->statusLabel->setText("Paused");
            break;
        case GST_STATE_READY:
            ui->statusLabel->setText("Ready");
            break;
        case GST_STATE_NULL:
            ui->statusLabel->setText("Stopped");
            break;
        default:
            ui->statusLabel->setText("Unknown state");
            break;
        }
    }
}

gboolean MainWindow::busCallback(GstBus *bus, GstMessage *msg, gpointer data)
{
    MainWindow *window = static_cast<MainWindow*>(data);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError *err;
        gchar *debug;
        gst_message_parse_error(msg, &err, &debug);

        // Log the error
        QString errorMsg = QString("GStreamer error: %1").arg(err->message);
        QString debugInfo = QString("Debug info: %1").arg(debug);

        qDebug("%s", qPrintable(errorMsg));
        qDebug("%s", qPrintable(debugInfo));

        // Update UI with error info
        QMetaObject::invokeMethod(window, "logMessage",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, errorMsg));
        QMetaObject::invokeMethod(window, "logMessage",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, debugInfo));

        g_error_free(err);
        g_free(debug);

        // Use Qt's signal-slot mechanism to safely stop streaming from the main thread
        QMetaObject::invokeMethod(window, "stopStreaming", Qt::QueuedConnection);
        break;
    }
    case GST_MESSAGE_EOS:
        // End of stream - safely stop streaming from the main thread
        QMetaObject::invokeMethod(window, "logMessage",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, "End of stream reached"));
        QMetaObject::invokeMethod(window, "stopStreaming", Qt::QueuedConnection);
        break;
    case GST_MESSAGE_STATE_CHANGED: {
        GstState old_state, new_state, pending_state;
        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

        // Only print state changes for the pipeline
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(window->pipeline)) {
            QString stateChange = QString("Pipeline state changed from %1 to %2")
                                      .arg(gst_element_state_get_name(old_state))
                                      .arg(gst_element_state_get_name(new_state));

            QMetaObject::invokeMethod(window, "logMessage",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, stateChange));
        }
        break;
    }
    default:
        break;
    }

    return TRUE;
}

void MainWindow::logMessage(const QString &message)
{
    ui->logTextEdit->append(message);
}

void* MainWindow::gstThreadFunc(gpointer data)
{
    MainWindow *window = static_cast<MainWindow*>(data);
    g_main_loop_run(window->loop);
    return NULL;
}
