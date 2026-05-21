# Migración SmartKiln: control local por WiFi AP (sin BLE)

## Objetivo

- Horno **usable sin internet** (taller sin router).
- **Comandos solo** cuando el móvil está conectado al **AP del ESP** (usuario cerca).
- Firebase / STA opcionales después (telemetría remota).

## Regla de producto

| Acción | Canal |
|--------|--------|
| START / PAUSE / STOP, cargar programa, config horno | Solo AP → `http://192.168.4.1` |
| Ver temperatura en vivo (cerca) | AP (API local) |
| Ver desde lejos (futuro) | Firebase solo lectura (opcional) |

## API HTTP v1 (borrador)

Base: `http://192.168.4.1`

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/api/status` | `temperature`, `status`, `programName`, `stage`, `targetTemp`, … |
| POST | `/api/command` | Body JSON: `{"cmd":"START"}` \| PAUSE \| STOP \| STATUS |
| POST | `/api/profile` | Body: mismo JSON que `LOAD_PROFILE` por BLE |
| GET | `/api/info` | Nombre horno, modelo, volumen, mac |
| POST | `/api/kiln-info` | Config horno + userId (equivalente KILN_INFO) |
| POST | `/api/wifi-sta` | Credenciales router (opcional, para STA futuro) |

### Ejemplo `GET /api/status`

```json
{
  "temperature": 24.5,
  "status": "IDLE",
  "programName": "Cono 6",
  "stage": 0,
  "numStages": 3
}
```

## QR (primera vinculación)

Contenido sugerido (JSON o URL):

```json
{
  "ssid": "SmartKiln-A1B2",
  "password": "xxxxxxxx",
  "url": "http://192.168.4.1"
}
```

O formato WiFi estándar: `WIFI:T:WPA;S:SmartKiln-A1B2;P:xxxxxxxx;;`

## Fases

| Fase | Firmware | App | Prueba |
|------|----------|-----|--------|
| 0 | Git baseline, este doc | Git baseline | — |
| 1 | AP + WebServer + `/api/status` + `/api/command` | — | Navegador en WiFi del horno |
| 2 | `/api/profile`, `/api/kiln-info` | QR + conectar AP + HTTP | Play/Stop |
| 3 | Quitar NimBLE | Quitar BleManager, HybridConnectionManager | Taller sin internet |
| 4 | STA opcional + mDNS | Firebase solo lectura (opcional) | Casa con router |

## Decisiones (aplicadas)

- [x] SSID: `SmartKiln-XXXX` (últimos 4 caracteres del MAC sin `:`)
- [x] Contraseña AP: últimos 8 caracteres del MAC
- [x] AP+STA en Fase 1 (BLE sigue hasta Fase 3)
- [x] Endpoints: status, command, profile, kiln-info, ap-info

## Prueba Fase 1 (sin app)

1. Subir firmware (`pio run -t upload`).
2. Monitor serie: buscar `[AP] SSID` y `[AP] Password`.
3. En el móvil: WiFi → red `SmartKiln-XXXX` → contraseña del log.
4. Navegador: `http://192.168.4.1/api/status`
5. Datos AP para QR: `http://192.168.4.1/api/ap-info`
6. Comando: app REST o herramienta POST `http://192.168.4.1/api/command` body `{"cmd":"START"}`

## Estructura código objetivo (firmware)

```
src/
  main.cpp
  wifi_manager.cpp   # softAP (+ STA opcional)
  kiln_api.cpp       # WebServer rutas
  display_lvgl.cpp   # sin cambios conceptuales
```

## Notas

- BLE se mantiene hasta fin de Fase 2; luego se elimina.
- No reescribir `TaskControlCurva` / MAX31855; solo nuevo transporte.
