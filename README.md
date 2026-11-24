# Vascolet Collection

Vascolet 1 (La Máquina del Tiempo) y Vascolet 2 (La Fórmula Secreta) en una sola app para Android,
con las opciones del XA: gráficos (HD, efectos avanzados, cámara fija, 120 FPS, diseño clásico/moderno),
controles táctiles (joystick, estilo, tamaño, íconos, opacidad, mapeo) y sonido.

- `src/platform.*`: capa de plataforma (SDL2), la misma del XA.
- `src/launcher/`: menú de inicio y opciones.
- `games/v1/assets`, `games/v2/assets`: archivos originales de cada juego (el repo tiene que ser privado).
- Cada push a `main` compila el APK y lo publica en el release `latest`.

Estado: etapa 1 (menú y opciones). Los juegos se están portando (etapa 2: Vascolet 1, etapa 3: Vascolet 2).
