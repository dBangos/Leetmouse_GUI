#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow){
    ui->setupUi(this);
    setFixedSize(width(), height());
    setWindowTitle("Leetmouse GUI");
    ui->plainTextEdit->setReadOnly(true);

    //Initializing variables
    current_curve_points = new QLineSeries();
    new_curve_points = new QLineSeries();

    //Chart Creation
    QChart *chart = new QChart();
    QChartView *chartView = new QChartView(chart);
    ui->gridLayout->addWidget(chartView);
    chartView->setRenderHint(QPainter::Antialiasing);
    chart->addSeries(current_curve_points);
    chart->addSeries(new_curve_points);
    chart->legend()->hide();

    //Axes Creation
    //Series added after the axes have been created will ignore said axes and just use the whole plot
    axisX = new QValueAxis;
    axisY = new QValueAxis;
    axisX->setTitleText("Input Speed(count/ms)");
    axisY->setTitleText("Ratio of Input to Output");
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    current_curve_points->attachAxis(axisX);
    current_curve_points->attachAxis(axisY);
    new_curve_points->attachAxis(axisX);
    new_curve_points->attachAxis(axisY);

    //Adding tooltips when hovering over labels
    ui->label_sens->setToolTip("Multiplies the base sensitivity by this value");
    ui->label_accel->setToolTip("Sets the rate of the sensitivity increase");
    ui->label_senscap->setToolTip("Limits the maximum sensitivity that can be reached");
    ui->label_offset->setToolTip("Input speed below this value do not cause acceleration");
    ui->label_speedcap->setToolTip("Limits the maximum speed taken into account for acceleration ");

    //Reading the config file for the previous values
    QFile config_file("config.h");

    if (!config_file.open(QIODevice::ReadOnly | QIODevice::Text)){
        ui->plainTextEdit->appendPlainText("Failed to open config.h file.");
    }
    else{
        QTextStream in(&config_file);
        QStringList line_split;
        QString number_string;
        while (!in.atEnd()) {
            QString line = in.readLine();
            // If the line is an empty qstring then .at() crashes the app, thus they have to be executed in sequence
            if(!line.isEmpty()){
                if(line.at(0) == '#'){
                    line_split = line.split(' ');
                    number_string = line_split.at(2);
                    number_string.chop(1); //Removes the f after each number
                    //Looks bad but is probably better than enum and switch
                    if(line_split.at(1) == "SENSITIVITY") config_variables.sensitivity = number_string.toDouble();
                    else if(line_split.at(1) == "ACCELERATION") config_variables.acceleration = number_string.toDouble();
                    else if(line_split.at(1) == "SENS_CAP") config_variables.sens_cap = number_string.toDouble();
                    else if(line_split.at(1) == "OFFSET") config_variables.offset = number_string.toDouble();
                    else if(line_split.at(1) == "POST_SCALE_X") config_variables.post_scale_x = number_string.toDouble();
                    else if(line_split.at(1) == "POST_SCALE_Y") config_variables.post_scale_y = number_string.toDouble();
                    else if(line_split.at(1) == "SPEED_CAP") config_variables.speed_cap = number_string.toDouble();
                    else if(line_split.at(1) == "PRE_SCALE_X") config_variables.pre_scale_x = number_string.toDouble();
                    else if(line_split.at(1) == "PRE_SCALE_Y") config_variables.pre_scale_y = number_string.toDouble();
                }
            }
        }
        config_file.close();
        ui->plainTextEdit->appendPlainText("Successfully opened the config.h file.");
    }

    //Calculate the points from the accel function
    input_variables = config_variables;
    calculate_points();

    //Setting up the spinboxes
    setup_doubleSpinBox(ui->doubleSpinBox_sens,&input_variables.sensitivity);
    setup_doubleSpinBox(ui->doubleSpinBox_accel,&input_variables.acceleration);
    setup_doubleSpinBox(ui->doubleSpinBox_senscap,&input_variables.sens_cap);
    setup_doubleSpinBox(ui->doubleSpinBox_offset,&input_variables.offset);
    setup_doubleSpinBox(ui->doubleSpinBox_speedcap,&input_variables.speed_cap);
    setup_doubleSpinBox(ui->doubleSpinBox_postx,&input_variables.post_scale_x);
    setup_doubleSpinBox(ui->doubleSpinBox_posty,&input_variables.post_scale_y);
    setup_doubleSpinBox(ui->doubleSpinBox_prex,&input_variables.pre_scale_x);
    setup_doubleSpinBox(ui->doubleSpinBox_prey,&input_variables.pre_scale_y);


    //With every change of the values calculate new points and update chart
    //TODO: This uses pixels, intstead it should use the count from the mouse
    /*
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this,  [=](){
        float prev_x, prev_y;
        float current_speed = qSqrt(((prev_x - QCursor::pos().x())*(prev_x - QCursor::pos().x()) + (prev_y - QCursor::pos().y())*(prev_y - QCursor::pos().y())));
        prev_x = QCursor::pos().x();
        prev_y = QCursor::pos().y();
        std::cout<<acceleration<<" "<<QCursor::pos().x()<<" "<<current_speed<<std::endl;
    });
    timer->start(1);
    */
}

double MainWindow::scale_axes(){
    //Returns the lenght of the x_axis to be passed to the point calculating loop
    //Calculates the axes ranges so they show all the interesting stuff happening
    //  On the Y axis figure out the cap of each line(the minimum of the speed and sens caps)
    //      then find the which line has the largest limit and use that to scale the axes.
    //  On the X axis if there is an offset or there is a cap, show a just bit after the max one.

    double x_range_factor = 1.4;
    double y_range_factor = 1.2;
    double x_offset_factor = 2;
    QVector<double> x_interesting_points {0,0,0,0, config_variables.offset * x_offset_factor, input_variables.offset * x_offset_factor};
    QVector<double> y_config_possible_limit {0,0}; //Could possibly just use variables here, but I think this looks cleaner
    QVector<double> y_input_possible_limit {0,0};

    if(config_variables.sens_cap != 0 && config_variables.acceleration != 0 && config_variables.sens_cap >= config_variables.sensitivity){
        x_interesting_points[0] = config_variables.offset + config_variables.sens_cap/config_variables.acceleration;
        y_config_possible_limit[0] = config_variables.sens_cap;
    }
    if(config_variables.speed_cap != 0){
        x_interesting_points[1] = config_variables.speed_cap;
        y_config_possible_limit[1] =config_variables.sensitivity + config_variables.speed_cap * config_variables.acceleration;
    }
    if(input_variables.sens_cap != 0 && input_variables.acceleration != 0 && input_variables.sens_cap >= input_variables.sensitivity){
        x_interesting_points[2] = input_variables.offset + input_variables.sens_cap/input_variables.acceleration;
        y_input_possible_limit[0] = input_variables.sens_cap;
    }
    if(input_variables.speed_cap != 0){
        x_interesting_points[3] = input_variables.speed_cap;
        y_input_possible_limit[1] = input_variables.sensitivity + input_variables.speed_cap * input_variables.acceleration;
    }
    QVector<double> y_interesting_points {*std::min_element(y_config_possible_limit.begin(), y_config_possible_limit.end()),*std::min_element(y_input_possible_limit.begin(), y_input_possible_limit.end())};
    double x_range = *std::max_element(x_interesting_points.begin(), x_interesting_points.end());
    double y_range = *std::max_element(y_interesting_points.begin(), y_interesting_points.end());

    if (x_range == 0) x_range = 10; //If there are no limits set, just use a value to draw the lines
    if (y_range == 0) { //If there are no limits set, use the accel value to scale, or if even that is not set, fall back to sensitivity
        if(config_variables.acceleration != 0 || input_variables.acceleration != 0){
            y_interesting_points[0] = config_variables.sensitivity + (x_range - config_variables.offset) * config_variables.acceleration;
            y_interesting_points[1] = input_variables.sensitivity + (x_range - input_variables.offset) * input_variables.acceleration;
            y_range = *std::max_element(y_interesting_points.begin(), y_interesting_points.end());
        }
        else{
            y_interesting_points[0] = config_variables.sensitivity;
            y_interesting_points[1] = input_variables.sensitivity;
            y_range = *std::max_element(y_interesting_points.begin(), y_interesting_points.end());
        }
    }

    double y_start_point; //Setting the y initial point to be the lowest sensitivity
    config_variables.sensitivity > input_variables.sensitivity ? y_start_point = input_variables.sensitivity : y_start_point = config_variables.sensitivity;

    axisX->setRange(0, x_range * x_range_factor);
    axisY->setRange(y_start_point, y_range * y_range_factor);
    return x_range*x_range_factor;
}

void MainWindow::calculate_points(){
    //Calculates the new points and draws them on the chart
    //This is preferable to drawing lines with a start and end point as it can support curves
    double x_size = scale_axes();
    double ratio_out_in;
    double input_speed;
    double i = 0;
    while(i < x_size){
        //Line based on config
        input_speed = i;
        if(input_speed >= config_variables.speed_cap && config_variables.speed_cap != 0) input_speed = config_variables.speed_cap;
        ratio_out_in = config_variables.sensitivity + (input_speed - config_variables.offset) * config_variables.acceleration;
        if(input_speed < config_variables.offset && config_variables.offset != 0) ratio_out_in = config_variables.sensitivity;
        else if(ratio_out_in >= config_variables.sens_cap && config_variables.sens_cap != 0 && config_variables.sens_cap >= config_variables.sensitivity) ratio_out_in = config_variables.sens_cap;
        current_curve_points->append(i,ratio_out_in);

        //Line based on input
        input_speed = i;
        if(input_speed >= input_variables.speed_cap && input_variables.speed_cap != 0) input_speed = input_variables.speed_cap;
        ratio_out_in = input_variables.sensitivity + (input_speed - input_variables.offset) * input_variables.acceleration;
        if(input_speed < input_variables.offset && input_variables.offset != 0) ratio_out_in = input_variables.sensitivity;
        else if(ratio_out_in >= input_variables.sens_cap && input_variables.sens_cap != 0&& input_variables.sens_cap >= input_variables.sensitivity) ratio_out_in = input_variables.sens_cap;
        new_curve_points->append(i,ratio_out_in);

        i += x_size/100;
    }
}

void MainWindow::setup_doubleSpinBox(QDoubleSpinBox *spinbox, double *val){
    //Displays the config values
    //Sets spinbox step settings
    //Connects the spinboxes to the variables and calls for the calculation of the new points
    spinbox->setDecimals(5);
    spinbox->setSingleStep(0.025);
    spinbox->setValue(*val);
    //spinbox->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    connect(spinbox, &QDoubleSpinBox::valueChanged, this, [=](){
        *val = spinbox->value();
        reset_chart();
    });
}

void MainWindow::reset_chart(){
    current_curve_points->clear();
    new_curve_points->clear();
    calculate_points();
}

void MainWindow::on_pushButton_reset_clicked(){
    input_variables = config_variables;
    ui->doubleSpinBox_sens->setValue(config_variables.sensitivity);
    ui->doubleSpinBox_accel->setValue(config_variables.acceleration);
    ui->doubleSpinBox_senscap->setValue(config_variables.sens_cap);
    ui->doubleSpinBox_offset->setValue(config_variables.offset);
    ui->doubleSpinBox_speedcap->setValue(config_variables.post_scale_x);
    ui->doubleSpinBox_postx->setValue(config_variables.post_scale_y);
    ui->doubleSpinBox_posty->setValue(config_variables.speed_cap);
    ui->doubleSpinBox_prex->setValue(config_variables.pre_scale_x);
    ui->doubleSpinBox_prey->setValue(config_variables.pre_scale_y);
    reset_chart();
}

void MainWindow::on_pushButton_save_clicked(){
    //Creating the popup dialog asking the user for confirmation.
    QMessageBox msgBox;
    msgBox.setText("Saving will overwrite the previous values.");
    msgBox.setInformativeText("Do you want to save your changes?");
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    int ret = msgBox.exec();
    switch (ret) {
    case QMessageBox::Save:
        // Save was clicked
        save_to_file();
        //Update the chart
        config_variables = input_variables;
        reset_chart();
        break;
    case QMessageBox::Cancel:
        // Cancel was clicked
        break;
    }
}
void MainWindow::save_to_file(){
    //Reading the config file for the previous values
    QFile config_file("config.h");
    if (!config_file.open(QIODevice::ReadWrite | QIODevice::Text)){
        ui->plainTextEdit->appendPlainText("Failed to open config.h file.");
        return;
    }
    QTextStream in_stream(&config_file);
    QStringList new_lines;
    QString line;
    if(!config_file.size()){ //If there is no previous file write the values in config.h
        line = QString("#define SENSITIVITY %1f\n").arg(input_variables.sensitivity, 0, 'f', 7);
        line.append(QString("#define ACCELERATION %1f\n").arg(input_variables.acceleration, 0, 'f', 7));
        line.append(QString("#define SENS_CAP %1f\n").arg(input_variables.sens_cap, 0, 'f', 7));
        line.append(QString("#define OFFSET %1f\n").arg(input_variables.offset, 0, 'f', 7));
        line.append(QString("#define POST_SCALE_X %1f\n").arg(input_variables.post_scale_x, 0, 'f', 7));
        line.append(QString("#define POST_SCALE_Y %1f\n").arg(input_variables.post_scale_y, 0, 'f', 7));
        line.append(QString("#define SPEED_CAP %1f\n").arg(input_variables.speed_cap, 0, 'f', 7));
        line.append(QString("#define PRE_SCALE_X %1f\n").arg(input_variables.pre_scale_x, 0, 'f', 7));
        line.append(QString("#define PRE_SCALE_Y %1f\n").arg(input_variables.pre_scale_y, 0, 'f', 7));
        new_lines<<line;
    }
    else{ //If there is a previous file just modify the lines with the values
        while (!in_stream.atEnd()) {
            line = in_stream.readLine();
            // If the line is an empty qstring then .at() crashes the app, thus they have to be executed in sequence
            if(!line.isEmpty()){
                if(line.at(0) == '#'){//Checking if the line is active, don't want to change commented out lines
                    //Since the whole line gets replaced(no need to split), contains was used instead of a "==" comparison
                    if(line.contains("SENSITIVITY")) line = QString("#define SENSITIVITY %1f").arg(input_variables.sensitivity, 0, 'f', 7);
                    else if(line.contains("ACCELERATION")) line = QString("#define ACCELERATION %1f").arg(input_variables.acceleration, 0, 'f', 7);
                    else if(line.contains("SENS_CAP")) line = QString("#define SENS_CAP %1f").arg(input_variables.sens_cap, 0, 'f', 7);
                    else if(line.contains("OFFSET")) line = QString("#define OFFSET %1f").arg(input_variables.offset, 0, 'f', 7);
                    else if(line.contains("POST_SCALE_X")) line = QString("#define POST_SCALE_X %1f").arg(input_variables.post_scale_x, 0, 'f', 7);
                    else if(line.contains("POST_SCALE_Y")) line = QString("#define POST_SCALE_Y %1f").arg(input_variables.post_scale_y, 0, 'f', 7);
                    else if(line.contains("SPEED_CAP")) line = QString("#define SPEED_CAP %1f").arg(input_variables.speed_cap, 0, 'f', 7);
                    else if(line.contains("PRE_SCALE_X")) line = QString("#define PRE_SCALE_X %1f").arg(input_variables.pre_scale_x, 0, 'f', 7);
                    else if(line.contains("PRE_SCALE_Y")) line = QString("#define PRE_SCALE_Y %1f").arg(input_variables.pre_scale_y, 0, 'f', 7);
                }
            }
            new_lines<<line.append("\n");
        }
    }
    config_file.resize(0); //Deleting the contents of the config file
    for (auto&& i:new_lines){ //Writing the new content on the file
        in_stream<<i;
    }
    config_file.close();
    ui->plainTextEdit->appendPlainText("Saved the changes to the config.h file.");
}

MainWindow::~MainWindow(){
    delete ui;
}


