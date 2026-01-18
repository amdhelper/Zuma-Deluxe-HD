#pragma once

#include "../Scene.h"

extern HScene SC_GAME;
extern HScene SC_TEST;
extern HScene SC_MENU;

HScene Scene_Register_Game(); 
HScene Scene_Register_Test();
HScene Scene_Register_Menu();

void Scene_RegisterAll();
