#pragma once

/** Registra rutas HTTP y arranca servidor en puerto 80. Llamar tras wifi_manager_begin(). */
void kiln_api_begin();

/** Atender peticiones (llamar periódicamente desde tarea o loop). */
void kiln_api_loop();

/** Tarea dedicada HTTP (prioridad alta, ~100 Hz). */
void kiln_api_startTask();
