# Reproductor V2

`make` compila para ARM. `./player 44100 canciones/*.wav` reproduce la lista
continuamente y vuelve a la primera pista al terminar. Ctrl+C sale del programa.

- KEY0: play/pausa, KEY1: stop (reinicia la pista al volver a play).
- KEY2: anterior, KEY3: siguiente. SW1:SW0 seleccionan bypass, pasa-bajos,
  pasa-altos y eco. LEDR0/1/2 indican play/pausa/stop y LEDR5:4 el filtro.
- `PLAYER_ADC=1 ./player 44100 canciones/*.wav` habilita el ADC_IN0 (0-30%).
  No habilitar con la entrada flotante. Sin esta opcion, volumen fijo 30%.
- `PLAYER_VGA=1 ./player 44100 canciones/*.wav` escribe metadatos y estado en VGA.
- `./player --info archivo.wav` y `./player --probe` siguen disponibles.

Pendiente: sustituir el pasa-bajos provisional por el FIR de hardware y usar
interrupciones reales para los botones. Esta version usa captura de flancos
por polling. VGA y ADC requieren pruebas fisicas. No requiere recompilar Quartus.
