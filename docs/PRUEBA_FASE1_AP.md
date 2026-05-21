# Prueba Fase 1 — WiFi AP + API HTTP

## Subir firmware

En PlatformIO (VS Code / Cursor) o terminal:

```bash
platformio run -e esp32-s3-devkitc-1 -t upload
platformio device monitor -e esp32-s3-devkitc-1
```

O en Windows:

```
%USERPROFILE%\.platformio\penv\Scripts\platformio.exe run -t upload -d "ruta\IA Kiln"
```

## En el monitor serie

Busca:

```
[AP] SSID: SmartKiln-XXXX
[AP] Password: xxxxxxxx
[AP] IP: 192.168.4.1
[API] Servidor HTTP en puerto 80
```

## En el teléfono

1. Ajustes → WiFi → red **SmartKiln-XXXX** (password del log).
2. Navegador → `http://192.168.4.1/api/status` → JSON con temperatura.
3. `http://192.168.4.1/api/ap-info` → SSID/password para QR futuro.

## Probar comando START

App REST Client o similar (mismo WiFi del horno):

- POST `http://192.168.4.1/api/command`
- Body: `{"cmd":"START"}`

## Notas

- BLE sigue activo (se quita en Fase 3).
- Firebase sigue si el horno tiene WiFi de casa configurado.
- Comandos por API funcionan sin internet.
