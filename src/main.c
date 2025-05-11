#include "PainterEngine.h"
#include <sys/time.h>
#include "tank.h"

#define PX_OBJECT_TYPE_TANK 250503

#define SET_FLAG(obj, field, flag) (obj)->field |= flag
#define CLR_FLAG(obj, field, flag) (obj)->field &= (~flag)
#define TST_FLAG(obj, field, flag) ((obj)->field & flag)

typedef struct _PX_Object_Tank PX_Object_Tank;

typedef struct {
    PX_Object_Tank *tank; //所属坦克对象
	px_short direction; //方向（360°，0为正北朝上）
	px_byte speed; //子弹移动速度
    px_byte is_alive; //是否存活有效
} Bullet;

typedef struct _PX_Object_Tank {
    Point pos; //（坦克中心）位置(整数,因像素坐标只能是整数,仅用于UI显示)
	Point accurate_pos; //精确位置(浮点数,用于所有运算逻辑)
	px_short direction; //方向（360°，0为正北朝上）
	px_byte speed; //坦克移动速度
    px_byte color_id; //颜色
    px_short hits; //生命值
#define KEY_FLAG_W 0x00000001
#define KEY_FLAG_A 0x00000002
#define KEY_FLAG_S 0x00000004
#define KEY_FLAG_D 0x00000008
	px_uint32 key_flag;
} PX_Object_Tank;

px_color white_bg_color = {255,255,255,255};

PX_OBJECT_RENDER_FUNCTION(PX_Object_TankRender) //坦克渲染
{
	PX_Object_Tank* tank = PX_ObjectGetDescByType(pObject, PX_OBJECT_TYPE_TANK);
	px_float x, y, width, height;
	px_surface* surface = PX_Object_PanelGetSurface(App.object_printer);

	PX_SurfaceClearAll(surface, white_bg_color); //清空画布
	PX_OBJECT_INHERIT_CODE(pObject, x, y, width, height);
	draw_tank(tank->pos, tank->direction, tank->color_id);
}

PX_OBJECT_FREE_FUNCTION(PX_Object_TankFree)
{
	// PX_Object_Tank* tank = PX_ObjectGetDescByType(pObject, PX_OBJECT_TYPE_TANK);
}

PX_OBJECT_EVENT_FUNCTION(PX_Object_TankOnKeydown)
{
	Point tmp_pos;
	px_short new_dir = 0;
	PX_Object_Tank* tank = PX_ObjectGetDescByType(pObject, PX_OBJECT_TYPE_TANK);
	px_uint key = 0;

	if (e.Event == PX_OBJECT_EVENT_KEYDOWN) {
		key = PX_Object_Event_GetKeyDown(e);
		if ((key == 'd') || TST_FLAG(tank, key_flag, KEY_FLAG_D)) {
			SET_FLAG(tank, key_flag, KEY_FLAG_D);
			tank->direction += 10;
			tank->direction %= 360;
			printf("keydown:D. direction:%u\n", tank->direction);
		}
		if ((key == 'a') || TST_FLAG(tank, key_flag, KEY_FLAG_A)) {
			SET_FLAG(tank, key_flag, KEY_FLAG_A);
			tank->direction += 360;
			tank->direction -= 10;
			tank->direction %= 360;
			printf("keydown:A. direction:%u\n", tank->direction);
		}
		if ((key == 'w') || TST_FLAG(tank, key_flag, KEY_FLAG_W)) {
			SET_FLAG(tank, key_flag, KEY_FLAG_W);
			tmp_pos = tank->accurate_pos;
			tank->accurate_pos = move_point(tank->accurate_pos, tank->direction, tank->speed);
			tank->pos = round_point(tank->accurate_pos);
			printf("keydown:W. forward from (%f, %f) to (%f, %f)\n", POINT2POS(tmp_pos), POINT2POS(tank->accurate_pos));
		}
		if ((key == 's') || TST_FLAG(tank, key_flag, KEY_FLAG_S)) {
			SET_FLAG(tank, key_flag, KEY_FLAG_S);
			new_dir = tank->direction + 180;
			if (new_dir > 360) {
				new_dir -= 360;
			}
			tmp_pos = tank->accurate_pos;
			tank->accurate_pos = move_point(tank->accurate_pos, new_dir, tank->speed);
			tank->pos = round_point(tank->accurate_pos);
			printf("keydown:S. forward from (%f, %f) to (%f, %f)\n", POINT2POS(tmp_pos), POINT2POS(tank->accurate_pos));
		}
	}
}

PX_OBJECT_EVENT_FUNCTION(PX_Object_TankOnKeyup)
{
	PX_Object_Tank* tank = PX_ObjectGetDescByType(pObject, PX_OBJECT_TYPE_TANK);
	px_uint key = 0;
	printf("keyup...\n");
	if (e.Event == PX_OBJECT_EVENT_KEYUP) {
		key = PX_Object_Event_GetKeyDown(e);
		if (key == 'd') {
			CLR_FLAG(tank, key_flag, KEY_FLAG_D);
			printf("keyup:D\n");
		}
		if (key == 'a') {
			CLR_FLAG(tank, key_flag, KEY_FLAG_A);
			printf("keyup:A\n");
		}
		if (key == 'w') {
			CLR_FLAG(tank, key_flag, KEY_FLAG_W);
			printf("keyup:W\n");
		}
		if (key == 's') {
			CLR_FLAG(tank, key_flag, KEY_FLAG_S);
			printf("keyup:S\n");
		}
	}
}

PX_Object *PX_Object_TankCreate(px_memorypool *mp,PX_Object *parent)
{
	PX_Object_Tank* ptank;
	PX_Object* pObject = PX_ObjectCreateEx(mp, parent, 0, 0, 0, 0,0, 0, 
        PX_OBJECT_TYPE_TANK, 0, PX_Object_TankRender, PX_Object_TankFree, 0, sizeof(PX_Object_Tank));
	ptank = PX_ObjectGetDescByType(pObject, PX_OBJECT_TYPE_TANK);
	ptank->key_flag = 0;
	ptank->direction = 0;
	ptank->speed = 2.5;
	ptank->color_id = PALETTE_RED;
    SetPosition(ptank->pos, 200,200);
	SetPosition(ptank->accurate_pos, 200,200);
	//PX_ObjectRegisterEvent(pObject, PX_OBJECT_EVENT_KEYDOWN, PX_Object_TankOnKeydown, PX_NULL);
	PX_ObjectRegisterEvent(pObject, PX_OBJECT_EVENT_KEYUP, PX_Object_TankOnKeyup, PX_NULL);
	return pObject;
}

int main()
{
	PainterEngine_Initialize(800,580);
	PX_Object_TankCreate(mp, root);
	//sleep(10);
	return 0;
}
