struct MouseMovement {
	int relx, rely;
	int absx, absy;
};

void IN_Init();
void IN_Frame();
MouseMovement IN_GetMouseMovement();
