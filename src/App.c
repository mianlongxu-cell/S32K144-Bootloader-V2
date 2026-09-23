#include "App.h"
#include "Com.h"
#include "Dem.h"
#include "CanTp.h"
#include "Dcm.h"
#include "PduR.h"
#include "Rte.h"
#include "VehicleApp.h"

void App_Init(void)
{
    Rte_Init();//初始化RTE
    VehicleApp_Init();//先初始化车辆模型
    CanTp_Init();//初始化ISO-TP收发状态机
    Dcm_Init();//初始化诊断通信管理
    Dem_Init();//初始化诊断事件管理，也就是DTC管理模块
    Com_Init();//初始化普通CAN通信配置
    PduR_Init();//初始化PDU路由模块
}
