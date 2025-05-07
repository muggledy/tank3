#include "PainterEngine.h"

#define POS(x, y) (x),(y)
#define SIZE(width,height) (width),(height)

/*https://www.zhihu.com/question/481418891/answer/2163031711
https://zhuanlan.zhihu.com/p/72367056
Doc: https://github.com/matrixcascade/PainterEngine/blob/master/documents/PainterEngine%20the%20book.md*/

PX_OBJECT_EVENT_FUNCTION(OnButtonClick)
{
    PX_Object_PushButtonSetText(pObject,"我被点击了");
}

int main()
{
    PainterEngine_Initialize(800,580);

    PainterEngine_LoadFontModule("assets/Microsoft_JhengHei.ttf", PX_FONTMODULE_CODEPAGE_UTF8, 24); //支持中文
    PainterEngine_DrawText(430, 150, "你好PainterEngine", PX_ALIGN_CENTER, PX_COLOR(255, 255, 0, 0));

    px_int linewidth = 5;
    px_color color1 = PX_COLOR(255, 255, 0, 0); // 红色
    // 绘制线段（前两个参数是起点和终点坐标）
    PainterEngine_DrawLine(POS(50,50), POS(200,200), linewidth, color1);

    px_color color2 = PX_COLOR(255, 0, 255,0 ); // 绿色
    // 绘制矩形（第一个参数是左上角坐标）
    PainterEngine_DrawRect(POS(100,10), SIZE(150,100), color2);

    px_int radius = 50;
    px_color color3 = PX_COLOR(255, 0, 0, 255); // 蓝色
    // 绘制圆形（第一个参数是圆心坐标）
    PainterEngine_DrawCircle(POS(100,100), radius, linewidth, color3);
    // 绘制实心圆
    PainterEngine_DrawSolidCircle(30, 200, 20, PX_COLOR(255, 255, 0, 255));

    px_int inside_radius = 50; // 设置内外扇形的半径
    px_int outside_radius = 100;
    px_int start_angle = 0; // 设置扇形的起始角度和结束角度
    px_int end_angle = 135;
    // 绘制扇形
    PainterEngine_DrawSector(POS(250,100), inside_radius, outside_radius, start_angle, end_angle, color1);

    px_texture mytexture; //（图片）纹理
    if(!PX_LoadTextureFromFile(mp_static,&mytexture,"assets/demo.png")) {
        // 加载纹理失败
		return 0;
	}
    // 绘制图片
    PainterEngine_DrawTexture(&mytexture, POS(0,80), PX_ALIGN_LEFTTOP);

    PX_Object* myButtonObject;
    // 创建按钮（第二个参数 root 是 PainterEngine 的根对象，意思是创建一个按钮对象作为根对象的子对象，这样按钮就能链接到系统对象树中, 进行事件响应和渲染）
    myButtonObject=PX_Object_PushButtonCreate(mp,root,POS(300,200),SIZE(140,40),"我是一个按钮", PainterEngine_GetFontModule());
    PX_ObjectRegisterEvent(myButtonObject,PX_OBJECT_EVENT_EXECUTE,OnButtonClick,0); // 注册按钮（点击）事件callback
    return 1;
}
