/*
*********************************************************************************************************
*
*    模块名称 : H7-TOOL App主程序
*    文件名称 : main.c
*    版    本 : V1.1
*    说    明 : 
*
*    修改记录 :
*        版本号  日期        作者     说明
*        V1.0    2019-10-01 armfly  正式发布
*        V1.1    2019-11-02 armfly  整理格式。增加功能。
*
*    Copyright (C), 2019-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"
#include "main.h"

#include "status_link_mode.h"
#include "status_voltage_meter.h"
#include "status_current_meter.h"
#include "status_resistor_meter.h"
#include "status_temp_meter.h"
#include "status_programmer.h"
#include "status_system_set.h"
#include "status_tvcc_power.h"
#include "status_pulse_meter.h"

#include "wifi_if.h"
#include "ff.h"
#include "ff_gen_drv.h"
#include "emmc_diskio_dma.h"

#include "lwip_user.h"
#include "lua_if.h"

#include "target_reset.h"
#include "target_config.h"
#include "swd_host.h"

//#include "usbd_user.h"
#include "usb_if.h"

static void DispLogo(void);
uint16_t GetStatusIndex(uint16_t _NowStatus);

uint16_t g_MainStatus;  /* 主状态字 */

/* 主状态切换顺序 */
static const uint16_t StatusOrder[] =
{
    MS_LINK_MODE,        /* 联机状态 */
    MS_VOLTAGE_METER,    /* 电压表 */
    MS_RESISTOR_METER,   /* 电阻表 */
    MS_CURRENT_METER,    /* 高侧电流表 */
    MS_TEMP_METER,       /* 温度表 */
    MS_TVCC_POWER,       /* 微型数控电源 */   
    MS_PULSE_METER,      /* 脉冲计 */
    MS_PROG_INIT,        /* 脱机下载器 */    
};

/*
*********************************************************************************************************
*    函 数 名: main
*    功能说明: c程序入口
*    形    参: 无
*    返 回 值: 错误代码(无需处理)
*********************************************************************************************************
*/
int main(void)
{
    bsp_Init();
    LoadParam(); /* 读取应用程序参数, 该函数在param.c */

    PERIOD_Start(&g_tRunLed, 50, 50, 0); /* LED一直闪烁, 非阻塞 */

    DispLogo();
    
    bsp_InitESP32();

    DSO_InitHard();
    DSO_SetDC(1, 1);
    DSO_SetDC(2, 1);
    DSO_SetGain(1, 3);
    DSO_SetGain(2, 3);

    //usbd_OpenMassStorage();
    bsp_SetTVCC(3300);

    /* LwIP 初始化 */
    {
        /* 如果不插网线，此函数执行时间过长 */
        /* 网络参数存在在全局变量 g_tParam.lwip_ip, g_tParam.lwip_net_mask, g_tParam.lwip_gateway */
        lwip_start();

        lwip_pro();
    }

    //PERIOD_Stop(&g_tRunLed); /* 停止LED闪烁 */    
    PERIOD_Start(&g_tRunLed, 1000, 1000, 0);    /* LED一直闪烁, 每2秒闪1次 */
    
    usbd_Init();    /* 初始化USB协议栈 */
    
    //wifi_state = WIFI_INIT;

    /* 主程序采用状态机实现程序功能切换 */
    g_MainStatus = MS_LINK_MODE; /* 初始状态 = 联机界面 */
    while (1)
    {
        switch (g_MainStatus)
        {
        case MS_LINK_MODE:      /* 联机状态 */
            status_LinkMode();
            break;

        case MS_SYSTEM_SET:     /* 系统设置 */
            status_SystemSetMain();
            break;

        case MS_HARD_INFO:      /* 硬件信息 */
            status_HardInfo();
            break;

        case MS_ESP32_TEST:     /* ESP32模块固件升级 */
            status_ESP32Test();
            break;

        case MS_USB_UART1:      /* USB虚拟串口，映射到硬件UART1， RS485 RS232 */
            status_UsbUart1();
            break;

        case MS_MODIFY_PARAM:   /* 修改参数 */
            status_ModifyParam();
            break;          

        case MS_PROG_INIT:      /* 脱机下载器预览界面 */
            status_ProgInit();
            break;

        case MS_PROG_WORK:      /* 脱机下载器正式工作 */
            status_ProgWork();
            break;        

        case MS_VOLTAGE_METER:  /* 电压表 */
            status_VoltageMeter();
            break;

        case MS_CURRENT_METER:  /* 高侧电流表 */
            status_CurrentMeter();
            break;

        case MS_TEMP_METER:     /* 温度表 */
            status_TempMeter();
            break;

        case MS_RESISTOR_METER: /* 电阻表 */
            status_ResistorMeter();
            break;

        case MS_TVCC_POWER:     /* 微型电源 */
            status_TVCCPower();
            break;        
        
        case MS_PULSE_METER:    /* 脉冲测量 */
            status_PulseMeter();
            break;  
        
        default:
            g_MainStatus = MS_LINK_MODE;
            break;
        }
    }
}


/*
*********************************************************************************************************
*    函 数 名: NextStatus
*    功能说明: 状态切换, 向后翻
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint16_t NextStatus(uint16_t _NowStatus)
{
    uint16_t next;
    uint16_t i;
    uint16_t count;

    count = sizeof(StatusOrder) / 2;

    for (i = 0; i < count; i++)
    {
        if (_NowStatus == StatusOrder[i])
        {
            next = i;
            break;
        }
    }

    if (++next >= count)
    {
        next = 0;
    }

    PlayKeyTone();
    return StatusOrder[next];
}

/*
*********************************************************************************************************
*    函 数 名: LastStatus
*    功能说明: 状态切换, 向前翻
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint16_t LastStatus(uint16_t _NowStatus)
{
    uint16_t next;
    uint16_t i;
    uint16_t count;

    count = sizeof(StatusOrder) / 2;

    for (i = 0; i < count; i++)
    {
        if (_NowStatus == StatusOrder[i])
        {
            next = i;
            break;
        }
    }

    if (next > 0)
    {
        next--;
    }
    else
    {
        next = count - 1;
    }
    PlayKeyTone();
    return StatusOrder[next];
}

/*
*********************************************************************************************************
*    函 数 名: GetStatusIndex
*    功能说明: 根据状态字获取菜单序号
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
uint16_t GetStatusIndex(uint16_t _NowStatus)
{
    uint16_t idx;
    uint16_t i;
    uint16_t count;

    count = sizeof(StatusOrder) / 2;

    for (i = 0; i < count; i++)
    {
        if (_NowStatus == StatusOrder[i])
        {
            idx = i;
            break;
        }
    }

    return idx;
}

/*
*********************************************************************************************************
*    函 数 名: DispLogo
*    功能说明: 开机显示版本号
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
static void DispLogo(void)
{
    FONT_T tFont; /* 定义字体结构体变量 */

    tFont.FontCode = FC_ST_16;                  /* 字体代码 16点阵 */
    tFont.FrontColor = RGB(200, 200, 200);      /* 字体颜色 */
    tFont.BackColor = FORM_BACK_COLOR;          /* 文字背景颜色 */
    tFont.Space = 0;                            /* 文字间距，单位 = 像素 */

    ST7789_SetDirection(g_tParam.DispDir);

    LCD_ClrScr(FORM_BACK_COLOR);    /* 清屏，背景蓝色 */

    /* 显示APP固件版本。版本号放在中断向量表 */
    {
        char buf[64];
        uint16_t x = 5;
        uint16_t y = 3;
        uint16_t line_cap = 20;

        //LCD_DispStr(x, y, "H7-TOOL多功能开发工具", &tFont);
        LCD_DispStr(x, y, "H7-TOOL", &tFont);
        y += line_cap;

        sprintf(buf, "App Ver:%d.%02X",
                        APP_VERSION >> 8, APP_VERSION & 0xFF);
        LCD_DispStr(x, y, buf, &tFont);

        y += line_cap;
        sprintf(buf, "Boot Ver:%d.%02X",
                        BOOT_VERSION >> 8, BOOT_VERSION & 0xFF);
        LCD_DispStr(x, y, buf, &tFont);

        y += line_cap;
        LCD_DispStr(x, y, "正在配置网络...", &tFont);
    }

    ST7789_DrawScreen();        /* 立即刷屏一次 */
    
    bsp_DelayMS(50);            /* 刷屏后延迟50ms再开背光, 避免花屏闪一下问题 */
    
    LCD_SetBackLight(BRIGHT_DEFAULT);   /* 打开背光 */    
}

/*
*********************************************************************************************************
*    函 数 名: DispHeader
*    功能说明: 显示界面标题（抬头第1行）
*    形    参: _str : 标题文字
*    返 回 值: 无
*********************************************************************************************************
*/
void DispHeader(char *_str)
{    
    uint8_t idx;
    
    idx  = GetStatusIndex(g_MainStatus);
    DispHeader2(idx, _str);
}

/* 带序号参数 */
void DispHeader2(uint8_t _idx, char *_str)
{
    FONT_T tFont;
    char buf[48];

    /* 设置字体参数 */
    {
        tFont.FontCode = FC_ST_24;          /* 字体代码 16点阵 */
        tFont.FrontColor = HEAD_TEXT_COLOR; /* 字体颜色 */
        tFont.BackColor = HEAD_BACK_COLOR;  /* 文字背景颜色 */
        tFont.Space = 0;                    /* 文字间距，单位 = 像素 */
    }
    
    LCD_Fill_Rect(0, 0, HEAD_HEIGHT, HEAD_HEIGHT, HEAD_SN_COLOR);       /* 清屏标题序号区  */
    LCD_Fill_Rect(HEAD_HEIGHT, 0, HEAD_HEIGHT, 240 - HEAD_HEIGHT, HEAD_BACK_COLOR);   /* 清屏标题栏文本区  */
    
    sprintf(buf, "%02d", _idx);
    tFont.BackColor = HEAD_SN_COLOR;    /* 文字背景颜色 */
    LCD_DispStr(4, 4, buf, &tFont);
    
    /* 显示标题文字 */
    tFont.BackColor = HEAD_BACK_COLOR;  /* 文字背景颜色 */
    LCD_DispStrEx(HEAD_HEIGHT, 4, _str, &tFont, 240 - HEAD_HEIGHT, ALIGN_CENTER);

    LCD_DrawLine(0, HEAD_HEIGHT, 239, HEAD_HEIGHT, HEAD_BODER_COLOR);
    
    LCD_Fill_Rect(0, HEAD_HEIGHT + 1, 240 - HEAD_HEIGHT - 1, 240, FORM_BACK_COLOR); /* 清屏正文区  */
}

/* 显示序号 */
void DispHeaderSn(uint8_t _idx)
{
    FONT_T tFont;
    char buf[48];

    /* 设置字体参数 */
    {
        tFont.FontCode = FC_ST_24;          /* 字体代码 16点阵 */
        tFont.FrontColor = HEAD_TEXT_COLOR; /* 字体颜色 */
        tFont.BackColor = HEAD_BACK_COLOR;  /* 文字背景颜色 */
        tFont.Space = 0;                    /* 文字间距，单位 = 像素 */
    }
    
    LCD_Fill_Rect(0, 0, HEAD_HEIGHT, HEAD_HEIGHT, HEAD_SN_COLOR);       /* 清屏标题序号区  */
    
    sprintf(buf, "%02d", _idx);
    tFont.BackColor = HEAD_SN_COLOR;    /* 文字背景颜色 */
    LCD_DispStr(4, 4, buf, &tFont);
}

/*
*********************************************************************************************************
*    函 数 名: DispBox
*    功能说明: 显示一个圆角矩形框，固定风格。
*    形    参: _usX : 坐标X
*              _usY : 坐标Y
*              _usHeight : 高度
*              _usWidth : 宽度
*              _usColor : 填充颜色
*    返 回 值: 无
*********************************************************************************************************
*/
void DispBox(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{    
    /* 绘制边框 */
    LCD_DrawRoundRect(_usX, _usY, _usHeight, _usWidth, B0X_ARC, MEAS_BODER_COLOR);
    
    /* 填充矩形 */
    LCD_FillRoundRect(_usX + 1, _usY + 1, _usHeight - 2, _usWidth - 2, B0X_ARC, _usColor);
}   

/*
*********************************************************************************************************
*    函 数 名: DispLabel
*    功能说明: 显示一个文本标签框
*    形    参: _usX : 坐标X
*              _usY : 坐标Y
*              _usHeight : 高度
*              _usWidth : 宽度
*              _usColor : 填充颜色
*              _pStr : 显示内容
*              _pFont : 字体
*    返 回 值: 无
*********************************************************************************************************
*/
void DispLabel(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, 
    uint16_t _usColor, char *_pStr, FONT_T *_tFont)
{    
    /* 填充矩形 */
    LCD_Fill_Rect(_usX, _usY, _usHeight, _usWidth, _usColor);    
    LCD_DispStr(_usX, _usY, _pStr, _tFont);
}  

/*
*********************************************************************************************************
*    函 数 名: DispMeasBar
*    功能说明: 显示测量值
*    形    参: _ucLine : 行号 0 - 3 (最多4行)
*              _pName : 参数名称
*              _pValue : 测量值
*              _pUnit : 单位
*    返 回 值: 无
*********************************************************************************************************
*/
void DispMeasBar(uint8_t _ucLine, char *_pName, char *_pValue, char *_pUnit)
{
    DispMeasBarEx(_ucLine, _pName, _pValue, _pUnit, MEAS_BACK_COLOR);
}

/*
*********************************************************************************************************
*    函 数 名: DispMeasBarEx
*    功能说明: 显示测量值，带填充颜色参数
*    形    参: _ucLine : 行号 0 - 3 (最多4行)
*              _pName : 参数名称
*              _pValue : 测量值
*              _pUnit : 单位
*    返 回 值: 无
*********************************************************************************************************
*/
void DispMeasBarEx(uint8_t _ucLine, char *_pName, char *_pValue, char *_pUnit, uint16_t _usFillColor)
{
    FONT_T tFont;    
    uint16_t x;
    uint16_t y;
    
    /* 设置字体参数 */
    {
        tFont.FontCode = FC_ST_24;          /* 字体代码 16点阵 */
        tFont.FrontColor = MEAS_NAME_COLOR; /* 字体颜色 */
        tFont.BackColor = CL_MASK;          /* 文字背景颜色 */
        tFont.Space = 0;                    /* 文字间距，单位 = 像素 */
    }
    
    x = MEAS_WIN_LEFT;
    y = HEAD_HEIGHT + 9 + _ucLine * (MEAS_WIN_HEIGHT + 9);
    
    DispBox(x, y, MEAS_WIN_HEIGHT, MEAS_WIN_WIDTH, _usFillColor);
    
    /* 参数名字 */
    tFont.FrontColor = MEAS_NAME_COLOR;
    LCD_DispStr(MEAS_WIN_LEFT + 5, y + 4, _pName, &tFont);

    /* 测量值 */
    tFont.FrontColor = MEAS_VALUE_COLOR;
    LCD_DispStr(MEAS_WIN_LEFT + 80, y + 4, _pValue, &tFont);

    /* 单位 */
    tFont.FrontColor = MEAS_UNIT_COLOR;
    LCD_DispStr(MEAS_WIN_LEFT + 188, y + 4, _pUnit, &tFont);    
}

/*
*********************************************************************************************************
*    函 数 名: DispParamBar
*    功能说明: 显示参数设置 - 系统设置中使用
*    形    参: _ucLine : 行号 0 - 3 (最多4行)
*              _pName : 参数名称
*              _pValue : 测量值
*              _ucActive : 选中否
*    返 回 值: 无
*********************************************************************************************************
*/
void DispParamBar(uint8_t _ucLine, char *_pName, char *_pValue, uint8_t _ucActive)
{
    FONT_T tFont;    
    uint16_t x;
    uint16_t y;
    
    /* 设置字体参数 */
    {
        tFont.FontCode = FC_ST_24;          /* 字体代码 16点阵 */
        tFont.FrontColor = MEAS_NAME_COLOR; /* 字体颜色 */
        tFont.BackColor = CL_MASK;          /* 文字背景颜色 */
        tFont.Space = 0;                    /* 文字间距，单位 = 像素 */
    }
    
    x = MEAS_WIN_LEFT;
    y = HEAD_HEIGHT + 9 + _ucLine * (MEAS_WIN_HEIGHT + 3);
    
    /* 绘制圆角矩形 */
    if (_ucActive == 1)    /* 选中是用黄色底 */
    {    
        DispBox(x, y, MEAS_WIN_HEIGHT, MEAS_WIN_WIDTH, CL_YELLOW);
    }
    else
    {
        DispBox(x, y, MEAS_WIN_HEIGHT, MEAS_WIN_WIDTH, MEAS_BACK_COLOR);
    }
    
    /* 参数名字 */
    tFont.FrontColor = MEAS_NAME_COLOR;
    LCD_DispStr(MEAS_WIN_LEFT + 5, y + 4, _pName, &tFont);

    /* 测量值 */
    tFont.FrontColor = MEAS_VALUE_COLOR;
    LCD_DispStr(MEAS_WIN_LEFT + 120, y + 4, _pValue, &tFont);    
}

/*
*********************************************************************************************************
*    函 数 名: DispProgressBar
*    功能说明: 显示进度条
*    形    参:  _usX : 坐标
*               _usX : 坐标
*               _usHeight : 高度
*               _usWidth : 宽度
*               _str : 显示文字
*               _ucPercent : 百分比
*               tFont : 字体
*    返 回 值: 无
*********************************************************************************************************
*/
extern uint8_t s_DispRefresh;
void DispProgressBar(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, 
    char *_str, uint8_t _ucPercent, FONT_T *_tFont)    
{   
    uint16_t width;
    char buf[16];
    uint16_t StrWidth;
    uint16_t StrHeight;
    uint16_t x, y;
    
    width = ((_usWidth - 4) * _ucPercent) / 100;
    
    /* 填充矩形 */
    LCD_DrawRect(_usX,          _usY,     _usHeight,     _usWidth,  PROGRESS_BODER_COLOR);
    
    LCD_Fill_Rect(_usX + 2,     _usY + 2, _usHeight - 4, width,     PROGRESS_BACK_COLOR1);
    
    if (_ucPercent < 100)
    {
        LCD_Fill_Rect(_usX + width + 2, _usY + 2, _usHeight - 4, _usWidth - width - 4, PROGRESS_BACK_COLOR2); 
    }

    StrHeight = LCD_GetFontHeight(_tFont);
    y = _usY + (_usHeight - StrHeight) / 2;
    if (_str[0] == 0)   /* 居中显示百分比文字 */
    {              
        sprintf(buf, "%02d%%", _ucPercent);
        StrWidth = LCD_GetStrWidth(buf, _tFont);
        x = _usX + (_usWidth - StrWidth) / 2;
        LCD_DispStr(x, y, buf, _tFont);  
    }
    else
    {
        x = _usX + 4;
        LCD_DispStr(x, y, _str, _tFont);  
    }     
}

/*
*********************************************************************************************************
*   函 数 名: DispHelpBar
*   功能说明: 显示操作提示
*   形    参: 无
*   返 回 值: 无
*********************************************************************************************************
*/
const unsigned char g_ImageHelp[512] = { /* 0X10,0X10,0X00,0X10,0X00,0X10,0X01,0X1B, */
    0XF7,0X9E,0XF7,0X9E,0XF7,0X9E,0XFF,0XBE,0XFF,0XBE,0XEF,0X9E,0XD7,0X5E,0XCF,0X3F,
    0XCF,0X1F,0XD7,0X5F,0XE7,0X9F,0XFF,0XDF,0XFF,0XDE,0XF7,0X9E,0XF7,0X9E,0XF7,0X9E,
    0XF7,0X9E,0XF7,0X9E,0XF7,0XBE,0XEF,0X7E,0XC6,0XFE,0XAE,0XBE,0XA6,0X7E,0X9E,0X7E,
    0X9E,0X7E,0X9E,0X7E,0XA6,0X7E,0XBE,0XDE,0XE7,0X7E,0XFF,0XBE,0XF7,0X9E,0XF7,0X9E,
    0XF7,0X9E,0XF7,0XBE,0XE7,0X5E,0XB6,0XBE,0XA6,0X7E,0X9E,0X5E,0X96,0X5E,0X8E,0X1E,
    0X8E,0X1E,0X96,0X3E,0X96,0X5E,0X9E,0X5E,0XAE,0X9E,0XDF,0X5E,0XFF,0XBE,0XF7,0X9E,
    0XF7,0XBE,0XEF,0X7E,0XB6,0XBD,0X9E,0X7E,0X96,0X3E,0X96,0X3E,0X8E,0X1E,0XA6,0X7E,
    0XA6,0X7E,0X8E,0X1D,0X8E,0X1E,0X8E,0X3D,0X96,0X3D,0XA6,0X7D,0XE7,0X7E,0XFF,0XDE,
    0XFF,0XBE,0XBE,0XDD,0X9E,0X5D,0X96,0X3D,0X8E,0X1D,0X8E,0X1D,0X85,0XFD,0XDF,0X7E,
    0XE7,0X9F,0X85,0XFD,0X86,0X1D,0X86,0X1D,0X8E,0X1D,0X8E,0X1D,0XB6,0XBD,0XFF,0XDF,
    0XE7,0X7E,0X96,0X3D,0X8E,0X1D,0X86,0X1D,0X85,0XFD,0X86,0X1D,0X7D,0XFD,0X96,0X3D,
    0X96,0X3D,0X85,0XFD,0X85,0XFD,0X85,0XFD,0X85,0XFD,0X85,0XFD,0X8E,0X1C,0XE7,0X7E,
    0XCF,0X1E,0X7D,0XFC,0X85,0XFD,0X85,0XFD,0X7D,0XFD,0X7D,0XFD,0X8E,0X3D,0X96,0X3D,
    0X8E,0X1D,0X7D,0XFD,0X7D,0XFD,0X7D,0XFD,0X7D,0XDC,0X7D,0XFC,0X75,0XDC,0XC6,0XFE,
    0XB6,0XBD,0X75,0XBC,0X7D,0XDC,0X7D,0XDC,0X7D,0XFD,0X7D,0XDD,0X8E,0X3D,0XE7,0X7F,
    0XD7,0X5E,0X75,0XDC,0X7D,0XDC,0X7D,0XDC,0X7D,0XDC,0X7D,0XDC,0X75,0X9C,0XAE,0X9D,
    0XAE,0X9D,0X75,0X9C,0X7D,0XDC,0X7D,0XDC,0X7D,0XDC,0X7D,0XDC,0X6D,0X9C,0XBE,0XDE,
    0XDF,0X5E,0X75,0XBC,0X7D,0XDC,0X7D,0XDC,0X75,0XBC,0X75,0XBC,0X6D,0X7B,0XA6,0X7D,
    0XC6,0XFD,0X75,0X9C,0X7D,0XBC,0X75,0XBC,0X75,0XBC,0X75,0XBC,0X6D,0X9C,0XBE,0XDD,
    0XD7,0X5E,0X6D,0X9C,0X75,0XBC,0X75,0XBC,0X75,0X9C,0X75,0X9C,0X65,0X7B,0XBE,0XDD,
    0XE7,0X7E,0X85,0XDC,0X7D,0XBC,0X75,0X9C,0X75,0X9B,0X75,0X9C,0X65,0X7B,0XB6,0XBD,
    0XD7,0X3E,0X65,0X7B,0X6D,0X9C,0X6D,0X7B,0X6D,0X7B,0X75,0X9B,0X75,0X9B,0XDF,0X5E,
    0XFF,0XDE,0XAE,0X7C,0X85,0XDB,0X75,0X9B,0X6D,0X7B,0X6D,0X7B,0X5D,0X3B,0XB6,0XBD,
    0XD7,0X5E,0X5D,0X3B,0X6D,0X7B,0X6D,0X7B,0X6D,0X7B,0X75,0X7B,0X9E,0X3C,0XFF,0XDF,
    0XFF,0XBE,0XE7,0X5E,0X96,0X1C,0X7D,0XBB,0X75,0X7B,0X65,0X5B,0X75,0X9B,0XCF,0X1D,
    0XDF,0X7E,0X85,0XDB,0X5D,0X3A,0X6D,0X5B,0X75,0X7B,0X85,0XDB,0XDF,0X5D,0XFF,0XDE,
    0XF7,0X9E,0XFF,0XBE,0XD7,0X3D,0X95,0XFB,0X75,0X9B,0X6D,0X7B,0X6D,0X5B,0X6D,0X5A,
    0X65,0X5A,0X6D,0X5A,0X65,0X5A,0X6D,0X5A,0X7D,0XBB,0XCE,0XFD,0XFF,0XDE,0XF7,0X9E,
    0XF7,0X9E,0XF7,0X9E,0XFF,0XBE,0XDF,0X3D,0X9E,0X1B,0X6D,0X7A,0X5D,0X1A,0X4C,0XFA,
    0X4C,0XDA,0X4C,0XFA,0X5D,0X3A,0X8D,0XFB,0XDF,0X3D,0XFF,0XBE,0XF7,0X9E,0XF7,0X9E,
    0XF7,0X9E,0XF7,0X9E,0XF7,0X9E,0XF7,0X9E,0XEF,0X5D,0XCE,0XBB,0XA6,0X1A,0X85,0XBA,
    0X85,0XBA,0X9E,0X1A,0XC6,0XBC,0XEF,0X7D,0XF7,0XBE,0XF7,0X9E,0XF7,0X9E,0XF7,0X9E,
};
void DispHelpBar(char *_str1, char *_str2)
{
    FONT_T tFont;   /* 定义字体结构体变量 */

    tFont.FontCode = FC_ST_16;              /* 字体代码 16点阵 */
    tFont.FrontColor = HELP_TEXT_COLOR;     /* 字体颜色 */
    tFont.BackColor = HELP_BACK_COLOR;      /* 文字背景颜色 */
    tFont.Space = 0;                        /* 文字间距，单位 = 像素 */

    ST7789_DrawBMP8(1, 240 - 36, 16, 16, (uint8_t *)g_ImageHelp);
    
    LCD_DispStr(20, 240 - 36, _str1, &tFont);
    LCD_DispStr(20, 240 - 18, _str2, &tFont);  
}

/*
*********************************************************************************************************
*    函 数 名: DispInfoBar16
*    功能说明: 显示信息，16点阵.
*    形    参: _line   : 行号
*              _str1 : 参数名
*              _str2 : 参数值
*    返 回 值: 无
*********************************************************************************************************
*/
void DispInfoBar16(uint8_t _ucLine, char *_pName, char *_pValue)
{
    DispInfoBar16Ex(_ucLine, _pName, _pValue, INFO_VALUE_COLOR);
}

/* 带颜色参数 - 用于显示红色超标参数 */
void DispInfoBar16Ex(uint8_t _ucLine, char *_pName, char *_pValue, uint16_t _ucColor)
{
    FONT_T tFont;    
    uint16_t x;
    uint16_t y;
    
    /* 设置字体参数 */
    {
        tFont.FontCode = FC_ST_16;          /* 字体代码 16点阵 */
        tFont.FrontColor = INFO_NAME_COLOR; /* 字体颜色 */
        tFont.BackColor = INFO_BACK_COLOR;  /* 文字背景颜色 */
        tFont.Space = 0;                    /* 文字间距，单位 = 像素 */
    }
    
    x = 5;
    y = HEAD_HEIGHT + 5 + _ucLine * INFO_HEIGHT;
    
//    /* 绘制边框 */
//    LCD_DrawRoundRect(x, y, MEAS_WIN_HEIGHT, MEAS_WIN_WIDTH, MEAS_WIN_HEIGHT / 4, MEAS_BODER_COLOR);
//    
//    /* 填充矩形 */
//    LCD_FillRoundRect(x + 1, y + 1, MEAS_WIN_HEIGHT - 2, MEAS_WIN_WIDTH - 2, MEAS_WIN_HEIGHT / 4, CL_YELLOW);
    
    /* 参数名字 */
    tFont.FrontColor = INFO_NAME_COLOR;
    LCD_DispStr(x, y, _pName, &tFont);

    x += LCD_GetStrWidth(_pName, &tFont) + 5;
    
    /* 参数值 */
    tFont.FrontColor = _ucColor;
    LCD_DispStr(x, y, _pValue, &tFont);  
}

/*
*********************************************************************************************************
*    函 数 名: DSO_StartMode2
*    功能说明: 启动示波器欧式2，多通道低速扫描模式
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void DSO_StartMode2(void)
{
    WriteRegValue_06H(0x01FF, 2);    /* 多通道低速测量 */
    WriteRegValue_06H(0x0200, 1);    /* CH1选DC耦合 */
    WriteRegValue_06H(0x0201, 1);    /* CH2选DC耦合 */
    WriteRegValue_06H(0x0202, 0);    /* CH1通道增益0档，不放大 */
    WriteRegValue_06H(0x0203, 0);    /* CH2通道增益0档，不放大 */
    WriteRegValue_06H(0x0204, 0);    /* CH1通道直流偏值，未用 */
    WriteRegValue_06H(0x0205, 0);    /* CH2通道直流偏值，未用 */
    WriteRegValue_06H(0x0206, 12);   /* 采样频率1M */
    WriteRegValue_06H(0x0207, 0);    /* 采样深度1K */
    WriteRegValue_06H(0x0208, 0);    /* 触发电平 */
    WriteRegValue_06H(0x0209, 50);   /* 触发位置 */
    WriteRegValue_06H(0x020A, 0);    /* 触发模式 0=自动 */
    WriteRegValue_06H(0x020B, 0);    /* 触发通道CH1 */
    WriteRegValue_06H(0x020C, 0);    /* 触发边沿 */
    WriteRegValue_06H(0x020D, 2);    /* 通道使能 */
    WriteRegValue_06H(0x020E, 1);    /* 开始采集 */
}

/*
*********************************************************************************************************
*    函 数 名: PlayKeyTone
*    功能说明: 播放按键音。通过参数可以关闭
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void PlayKeyTone(void)
{
    if (g_tParam.KeyToneEnable != 0)
    {
        BEEP_KeyTone();
    }
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
