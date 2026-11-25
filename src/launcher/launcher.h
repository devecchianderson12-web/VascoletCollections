#pragma once
#include <string>

namespace vc {

// Pantalla inicial de la colección: elegir Vascolet 1 o Vascolet 2 y las opciones
// (gráficos, controles y sonido, las mismas que tiene el XA).
void runLauncher();
// Abre las opciones (gráficos, controles, sonido) y vuelve cuando el jugador sale de ellas.
void runOptionsMenu();
// Arte del inicio (menu.jpg a todo el alto con rayos animados) y botón rojo, en la resolución actual.
void drawTitleArt(float buttonColumnW);
void titleButton(const std::string& label, float cx, float cy, float w, float h, bool hot, bool pressed = false);

constexpr int BASE_W = 512, BASE_H = 384, WIDE_W = 684;

}
