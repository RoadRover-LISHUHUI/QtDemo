#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    chkParamCount=0;

    initMainWindow();

    initUartParam();

    initProcessDeal();

    initLogcatThreadDeal();

    timer1SID=startTimer(1000);
    initkeyList();

    isRunning=false;
    testState=overtest;
}

MainWindow::~MainWindow()
{
    killTimer(timer1SID);
    deleteLogcatThreadDeal();
    deleteProcessDeal();
    deleteUartParam();
    delete chartDeal;
    delete ui;
}

/*---------------------------------------this is windowDeal option-----------------------------------------*/
/*************************************************************
/函数功能：初始化主界面窗口
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::initMainWindow()
{
    QWidget* p = takeCentralWidget();
    if(p)
        delete p;

    setDockNestingEnabled(true);

    //记录所有的dock指针
    m_docks.append(ui->dockWidgetLeft);
    m_docks.append(ui->dockWidgetBottom);
    m_docks.append(ui->dockWidgetSequence);
    m_docks.append(ui->dockWidgetRuning);
    m_docks.append(ui->dockWidgetShow);

    reRankDockWidget();

    //初始化显示界面
    setIsRunInterface(false);

    //初始化接口树参数
    ui->treeWidget->interfaceTreeWidgetInit();
    ui->treeWidget->refreshUartCOM(UARTDeal->PortList());
    ui->treeWidget->expandAll();

    //ui->textBrowser_EXEShow->setOpenExternalLinks(true);//设置添加超链接

    chartDeal =new ChartWidget;
    //ui->dockWidgetShow->setWidget(chartDeal);
    ui->gridChartwidget->addWidget(chartDeal);

    ui->toolBar_Fun->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    connect(ui->actionrank,SIGNAL(triggered(bool)),this,SLOT(reRankDockWidget()));

    //取配置信息
    Model_iniSetting InfoINI;
    WorkItem        = InfoINI.ReadIni_item("item_Name").toString();
    WorkCurrent     = InfoINI.ReadIni_item("WorkCurrent").toInt();
    WorkFrequency   = InfoINI.ReadIni_item("WorkFrequency").toInt();
    ReportCreat     = InfoINI.ReadIni_item("ReportCreat").toBool();
    IsLogcatEnable  = InfoINI.ReadIni_item("LogcatEnable").toBool();
    logcatPath      = InfoINI.ReadIni_item("LogcatPath").toString();

    //处理错误参数
    if(!WorkCurrent)
        WorkCurrent=700;
    if(!WorkFrequency)
        WorkFrequency=1;

    devNumber.clear();

}

/*************************************************************
/函数功能：重新排序窗口
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::reRankDockWidget()
{
    //移除窗口
    for(int i=0;i<dockMax;++i)
    {
        removeDockWidget(m_docks[i]);
    }

    //排序
    addDockWidget(Qt::LeftDockWidgetArea,ui->dockWidgetLeft);
    splitDockWidget(ui->dockWidgetLeft,ui->dockWidgetBottom,Qt::Vertical);
    splitDockWidget(ui->dockWidgetLeft,ui->dockWidgetSequence,Qt::Horizontal);
    tabifyDockWidget(ui->dockWidgetSequence,ui->dockWidgetRuning);
    tabifyDockWidget(ui->dockWidgetRuning,ui->dockWidgetShow);

    //显示
    for(int i=0;i<dockMax;++i)
    {
        m_docks[i]->show();
    }
}

/*************************************************************
/函数功能：是否切换到运行界面
/函数参数：true：是  false：否
/函数返回：无
*************************************************************/
void MainWindow::setIsRunInterface(bool IsRun)
{
    if(IsRun)
    {
        ui->dockWidgetBottom->setVisible(true);
        ui->dockWidgetRuning->raise();   //显示运行窗口
        ui->acttest->setText(tr("结束"));
        ui->acttest->setIcon(QIcon(":/Title/actEnding.png"));
        ui->acttest->setChecked(IsRun);
        ui->treeWidget->setEnabled(false);


        //主窗口涉及清楚任务，因此放置上层处理
        int row=ui->tableWidget->rowCount();
        for(uint16_t i=row;i>0;i--)
        {
            ui->tableWidget->removeRow(i-1);
        }
    }
    else
    {
        //初始化界面：
        ui->acttest->setText(tr("运行"));
        ui->acttest->setIcon(QIcon(":/Title/actRunning.png"));
        ui->acttest->setChecked(IsRun);
        ui->treeWidget->setEnabled(true);
    }
}

/*************************************************************
/函数功能：主窗口关闭事件
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::closeEvent(QCloseEvent *event)
{
    if(getTestRunState())
    {
        if(QMessageBox::information(NULL, tr("退出"), tr("正在测试，是否退出软件?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No)==QMessageBox::Yes)//Upgrading
        {
            endTheFlow();
            event->accept();
        }
        else
            event->ignore();
    }
    else
        event->accept();
}

/*************************************************************
/函数功能：定时器事件，处理测试
/函数参数：事件
/函数返回：无
*************************************************************/
void MainWindow::timerEvent(QTimerEvent *event)
{
    //串口定时器处理：
    if(event->timerId()==timerUartID)
    {
        timerUartIDDeal();
    }
    //进程定时器处理+显示处理：
    else if(event->timerId() == timerProID)
    {
        timerProIDDeal();

        //显示执行
        if(ShowList.isEmpty() == false)
        {
            ui->textBrowser_EXEShow->append(ShowList.first());
            ShowList.removeFirst();
        }
    }
    //测试定时器处理
    else if(event->timerId()==timerTestID)
    {
        timerTestIDDeal();
    }
    //1S定时
    else if(event->timerId()==timer1SID)
    {
        //显示当前时间
        QDateTime current_date_time =QDateTime::currentDateTime();
        QString current_date =current_date_time.toString("yyyy.MM.dd hh:mm:ss ddd");//.zzz
        ui->statusBar->showMessage(current_date);

        //刷新uart COM
        ui->treeWidget->refreshUartCOM(UARTDeal->PortList());

        //实时扫描设备
        if((!getTestRunState())&&(testState==overtest))
        {
            if(proList.isEmpty())
                proList.append(ADBDevs);
            //cout;
        }

        //显示字符超过1W时清显示数据
        if(ui->textBrowser_EXEShow->toPlainText().count()>=10000)
        {
            appendTheExecLogInfo(ui->textBrowser_EXEShow->toPlainText());
            ui->textBrowser_EXEShow->clear();
            ui->textBrowser_mShow->clear();
        }

        //设置当前序列号个数
        setDevNumberCount(ui->treeWidget->getDevNumberComboBox()->count());
    }
}

/*************************************************************
/函数功能：硬件资源
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::on_actHard_triggered()
{
    ResHardware *hardCfg=new ResHardware;

    hardCfg->exec();
    initkeyList();

    delete hardCfg;
}

/*************************************************************
/函数功能：生成报告
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::on_actReport_triggered()
{
    DiaReport *reportDeal = new DiaReport;

    reportDeal->exec();
    delete reportDeal;
}

/*************************************************************
/函数功能：工具配置
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::on_actATtool_triggered()
{
    toolConfig *ATConfig = new toolConfig();

    if(ATConfig->exec()==QDialog::Accepted)
    {
        //此2个参数，为主函数的局部变量，未在子函数中赋值，需重新获取
        Model_iniSetting InfoINI;
        IsLogcatEnable  = InfoINI.ReadIni_item("LogcatEnable").toBool();
        logcatPath      = InfoINI.ReadIni_item("LogcatPath").toString();
    }

    delete ATConfig;
}

/*************************************************************
/函数功能：帮助
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::on_actHelp_triggered()
{
    QString pdfPath=QDir::currentPath()+"/ATtool使用说明.pdf";
    if(QDesktopServices::openUrl(QUrl::fromLocalFile(pdfPath))==false)
    {
        QMessageBox::warning(NULL, tr("提示"), tr("该运行目录下无《ATtool使用说明.pdf》文档！"));
    }

}

/*************************************************************
/函数功能：关于
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::on_about_triggered()
{
    QMessageBox::information(NULL, tr("关于"), tr("自动化测试系统 V1.05\n"
                                            "日期：2019.04.04\n"
                                            "版权：roadrover\n"
                                            "反馈邮箱：lishuhui@roadrover.cn"));

}

/*************************************************************
/函数功能：点击执行测试/结束测试
/函数参数：点击状态
/函数返回：无
*************************************************************/
void MainWindow::on_acttest_triggered(bool checked)
{
    if(UartConnectStatus())
    {
        if(checked)
        {
            QList <tUnit>*testList = ui->tableSequence->unitAll();
            if(testList->isEmpty()==false)
                startTheFlow(testList);
            else
                ui->textBrowser_EXEShow->append(tr("启动测试失败：测试序列不存在数据，请先添加测试序列！"));
        }
        else
            endTheFlow();
    }

    //处理后状态不一致，代表处理失败，按键状态恢复
    if(checked != getTestRunState())
    {
        ui->acttest->setChecked(!checked);
    }
}




/*---------------------------------------this is treeWidget option-----------------------------------------*/
/*************************************************************
/函数功能：串口打开槽函数
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::on_treeWidget_uartOpen(const QString &com, const QString &baud)
{
    UartOpen(com,baud);
}

/*************************************************************
/函数功能：串口关闭槽函数
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::on_treeWidget_uartClose()
{
    UartClose();
}

/*************************************************************
/函数功能：设备启用状态
/函数参数：状态
/函数返回：无
*************************************************************/
void MainWindow::on_treeWidget_devUseState(bool isUse)
{
    if(isUse)
    {
        devNumber = ui->treeWidget->getDevNumberComboBox()->currentText();
        //cout << NumberListIsSingle();
    }
    else
    {
        devNumber.clear();
    }
}



/*---------------------------------------this is AutoTest & tFlowDeal option---------------------------------------*/
/*************************************************************
/函数功能：测试单元开始执行
/函数参数：执行测试流
/函数返回：无
*************************************************************/
void MainWindow::startTheFlow(QList <tUnit> *testFlow)
{
    if((!isRunning)&&(testState==overtest))
    {
        if(devNumber.isEmpty())
        {//设备序列号
            if(QMessageBox::information(NULL, tr("警告"), tr("未启用设备序列号，是否继续?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No)==QMessageBox::No)//Upgrading
                return ;
        }

        if(ui->tableSequence->getSequenceFileName().isEmpty())
        {//提示保存文件：
            if(QMessageBox::information(NULL, tr("警告"), tr("未保存文件，是否继续?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No)==QMessageBox::No)//Upgrading
                return ;
        }

        if(IsLogcatEnable)
            startLogThread();//启动采集log线程

        exeFlow = testFlow;

        testState=getprop;
        timerTestID = startTimer(10);
        testTime = QDateTime::currentDateTime();

        ui->textBrowser_EXEShow->clear();
        ui->textBrowser_mShow->clear();
    }
    else
    {
        ui->textBrowser_EXEShow->append(tr("启动测试失败：请检查测试是否正在运行或正在生成报告！"));
    }
}

/*************************************************************
/函数功能：测试定时器执行处理
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::timerTestIDDeal()
{
    switch(testState)
    {
    case start:
    {
        //创建测试流程：
        tFlowDeal = new Model_tFlow(exeFlow);
        connect(tFlowDeal,SIGNAL(unitStartExe(tUnit)),this,SLOT(unitStartExeSlot(tUnit)));
        connect(tFlowDeal,SIGNAL(theUnitResult(bool)),this,SLOT(unitEndExeSlot(bool)));
        connect(tFlowDeal,SIGNAL(flowEndTest()),this,SLOT(onEndTestSlot()));
        connect(tFlowDeal,SIGNAL(keyClicked(QString)),this,SLOT(execKeyClicked(QString)));

        on_BtnCircularCurrent_clicked();

        isRunning = true;
        setIsRunInterface(true);

        ui->textBrowser_EXEShow->append(tr("启动测试:"));
        appendThePropertiesToFile(ResultPath(ui->tableSequence->getSequenceFileName()),"start_time:"+testTime.toString("yyyy.MM.dd-hh.mm.ss")+"\r\n");//添加时间

        testState=waitnull;
        break;
    }
    case waitnull:
    {
        if(testString.isEmpty()==false)
        {
            execKeyClicked(testString); //执行按键
            testString.clear();
        }
        break;
    }
    case getprop:
    {
        if(!isPRORunning)
        {
            isHadProp=false;
            //机器版本信息:
            ui->textBrowser_EXEShow->append(tr("获取机器版本信息，请稍后... ..."));
            appendThePropertiesToFile(ResultPath(ui->tableSequence->getSequenceFileName()),"clear");

            if(getDevNumber().isEmpty())
            {
                if(NumberListIsSingle())
                    proList.append(GETPROP);
                else
                    testState = start;
            }
            else
                proList.append(GETPROP_S(getDevNumber()));
        }
        break;
    }
    case report:
    {
        if(!isPRORunning)
        {
            isHadReport = false;
            ui->textBrowser_EXEShow->append(tr("正在生成报告，请稍后... ..."));
            proList.append(PYTHONREPORT(ResultPath(ui->tableSequence->getSequenceFileName())+ "/" + testTime.toString("yyyyMMddhhmmss")+"/"));
        }
        break;
    }
    case overtest:
    {
        delete tFlowDeal;
        killTimer(timerTestID);
        isRunning=false;
        setIsRunInterface(false);
        appendTheExecLogInfo(ui->textBrowser_EXEShow->toPlainText());
        chartDeal->clearSerials();
        on_BtnOverCurrent_clicked();

        break;
    }
    }
}

/*************************************************************
/函数功能：测试进程输出处理
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::testProcessOutputDeal(QString String)
{
    if(testState == getprop)
    {
        if(String.contains("Out>>"))
        {
            isHadProp=true;
            appendThePropertiesToFile(ResultPath(ui->tableSequence->getSequenceFileName()),String.remove("Out>>"));
        }
    }
    else if(testState == report)
    {
        //判断执行OK send email success !
        if(String.contains("send email success !"))
            isHadReport=true;
        cout << String;
    }
}

/*************************************************************
/函数功能：测试进程结束处理
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::testProcessOverDeal()
{
    if(testState == getprop)
    {
        if(isHadProp)
            testState = start;
    }
    else if(testState == report)
    {
        if(isHadReport)
            ui->textBrowser_EXEShow->append(tr("报告生成结束，请查找本地对应目录或邮件或")+tr("<html><a href=\"%1\">点击查阅</a></html>")
                                        .arg("http://192.168.13.96/result/"+ ui->tableSequence->getSequenceFileName()+ "/" + testTime.toString("yyyyMMddhhmmss")+"/report.html"));//ResultPath+"/"
        else
            ui->textBrowser_EXEShow->append(tr("生成失败，请检查原因后，手动生成。执行后台分析文件：customMessageLog.txt"));
        testState = overtest;
    }

}

/*************************************************************
/函数功能：获取测试运行状态
/函数参数：无
/函数返回：无
*************************************************************/
bool MainWindow::getTestRunState()
{
    return isRunning;
}

/*************************************************************
/函数功能：结束测试
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::endTheFlow()
{
    if(getTestRunState())
    {
        //进程若正在执行，终止：只针对uiautomator，bat中的其他命令继续执行
        if(proSysIsRunning())
            proStopSysUiautomator();

        tFlowDeal->endTheTest();
        txList.clear();//结束即清空传输数据帧
        isRunning=false;
        appendThePropertiesToFile(ResultPath(ui->tableSequence->getSequenceFileName()),"end_time:"+QDateTime::currentDateTime().toString("yyyy.MM.dd-hh.mm.ss")+"\r\n");
    }
}

/*************************************************************
/函数功能：测试单元结束执行
/函数参数：无
/函数返回：无  any:测试停止时若机器关，需要将机器开启恢复机器工作状态再进行获取信息，生成报告
*************************************************************/
void MainWindow::onEndTestSlot()
{
    ui->textBrowser_EXEShow->append(tr("结束测试！"));

    if(ReportCreat)
        testState = report;
    else
        testState = overtest;

    if(IsLogcatEnable)
        stopLogThread();
}

/*************************************************************
/函数功能：测试单元开始执行
/函数参数：单元信息
/函数返回：无
*************************************************************/
void MainWindow::unitStartExeSlot(tUnit eUnit)
{
    QDateTime currentTime = QDateTime::currentDateTime();
    int row=ui->tableWidget->rowCount();

    ui->label_ItemName->setText(eUnit.name);

    ui->tableWidget->setRowCount(row+1);
    ui->tableWidget->setItem(row,colUnit,new QTableWidgetItem(QIcon(":/test/current.png"),eUnit.name));
    ui->tableWidget->setItem(row,colAct,new QTableWidgetItem(currentTime.toString("yyyy.MM.dd hh:mm:ss.zzz")));

    ui->tableWidget->setColumnWidth(colUnit,150);
    ui->tableWidget->setColumnWidth(colAct,150);

    QScrollBar *scrollBar = ui->tableWidget->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());

    savePath = ResultPath(ui->tableSequence->getSequenceFileName())
                + "/" + testTime.toString("yyyyMMddhhmmss")
                + "/" + eUnit.name
                + "/Loop"+toStr(tFlowDeal->getTheFlowLoop()) ;
    if(appendTheResultToFile("StartTheUnit:"+currentTime.toString("yyyy.MM.dd-hh.mm.ss")+"    name:"+eUnit.name)==false)
    {
        cout<<("保存报告路径创建有误，请检查"+savePath+"/report.txt ");
        endTheFlow();
    }
}

/*************************************************************
/函数功能：测试单元结束执行
/函数参数：单元信息
/函数返回：无
*************************************************************/
void MainWindow::unitEndExeSlot(bool exeResult)
{
    int row=ui->tableWidget->rowCount();

    ui->label_ItemName->setText("");

    if(exeResult)
    {
        ui->tableWidget->setItem(row-1,colResult,new QTableWidgetItem(QIcon(":/test/pass.png"),NULL));
    }
    else
    {
        ui->tableWidget->setItem(row-1,colResult,new QTableWidgetItem(QIcon(":/test/fail.png"),NULL));
    }

    ui->tableWidget->setItem(row-1,colStr,new QTableWidgetItem(tr("测试结束")));

    appendTheResultToFile("EndTheUnit:"+QDateTime::currentDateTime().toString("yyyy.MM.dd-hh.mm.ss")+"    result:"+QString::number(exeResult)+"\r\n\r\n");
}

/*************************************************************
/函数功能：测试流执行按键点击
/函数参数：按键
/函数返回：无
/备注：测试流中按键字符串组成：KEY1:Name:on/off || KEY1:Name
/以：分割   制定on/off或无
*************************************************************/
void MainWindow::execKeyClicked(QString key)
{
    char buf[2]={0};
    int keyNum = getKeyNumber(key);

    if(keyNum != -1)
    {
        buf[0] = keyNum;
        if(key.contains(":on"))
            buf[1]=true;
        else
            buf[1]=false;
        appendTxList(CMDClickedKey,buf,2,CMD_NEEDACK);
        //cout << key;
    }
}

/*************************************************************
/函数功能：检测来自硬件的参数
/函数参数：chk：检测的项
/函数返回：无
*************************************************************/
void MainWindow::chkParamFromHardware(uint8_t chk)
{
    char buf=0;

    if(chk==CHKCurrent)
        appendTxList(Upload_SingleCurrent,&buf,1,CMD_NEEDNACK);
    else if(chk==CHKSound)
        appendTxList(Upload_SingleAudio,&buf,1,CMD_NEEDNACK);
    else if(chk==CHKVlot)
        appendTxList(Upload_SingleVB,&buf,1,CMD_NEEDNACK);
}




/*---------------------------------------this is UART option--------------------------------------------*/
/*************************************************************
/函数功能：初始化串口参数
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::initUartParam()
{
    UARTDeal=new Model_UART;
    ackWait=0;
    txList.clear();
}

/*************************************************************
/函数功能：释放串口参数
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::deleteUartParam()
{
    //若串口打开,关串口
    if(UARTDeal->isOpenCurrentUart())
        UartClose();

    delete UARTDeal;
}

/*************************************************************
/函数功能：串口打开
/函数参数：端口，波特率
/函数返回：无
*************************************************************/
void MainWindow::UartOpen(const QString &com, const QString &baud)
{
    UARTDeal->Open(com,baud);

    if(!UARTDeal->isOpenCurrentUart())
        ui->treeWidget->setCheckedState(topUart,false);
    else
    {
        connect(UARTDeal,SIGNAL(RxFrameDat(char,uint8_t,char*)),this,SLOT(UartRxDealSlot(char,uint8_t,char*)));
        connect(UARTDeal,SIGNAL(UartByteArrayBackStage(QByteArray,uartDir,bool)),this,SLOT(UartByteArraySlot(QByteArray,uartDir,bool)));
        connect(UARTDeal,SIGNAL(UartRxAckResault(bool)),this,SLOT(UartRxAckResault(bool)));
        connect(UARTDeal,SIGNAL(UartDisConnect()),this,SLOT(UartDisconnect()));

        timerUartID=startTimer(CmdACKDelay);
    }
}

/*************************************************************
/函数功能：串口关闭
/函数参数：无
/函数返回：无
/备注：断开串口前要断开信号连接，否则将会出现信号多次发送的现象照成帧显示错误；
*************************************************************/
void MainWindow::UartClose()
{
    killTimer(timerUartID);
    UARTDeal->disconnect();
    UARTDeal->Close();
}

/*************************************************************
/函数功能：串口连接状态
/函数参数：无
/函数返回：无
*************************************************************/
bool MainWindow::UartConnectStatus()
{
    bool status=UARTDeal->isOpenCurrentUart();
    if(!status)
    {
        QMessageBox::warning(NULL, tr("警告"), tr("未打开串口，请处理！"));
        endTheFlow();//内里有判断，若在测试将结束处理
    }
    return status;
}

/*************************************************************
/函数功能：处理定时串口ID数据
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::timerUartIDDeal()
{
    if((txList.isEmpty() == false) && (!ackWait))
    {
        uartFrame uartDat=txList.at(0);

        if(uartDat.ack == CMD_NEEDACK)
        {
            ackWait=1;
        }

        cout << "txList:" <<txList.length();
        UARTDeal->UartTxCmdDeal(uartDat.cmd,uartDat.dat,uartDat.len,uartDat.ack);
        txList.removeFirst();

    }
    else if(ackWait)
    {
        //cout << ackWait;
        if((++ackWait) > (CmdReSendTimer+2))
        {
            ackWait=0;
            txList.clear();

            QMessageBox::warning(NULL, tr("警告"), tr("串口响应失败，请检查！"));
            endTheFlow();
        }
    }
}

/*************************************************************
/函数功能：填充串口发送列表
/函数参数：同命令发送
/函数返回：无
*************************************************************/
void MainWindow::appendTxList(char cmd,char* dat,char len,uint8_t ack)
{
    if(UartConnectStatus())
    {
        uartFrame uartDat;

        uartDat.cmd = cmd;
        for(int i=0;i<len;i++)
        {
            uartDat.dat[i] = dat[i];
        }

        uartDat.len = len;
        uartDat.ack = ack;
        txList.append(uartDat);
    }
}

/*************************************************************
/函数功能：串口字符串后台显示数据传输
/函数参数：revDats:接收字符串数据 dir：传输方向  isHex：是否为hex数据
/函数返回：无
*************************************************************/
void MainWindow::UartByteArraySlot(QByteArray revDats,uartDir dir,bool isHex)
{
    QString strShow;

    if(isHex==Uart_Hex)
        strShow=qtStrDeal->hexToString((unsigned char *)revDats.data(),revDats.length());//hex显示
    else if(isHex==Uart_NHex)
        strShow=revDats;

    if(dir==Uart_Rx)
        ui->textBrowser_mShow->append("Rx: "+strShow);
    else if(dir==Uart_Tx)
        ui->textBrowser_mShow->append("Tx: "+strShow);
    else
        ui->textBrowser_mShow->append("Warn: "+strShow);
}

/*************************************************************
/函数功能：串口接收处理函数
/函数参数：接收帧数据
/函数返回：无
*************************************************************/
void MainWindow::UartRxDealSlot(char cmd,uint8_t dLen,char *dat)
{
    unsigned int tempDat=0;

    if(cmd == Upload_SingleCurrent)
    {
        for(int i=0;i<dLen;i++)
        {
            tempDat=((uint8_t)dat[i]<<(i*8))|tempDat;//低位在前，高位在后
        }
        Current = tempDat;

        chartDeal->refreshChart(CHKCurrent,tempDat/1000.0);
    }
    else if(cmd == Upload_SingleVB )
    {
        for(int i=0;i<dLen;i++)
        {
            tempDat=((uint8_t)dat[i]<<(i*8))|tempDat;//低位在前，高位在后
        }
        Volt = tempDat;
        //cout << Volt;

        chartDeal->refreshChart(CHKVlot,tempDat/100.0);
    }
    else if(cmd == Upload_SingleAudio)
    {
        if(dLen==1)
        {
            SoundV = dat[0];
            chartDeal->refreshChart(CHKSound,SoundV);
        }
    }
}

/*************************************************************
/函数功能：帧响应结果
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::UartRxAckResault(bool result)
{
    //cout << "Frame ack is " << result;
    if(!result)
        cout <<"Frame Chk is Error.";
    else
    {
        ackWait=0;
        actIsRunning = false;//KEY有响应时结束测试流动作
    }
}

/*************************************************************
/函数功能：串口断开
/函数参数：无
/函数返回：无
/备注：原本想做一个按键的翻转，但是只翻转选中项目没有意义，保留功能
*************************************************************/
void MainWindow::UartDisconnect()
{
    //ui->treeWidget->setCheckedState(topUart,false);
    endTheFlow();
}



/*---------------------------------------this is Process option-----------------------------------------*/
/*************************************************************
/函数功能：初始化进程处理
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::initProcessDeal()
{
    PRODeal = new Model_Process;
    connect(PRODeal,SIGNAL(ProcessisOver(uint8_t)),this,SLOT(onProcessOverSlot(uint8_t)));
    connect(PRODeal,SIGNAL(ProcessOutDeal(int,QString)),this,SLOT(onProcessOutputSlot(int,QString)));

    PRODeal->ProcessPathJump(QCoreApplication::applicationDirPath());
    timerProID = startTimer(1);
    isPRORunning=false;
    currentCMDString.clear();
}

/*************************************************************
/函数功能：释放进程处理
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::deleteProcessDeal()
{
    killTimer(timerProID);
    delete PRODeal;
}

/*************************************************************
/函数功能：进程执行处理
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::timerProIDDeal()
{
    if((proList.isEmpty()==false)&&(!proSysIsRunning()))
    {
        //cout << proList.length();
        PRODeal->ProcessStart(PROSYS,proList.first());

        currentCMDString = proList.first();
        isPRORunning=true;
        proList.removeFirst();
    }
}

/*************************************************************
/函数功能：进程输出槽函数处理
/函数参数：进程号  字符串
/函数返回：无
*************************************************************/
void MainWindow::onProcessOutputSlot(int pNum,QString String)
{
    //进程处理
    if((pNum==PROSYS)&&(String.isEmpty()==false))
    {
        if(String.contains("Error>>"))
            ui->textBrowser_EXEShow->append(String);
        else
        {
            if((!getTestRunState())&&(currentCMDString == ADBDevs))
            {//处理设备扫描进程:显示设备列表
                QStringList devList = String.split("\r\n");

                if(devList.isEmpty()==false)
                    devList.removeFirst();

                if(devList.isEmpty()==false)
                {
                    for(int i=0;i<devList.length();)
                    {
                        QString tempString=devList.at(i);
                        if(tempString.contains("\tdevice"))
                        {
                            devList.replace(i,tempString.remove("\tdevice"));
                            if(devList.at(i).isEmpty())
                                devList.removeAt(i);
                            else
                                i++;
                        }
                        else
                            devList.removeAt(i);
                    }
                    ui->treeWidget->refreshDevNum1(devList);
                }
            }
            else
            {
                //测试进程输出处理
                testProcessOutputDeal(String);
                //cout<< String;
            }
        }
    }

}

/*************************************************************
/函数功能：进程结束处理
/函数参数：进程号
/函数返回：无
*************************************************************/
void MainWindow::onProcessOverSlot(uint8_t pNum)
{
    if(pNum==PROSYS)
    {
        //测试进程结束处理
        testProcessOverDeal();

        //恢复状态
        currentCMDString.clear();
        isPRORunning=false;
    }
}

/*************************************************************
/函数功能：停止系统进程执行logcat
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::proStopSysLogcat()
{
    PRODeal->stopProcess(getDevNumber(),"logcat");
}

/*************************************************************
/函数功能：停止系统进程执行uiautomator
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::proStopSysUiautomator()
{
    PRODeal->stopProcess(getDevNumber(),"uiautomator");
}

/*************************************************************
/函数功能：判断系统进程是否正在执行
/函数参数：无
/函数返回：无
/备注：判断的只是bat中某一条指令是否执行并不能判断bat执行是否正在运行
*************************************************************/
bool MainWindow::proSysIsRunning()
{
    if(PRODeal->GetProcessRunStatus(PROSYS) != noRun)
        return true;
    else
        return false;
}

/*---------------------------------------this is LogcatThread option-----------------------------------------*/
void MainWindow::initLogcatThreadDeal()
{
    logThreadDeal =new model_ThreadLog();
}
void MainWindow::deleteLogcatThreadDeal()
{
    delete logThreadDeal;
}

void MainWindow::startLogThread()
{
    logThreadDeal->SetRunPath(logcatPath);
    logThreadDeal->SetRunDEV(getDevNumber());
    logThreadDeal->start();
}

void MainWindow::stopLogThread()
{
    logThreadDeal->stop();
    proStopSysLogcat();
}

/*---------------------------------------this is test fun option-----------------------------------------*/
/*************************************************************
/函数功能：点击按键操作
/函数参数：无
/函数返回：无
/备注：手动点击按键，用来测试
*************************************************************/
void MainWindow::on_pushButton_2_clicked()
{
    QStringList theAct=ui->comboBox->currentText().split(':');
    char buf[2]={0};
    if(theAct.isEmpty()==false)
    {
        buf[0] = theAct.at(0).right(1).toInt();//KEY1  取"1"

        if(ui->checkBox->checkState() == Qt::Checked)
            buf[1]=1;
        else
            buf[1]=0;
        appendTxList(CMDClickedKey,buf,2,CMD_NEEDACK);
    }
}

/*************************************************************
/函数功能：初始化按键列表
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::initkeyList()
{
    Model_XMLFile xmlRead;
    QList <keyControl> keyList;
    QStringList comboList;

    xmlRead.readKeyInfoXML(WorkItem,&keyList);

    if(keyList.isEmpty()==false)
    {
        for(int i=0;i<keyList.length();i++)
        {
            if(keyList.at(i).isUse)
            {
                comboList.append("KEY"+QString::number(i+1)+":"+keyList.at(i).name);

                //标记特殊按键处理字符串：
                if(keyList.at(i).type == HardACC)
                {
                    AccKey = comboList.last();
                }
                else if(keyList.at(i).type == HardBAT)
                {
                    BatKey = comboList.last();
                }
            }
        }
    }
    ui->comboBox->clear();
    ui->comboBox->addItems(comboList);
}

/*************************************************************
/函数功能：显示帧或隐藏帧
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::on_pushButtonhide_clicked(bool checked)
{
    if(checked)
    {
        ui->pushButtonhide->setText(tr("显示帧"));
        ui->textBrowser_mShow->setVisible(false);
    }
    else
    {
        ui->pushButtonhide->setText(tr("隐藏帧"));
        ui->textBrowser_mShow->setVisible(true);
    }
}

/*************************************************************
/函数功能：点击获取参数
/函数参数：无
/函数返回：无
*************************************************************/
void MainWindow::on_btnReadCurrent_clicked()
{
    chkParamFromHardware(0x00);
}

void MainWindow::on_BtnCircularCurrent_clicked()
{
    char buf=200;
    appendTxList(Upload_CircularCurrent,&buf,1,CMD_NEEDNACK);
    //appendTxList(Upload_CircularVB,&buf,1,CMD_NEEDNACK);
    //appendTxList(Upload_CircularAudio,&buf,1,CMD_NEEDNACK);
}

void MainWindow::on_BtnOverCurrent_clicked()
{
    char buf=0;
    appendTxList(CMDOverCurrentUp,&buf,1,CMD_NEEDNACK);//any:若串口断开，结束测试后仍会发送一帧数据使其断开
    //appendTxList(CMDOverVBUp,&buf,1,CMD_NEEDNACK);
    //appendTxList(CMDOverAudioUp,&buf,1,CMD_NEEDNACK);
}
