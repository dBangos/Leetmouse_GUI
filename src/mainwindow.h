#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QtCharts>
#include <QChartView>
#include <QLineSeries>

#include <QString>
#include <QFile>
#include <QtMath>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_save_clicked();

    void on_pushButton_reset_clicked();

private:
    Ui::MainWindow *ui;

    struct accel_variables{
        double sensitivity = 0;
        double acceleration = 0;
        double sens_cap = 0;
        double offset = 0;
        double post_scale_x = 0;
        double post_scale_y = 0;
        double speed_cap = 0;
        double pre_scale_x = 0;
        double pre_scale_y = 0;
    } config_variables, input_variables;
    double scale_axes();
    void calculate_points();
    void save_to_file();
    void reset_chart();
    void setup_doubleSpinBox(QDoubleSpinBox *spinbox, double *val);
    QLineSeries *current_curve_points, *new_curve_points;
    QValueAxis *axisX, *axisY;
};
#endif // MAINWINDOW_H
