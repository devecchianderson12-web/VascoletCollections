#pragma once

// Vascolet 1: La Máquina del Tiempo.
namespace v1 {

extern int LOGICAL_W;   // ancho visible (depende de la pantalla)
extern int LOGICAL_H;
extern int UI_W;      // ancho de la interfaz sin deformar (pantalla completa)   // alto visible: 900 (vista del original, 1200x900)

// Corre el juego hasta que el jugador vuelve al menú de la colección.
void run();

}
