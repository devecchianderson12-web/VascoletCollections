#pragma once

namespace vc {

// Pantalla inicial de la colección: elegir Vascolet 1 o Vascolet 2 y las opciones
// (gráficos, controles y sonido, las mismas que tiene el XA).
void runLauncher();
// Abre las opciones (gráficos, controles, sonido) y vuelve cuando el jugador sale de ellas.
void runOptionsMenu();

constexpr int BASE_W = 512, BASE_H = 384, WIDE_W = 684;

}
