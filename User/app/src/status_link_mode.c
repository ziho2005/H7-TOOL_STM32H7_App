/*
*********************************************************************************************************
*
*    模块名称 : 联机界面
*    文件名称 : status_link_mode.c
*    版    本 : V1.0
*    说    明 : 
*    修改记录 :
*        版本号  日期        作者     说明
*        V1.0    2019-10-06 armfly  正式发布
*
*    Copyright (C), 2019-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"
#include "main.h"
#include "usbd_user.h"

#define DATE_X      5
#define DATE_Y      40

#define WEEK_X      5 + 148
#define WEEK_Y      DATE_Y

#define TIME_X      120 - (5 * 16 / 2)
#define TIME_Y      DATE_Y + 30

#define RJ45_IP_X   5
#define RJ45_IP_Y   TIME_Y + 60

#define WIFI_IP_X   5
#define WIFI_IP_Y   RJ45_IP_Y + 20

#define UDP_PORT_X  5
#define UDP_PORT_Y  WIFI_IP_Y + 20

static void DispLinkStatus(void);
static void DispClock(void);

/*
*********************************************************************************************************
*    函 数 名: status_LinkMode
*    功能说明: 联机模式（功能由上位机控制）
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void status_LinkMode(void)
{
    uint8_t ucKeyCode; /* 按键代码 */
    uint8_t fRefresh;
    uint8_t fIgnoreKey = 0;
    uint8_t LastMinute = 99;

    DispHeader("联机模式");
    DispHelpBar("长按S进入系统设置",
                "长按C切换方向");  
    
    usbd_CloseCDC();
    usbd_OpenCDC(COM_USB1); /* 启用USB虚拟串口8， 用于和PC软件USB通信 */

    fRefresh = 1;
    
    RTC_ReadClock();
    DispClock();    /* 显示时钟 */
    
    bsp_StartAutoTimer(0, 1000);    
    while (g_MainStatus == MS_LINK_MODE)
    {
        if (fRefresh)   /* 刷新整个界面 */
        {
            fRefresh = 0;
            
            DispLinkStatus();
        }
        
        if (bsp_CheckTimer(0))
        {            
            RTC_ReadClock();
            
            if (LastMinute != g_tRTC.Min)
            {
                LastMinute = g_tRTC.Min;
                DispClock();    /* 显示时钟 */
            }
        }

        bsp_Idle();
        
        ucKeyCode = bsp_GetKey();   /* 读取键值, 无键按下时返回 KEY_NONE = 0 */
        if (ucKeyCode != KEY_NONE)
        {
            /* 有键按下 */
            switch (ucKeyCode)
            {
            case KEY_DOWN_S:        /* S键按下 */
                break;

            case KEY_UP_S:          /* S键释放 */
                g_MainStatus = NextStatus(g_MainStatus);
                break;

            case KEY_LONG_DOWN_S:   /* S键长按 */
                PlayKeyTone();
                g_MainStatus = MS_SYSTEM_SET;
                break;

            case KEY_DOWN_C:        /* C键按下 */
                break;

            case KEY_UP_C:          /* C键释放 */
                if (fIgnoreKey == 1)
                {
                    fIgnoreKey = 0;
                    break;
                }
                g_MainStatus = LastStatus(g_MainStatus);
                break;

            case KEY_LONG_DOWN_C:        /* C键 */
                PlayKeyTone();
                if (++g_tParam.DispDir > 3)
                {
                    g_tParam.DispDir = 0;
                }
                LCD_SetDirection(g_tParam.DispDir);
                SaveParam();
                DispHeader("联机模式");
                fRefresh = 1;
                fIgnoreKey = 1; /* 需要忽略J即将到来的弹起按键 */
                break;

            default:
                break;
            }
        }
    }

	if (g_MainStatus != MS_SYSTEM_SET)
    {
        usbd_CloseCDC();
        usbd_OpenCDC(COM1); /* 启用USB虚拟串口1， 用于虚拟串口，RS232 RS485 TTL-UART */
    }
    
    DSO_StartMode2();   /* 示波器启动模式2-低速多通道扫描 */
    
    bsp_StopTimer(0);  
}


/*
*********************************************************************************************************
*    函 数 名: DispLinkStatus
*    功能说明: 显示连接状态
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void DispLinkStatus(void)
{
    FONT_T tFont;
    char buf[48];

    /* 设置字体参数 */
    {
        tFont.FontCode = FC_ST_16;          /* 字体代码 16点阵 */
        tFont.FrontColor = INFO_NAME_COLOR; /* 字体颜色 */
        tFont.BackColor = INFO_BACK_COLOR;  /* 文字背景颜色 */
        tFont.Space = 0;                    /* 文字间距，单位 = 像素 */
    }    
        
//        sprintf(buf, "%02X-%02X-%02X-%02X-%02X-%02X",
//                        g_tVar.MACaddr[0], g_tVar.MACaddr[1], g_tVar.MACaddr[2],
//                        g_tVar.MACaddr[3], g_tVar.MACaddr[4], g_tVar.MACaddr[5]);
//        DispInfoBar16(0, "以太网MAC:", buf);

    sprintf(buf, "RJ45 IP地址:%d.%d.%d.%d", g_tParam.LocalIPAddr[0], g_tParam.LocalIPAddr[1],
                    g_tParam.LocalIPAddr[2], g_tParam.LocalIPAddr[3]);
    LCD_DispStr(RJ45_IP_X, RJ45_IP_Y, buf, &tFont);
    
    sprintf(buf, "WiFi IP地址:%d.%d.%d.%d", g_tParam.LocalIPAddr[0], g_tParam.LocalIPAddr[1],
                    g_tParam.LocalIPAddr[2], g_tParam.LocalIPAddr[3]);
    LCD_DispStr(WIFI_IP_X, WIFI_IP_Y, buf, &tFont);    
    
    sprintf(buf, "端口号:%d", g_tParam.LocalTCPPort);
    LCD_DispStr(UDP_PORT_X, UDP_PORT_Y, buf, &tFont); 
    
    
}

/*
*********************************************************************************************************
*    函 数 名: DispClock
*    功能说明: 显示时钟. 时钟数据在g_tRTC
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void DispClock(void)
{
    FONT_T tFont;
    char buf[48];
    const char *WeekStr[7] = {"星期一", "星期二", "星期三", "星期四", "星期五", "星期六",  "星期日"};
    
    /* 设置字体参数 */
    {
        tFont.FontCode = FC_ST_24;
        tFont.FrontColor = INFO_NAME_COLOR; /* 字体颜色 */
        tFont.BackColor = INFO_BACK_COLOR;  /* 文字背景颜色 */
        tFont.Space = 0;                    /* 文字间距，单位 = 像素 */
    }    
    
    /*
        2019-12-01 星期一
             00:00
    */
    sprintf(buf, "%04d-%02d-%02d", g_tRTC.Year, g_tRTC.Mon, g_tRTC.Day);
    LCD_DispStr(DATE_X, DATE_Y, buf, &tFont);

    if (g_tRTC.Week >= 1 && g_tRTC.Week <= 7)
    {
        LCD_DispStr(WEEK_X, WEEK_Y, (char *)WeekStr[g_tRTC.Week - 1], &tFont);    
    }
    else
    {
        LCD_DispStr(WEEK_X, WEEK_Y, "------", &tFont);   
    }
    
    tFont.FontCode = FC_ST_32;
    sprintf(buf, "%02d:%02d", g_tRTC.Hour, g_tRTC.Min);
    LCD_DispStr(TIME_X, TIME_Y, buf, &tFont);    
   
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
