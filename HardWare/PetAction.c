#include "stm32f10x.h"
#include "PetAction.h"
#include "Servo.h"
#include "Variable.h"

typedef struct
{
	uint8_t angle[SERVO_COUNT];
	uint16_t duration_ms;
} ActionFrame;

static const ActionFrame pose_sleep[] = {{{20,20,160,160,90}, 500}};
static const ActionFrame pose_sit[] = {{{90,90,20,20,90}, 400}};
static const ActionFrame pose_stand[] = {{{90,90,90,90,90}, 400}};
static const ActionFrame pose_down[] = {{{20,20,20,20,90}, 400}};
static const ActionFrame move_forward[] = {
	{{90,45,45,90,90},200}, {{135,45,45,135,90},200}, {{135,90,90,135,90},200}, {{90,90,90,90,90},200},
	{{45,90,90,45,90},200}, {{45,135,135,45,90},200}, {{90,135,135,90,90},200}, {{90,90,90,90,90},200}
};
static const ActionFrame move_back[] = {
	{{90,135,135,90,90},200}, {{45,135,135,45,90},200}, {{45,90,90,45,90},200}, {{90,90,90,90,90},200},
	{{135,90,90,135,90},200}, {{135,45,45,135,90},200}, {{90,45,45,90,90},200}, {{90,90,90,90,90},200}
};
static const ActionFrame turn_left[] = {{{90,45,135,90,90},200},{{45,45,135,135,90},200},{{45,90,90,135,90},200},{{90,90,90,90,90},200}};
static const ActionFrame turn_right[] = {{{45,90,90,135,90},200},{{45,45,135,135,90},200},{{90,45,135,90,90},200},{{90,90,90,90,90},200}};
static const ActionFrame swing[] = {{{30,30,30,30,50},500},{{150,150,150,150,130},900},{{30,30,30,30,50},900},{{90,90,90,90,90},500}};
static const ActionFrame jump_forward[] = {{{140,90,90,35,90},250},{{140,140,35,35,90},280},{{90,90,90,90,90},400}};
static const ActionFrame jump_back[] = {{{140,90,90,35,90},250},{{140,140,35,35,90},280},{{90,90,90,90,90},400}};
static const ActionFrame hello[] = {{{90,20,20,45,110},350},{{90,55,20,45,140},300},{{90,10,20,45,70},300},{{90,55,20,45,140},300},{{90,90,90,90,90},450}};
static const ActionFrame stretch[] = {{{10,10,90,90,90},700},{{90,90,170,170,90},700},{{90,90,90,90,90},500}};
static const ActionFrame leg_stretch[] = {{{90,20,170,110,90},700},{{90,90,90,90,90},400},{{20,90,110,170,90},700},{{90,90,90,90,90},500}};

static const ActionFrame *frames;
static uint8_t frame_count;
static uint8_t frame_index;
static uint16_t frame_elapsed;
static uint8_t active_action;
static uint8_t repeat_left;
static uint8_t sustained_action;

static uint16_t FrameDuration(void)
{
	if (active_action >= 4 && active_action <= 7) 
		return SpeedDelay;
	return frames[frame_index].duration_ms;
}

static void SelectAction(uint8_t action)
{
	frames = pose_stand;
	frame_count = 1; 
	repeat_left = 1;
	switch (action)
	{
		case 0: frames = pose_sleep; frame_count = 1; break;
		case 1: frames = pose_sit; frame_count = 1; break;
		case 2: frames = pose_stand; frame_count = 1; break;
		case 3: frames = pose_down; frame_count = 1; break;
		case 4: frames = move_forward; frame_count = 8; repeat_left = Chongfunumber; break;
		case 5: frames = move_back; frame_count = 8; repeat_left = Chongfunumber; break;
		case 6: frames = turn_left; frame_count = 4; repeat_left = Chongfunumber * 2; break;
		case 7: frames = turn_right; frame_count = 4; repeat_left = Chongfunumber * 2; break;
		case 8: frames = swing; frame_count = 4; repeat_left = SwingRepeatnumber; break;
		case 10: frames = jump_forward; frame_count = 3; break;
		case 11: frames = jump_back; frame_count = 3; break;
		case 12: frames = pose_stand; frame_count = 1; break;
		case 13: frames = hello; frame_count = 5; repeat_left = HelloRepeatnumber; break;
		case 14: frames = stretch; frame_count = 3; break;
		case 15: frames = leg_stretch; frame_count = 4; break;
	}
}

void PetAction_Init(void)
{
	active_action = 0xFF;
	PetAction_Request(2, 0);
}

void PetAction_Request(uint8_t action, uint8_t sustained)
{
	if (action > 15 || action == 9) return;
	Action_Mode = action;
	Sustainedmove = sustained;
	active_action = action;
	sustained_action = sustained;
	frame_index = 0;
	frame_elapsed = 0;
	SelectAction(action);
	Servo_SetTargets(frames[0].angle, FrameDuration());
}

void PetAction_Stop(void)
{
	PetAction_Request(2, 0);
}

uint8_t PetAction_IsBusy(void)
{
	return active_action != 0xFF && active_action != 2;
}

void PetAction_Task20ms(void)
{
	uint16_t duration;
	Servo_Task20ms();
	if (active_action == 0xFF) return;
	duration = FrameDuration();
	frame_elapsed += 20;
	if (frame_elapsed < duration) return;
	frame_elapsed = 0;
	frame_index++;
	if (frame_index >= frame_count)
	{
		if (sustained_action || repeat_left > 1)
		{
			if (!sustained_action) repeat_left--;
			frame_index = 0;
		}
		else
		{
			if (active_action <= 3) { active_action = 0xFF; return; }
			PetAction_Request(2, 0);
			return;
		}
	}
	Servo_SetTargets(frames[frame_index].angle, FrameDuration());
}

void PetAction_Perform(void)
{
	if (Action_Mode != active_action && Action_Mode != 9) PetAction_Request((uint8_t)Action_Mode, (uint8_t)Sustainedmove);
}
