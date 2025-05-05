#include "PainterEngine.h"
#include <sys/time.h>

#define PX_OBJECT_TYPE_TANK 250503
#define POS(x, y) (x),(y)

typedef struct _PX_Object_Tank PX_Object_Tank;

typedef struct {
    PX_Object_Tank *tank; //所属坦克对象
	px_short direction; //方向（360°，0为正北朝上）
	px_byte speed; //子弹移动速度
    px_byte is_alive; //是否存活有效
} Bullet;

typedef struct {
    px_float x;
    px_float y;
} Position;

#define SetPosition(pos,_x,_y) \
do { \
    (pos).x = (_x); \
    (pos).y = (_y); \
} while(0)

typedef struct _PX_Object_Tank {
    Position pos; //（坦克中心）位置
	px_short direction; //方向（360°，0为正北朝上）
	px_byte speed; //坦克移动速度
    px_color color; //（车身）颜色（纯色）
    px_short hits; //生命值
	px_texture render_texture; //（车身）渲染纹理
} PX_Object_Tank;

PX_OBJECT_RENDER_FUNCTION(PX_Object_TankRender) //坦克渲染
{
	PX_Object_Tank* tank = PX_ObjectGetDescByType(pObject, PX_OBJECT_TYPE_TANK);
	px_float x, y, width, height;
	PX_OBJECT_INHERIT_CODE(pObject, x, y, width, height);
	PainterEngine_DrawLine(POS(tank->pos.x,tank->pos.y), POS(tank->pos.x+10,tank->pos.y+10), 1, tank->color);
	
}

PX_OBJECT_FREE_FUNCTION(PX_Object_TankFree)
{
	PX_Object_Tank* tank = PX_ObjectGetDescByType(pObject, PX_OBJECT_TYPE_TANK);
	//...
}

PX_OBJECT_EVENT_FUNCTION(PX_Object_TankOnMove)
{
	pObject->x=PX_Object_Event_GetCursorX(e);//锤子跟随鼠标移动
	pObject->y=PX_Object_Event_GetCursorY(e);
}

PX_Object *PX_Object_TankCreate(px_memorypool *mp,PX_Object *parent)
{
	PX_Object_Tank* ptank;
	PX_Object* pObject = PX_ObjectCreateEx(mp, parent, 0, 0, 0, 0, 0, 0, 
        PX_OBJECT_TYPE_TANK, 0, PX_Object_TankRender, PX_Object_TankFree, 0, sizeof(PX_Object_Tank));
	ptank=PX_ObjectGetDescByType(pObject,PX_OBJECT_TYPE_TANK);
	ptank->color = PX_COLOR(255, 255, 0, 0);
    SetPosition(ptank->pos, 200,200);
	// pfox->ptexture_mask = PX_ResourceLibraryGetTexture(PainterEngine_GetResourceLibrary(), "fox_mask");//遮罩
	// if(!PX_TextureCreate(mp,&pfox->render_target,ptexture->width,ptexture->height))
	// {
	// 	PX_ObjectDelete(pObject);
	// 	return 0;
	// }
	PX_ObjectRegisterEvent(pObject, PX_OBJECT_EVENT_CURSORDRAG, PX_Object_TankOnMove, PX_NULL); //注册拖拽事件
	return pObject;
}

int main()
{
    PainterEngine_Initialize(800,580);
	PX_Object_TankCreate(mp, root);
	return 0;
}