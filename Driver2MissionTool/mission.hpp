#pragma once

#include "core/dktypes.h"
#include "math/psx_math_types.h"

#define MISSION_IDENT		(('D' << 24) | ('2' << 16) | ('M' << 8) | 'S' )

int TargetFlags[19]
{
	0x1,
	0x800,

	0x2,
	0x100,

	0x200,
	0x400,

	0x10000,
	0x20000,
	0x30000,

	0x1000000,
	0x2000000,
	0x3000000,

	0x100000,
	0x200000,

	0x10,
	0x20,
	0x40,
	0x40000000,

	0x1000,	
};

enum TargetFlags
{
	// common target properties
	TARGET_FLAG_ACTIVE_P1 = 0x1,
	TARGET_FLAG_ACTIVE_P2 = 0x800,
	TARGET_FLAG_ACTIVE_ALLP = (TARGET_FLAG_ACTIVE_P1 | TARGET_FLAG_ACTIVE_P2),

	TARGET_FLAG_COMPLETED_P1 = 0x2,
	TARGET_FLAG_COMPLETED_P2 = 0x100,
	TARGET_FLAG_COMPLETED_ALLP = (TARGET_FLAG_COMPLETED_P1 | TARGET_FLAG_COMPLETED_P2),

	TARGET_FLAG_VISIBLE_P1 = 0x200,
	TARGET_FLAG_VISIBLE_P2 = 0x400,
	TARGET_FLAG_VISIBLE_ALLP = (TARGET_FLAG_VISIBLE_P1 | TARGET_FLAG_VISIBLE_P2),

	// point target properties
	TARGET_FLAG_POINT_CTF_BASE_P1 = 0x10000,
	TARGET_FLAG_POINT_CTF_BASE_P2 = 0x20000,
	TARGET_FLAG_POINT_CTF_FLAG = 0x30000,

	TARGET_FLAG_POINT_SECRET_POINT1 = 0x1000000,
	TARGET_FLAG_POINT_SECRET_POINT2 = 0x2000000,
	TARGET_FLAG_POINT_SECRET_STARTFINISH = 0x3000000,

	TARGET_FLAG_POINT_ON_BOAT = 0x100000,
	TARGET_FLAG_POINT_STOP_COPS_TRIGGER = 0x200000,

	// car target properties
	TARGET_FLAG_CAR_SAVED = 0x10,
	TARGET_FLAG_CAR_PLAYERCONTROLLED = 0x20,
	TARGET_FLAG_CAR_SWAPPED = 0x40,
	TARGET_FLAG_CAR_PINGED_IN = 0x40000000,

	// event target properties
	TARGET_FLAG_EVENT_TRIGGERED = 0x1000,
};

enum TARGET_TYPE
{
	Target_Point = 1,
	Target_Car = 2,				// any mission target car. Also, a multiplayer player 2 car in Cops'n'Robbers
	Target_Event = 3,
	Target_Player2Start = 4,		// player 2 start
	Target_MultiCar = 5
};

struct StringsStack
{
	uint offset = 0;
	ushort size = 0;
	char* strings[64];

	uint addString(char* str) {
		strings[size] = str;
		size++;
		offset += strlen(str) + 1;
		return (offset - strlen(str) - 1);
	}

	char* getString(ushort pos) {
		return strings[pos];
	}

	ushort getSize() {
		return size;
	}
};

int MSTYPE_ARR[6]
{
	0,
	1,
	2,
	4,
	0x10,
	0x20
};

int TimerFlags_Arr[6]
{
	0x1,
	0x2,
	0x4,
	0x8,
	0x10,
	0x20,
};

enum TimerFlags
{
	TIMER_FLAG_ACTIVE = 0x1,
	TIMER_FLAG_COUNTER = 0x2,
	TIMER_FLAG_PAUSED = 0x4,

	TIMER_FLAG_COMPLETE_ON_OUT = 0x8,
	TIMER_FLAG_BOMB_COUNTDOWN = 0x10,
	TIMER_FLAG_BOMB_TRIGGERED = 0x20,
};

enum MSTYPE
{
	MSTYPE_StartOnCar = 0,
	MSTYPE_StartDataRequired = 1,
	MSTYPE_StartOnFoot = 2,
	MSTYPE_Countdown = 4, // Countdown before start
	MSTYPE_FelonyBarNotActive = 0x10,
	MSTYPE_FelonyBarNoFill = 0x20,
};

typedef struct _COP_DATA
{
	int speed;
	int power;
	int min;
	int max;
} MS_COP_DATA;

struct MULTICAR_DATA
{
	int x;
	int z;
	ubyte palette;
	ubyte model;
	short rot;
};

typedef struct {		/* long word type 3D vector */
	long	vx, vy;
	long	vz, pad;
} VECTOR;

typedef struct _MISSION
{
	int id; // SM2D 4 bytes
	int size; // 1 byte
	int city; // 1 byte
	int time; // 1 byte
	int weather; // 1 byte
	XYPAIR playerStartPosition;
	int playerStartRotation;
	int type;
	short timer;
	short timerFlags;
	int strings;
	MS_COP_DATA cops;
	int msgCarWrecked;
	int msgOutOfTime;
	int msgComplete;
	int region;
	int route;
	int msgDrowned;
	int playerCarModel;
	int playerCarColour;
	int maxDamage;
	int residentModels[5];
	int nCutscenes;
	int title;
	int msgPoliceCar;
	int msgDoorsLocked;
} MS_MISSION;

typedef struct _TARGET
{
	int type;			// data 0

	union
	{
		MULTICAR_DATA multiCar[5];

		struct
		{
			int target_flags;	// data 1
			int display_flags;	// data 2

			union
			{
				struct
				{
					int posX;				// data 3
					int posZ;				// data 4
					int radius;				// data 5
					int posY;				// data 6
					int height;				// data 7
					int loseTailMessage;	// data 8
					int actionFlag;			// data 9
					int boatOffsetX;		// data 10
					int boatOffsetZ;		// data 11
				} point;

				struct
				{
					int posX;			// data 3
					int posZ;			// data 4
					int rotation;		// data 5
					int slot;			// data 6
					int model;			// data 7
					int palette;		// data 8
					int type;			// data 9
					int flags;			// data 10
					int cutscene;		// data 11
					int maxDistance;	// data 12
					union
					{
						struct
						{
							int closeMessages;		// data 13
							int farMessages;		// data 14
						} tail;
						struct
						{
							int maxDamage;			// data 13
							int tooFarMessage;		// data 14
							int gettingFarMessage;	// data 15
						} chasing;
					};
				} car;

				struct
				{
					int eventId;			// data 3
					union
					{
						VECTOR* eventPos;	// data 4
						int unused[10];
					};
					int loseMessage;		// data 14
				} event;
			};
		};
	};
} MS_TARGET;