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

1. **Desactiva datos móviles (4G)**.
2. WiFi → **SmartKiln-XXXX** (password del log).
3. Si dice *Sin internet* → **Mantener conexión** / **Usar igualmente**.
4. En la **barra de direcciones** del navegador (no el buscador):
   - `http://192.168.4.1/api/status` → JSON
   - o `http://192.168.4.1/` → página con enlaces
5. Debe ser **http**, nunca **https**.

### Si dice «No se puede acceder a este sitio»

- En detalles del WiFi SmartKiln: **Puerta de enlace = 192.168.4.1**, IP del móvil `192.168.4.x` (no `169.254…`).
- **DNS privado: Desactivado** (Ajustes WiFi → SmartKiln).
- Prueba Firefox o Samsung Internet.
- Sube el firmware actualizado (mejoras AP + portal cautivo).

## Probar comando START

App REST Client o similar (mismo WiFi del horno):

- POST `http://192.168.4.1/api/command`
- Body: `{"cmd":"START"}`

## Notas

- BLE sigue activo (se quita en Fase 3).
- Firebase sigue si el horno tiene WiFi de casa configurado.
- Comandos por API funcionan sin internet.
