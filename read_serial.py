import serial
import time
import sys

try:
    print('Conectando a COM5 (115200 baud)...')
    ser = serial.Serial('COM5', 115200, timeout=2)
    print('Conectado. Leyendo logs del ESP32...')
    print('=' * 60)
    
    # Leer las últimas 50 líneas disponibles
    lines = []
    for i in range(50):
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    lines.append(line)
            else:
                time.sleep(0.1)
        except Exception as e:
            print(f"Error leyendo línea: {e}")
            break
    
    # Mostrar las líneas
    if lines:
        print('\n'.join(lines[-50:]))
    else:
        print('No se recibieron datos. Esperando nuevos datos...')
        time.sleep(3)
        for i in range(20):
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    print(line)
            time.sleep(0.1)
    
    ser.close()
    print('\n' + '=' * 60)
    print('Conexión cerrada.')
    
except serial.SerialException as e:
    print(f'Error de conexión serial: {e}')
    print('Asegúrate de que:')
    print('1. El puerto COM5 esté disponible')
    print('2. No haya otra aplicación usando el puerto')
    print('3. El ESP32 esté conectado y funcionando')
except Exception as e:
    print(f'Error: {e}')




























