#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <gst/gst.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startStreaming();
    void stopStreaming();
    void updateStatus();
    void browseFile();

private:
    Ui::MainWindow *ui;
    GstElement *pipeline;
    GMainLoop *loop;
    GThread *gst_thread;
    QString videoPath;
    QTimer *statusTimer;
    bool isStreaming;

    static gboolean busCallback(GstBus *bus, GstMessage *msg, gpointer data);
    static void *gstThreadFunc(gpointer data);
    void setupGstreamer();
    void cleanupGstreamer();
};
#endif // MAINWINDOW_H
