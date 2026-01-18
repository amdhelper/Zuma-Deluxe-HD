#pragma once

#include "../Scene.h"

extern HScene SC_GAME;
extern HScene SC_TEST;

HScene Scene_Register_Game(); 
HScene Scene_Register_Test();

void Scene_RegisterAll();
